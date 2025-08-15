/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc_ref.h"
#include "mmc_meta_service_default.h"
#include "mmc_meta_mgr_proxy_default.h"
#include "mmc_meta_net_server.h"
#include "spdlogger4c.h"
#include "spdlogger.h"

namespace ock {
namespace mmc {
Result MmcMetaServiceDefault::Start(const mmc_meta_service_config_t &options)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (started_) {
        MMC_LOG_INFO("MetaService " << name_ << " already started");
        return MMC_OK;
    }
    options_ = options;
    std::string logPath;
    std::string logAuditPath;
    MMC_RETURN_ERROR(GetLogPath(logPath, logAuditPath), "failed to get log path");
    MMC_LOG_INFO("Starting meta service " << name_ << ", log path: " << logPath << " log level: " << options.logLevel
                 << " log rotation file size: " << options.logRotationFileSize
                 << " log rotation file count: " << options.logRotationFileCount);
    MMC_RETURN_ERROR(ock::mmc::MmcOutLogger::Instance().SetLogLevel(static_cast<LogLevel>(options.logLevel)),
                     "failed to set log level " << options.logLevel);
    MMC_RETURN_ERROR(SPDLOG_Init(logPath.c_str(), options.logLevel, options.logRotationFileSize,
        options.logRotationFileCount), "failed to init spdlog, error: " << SPDLOG_GetLastErrorMessage());

    ock::mmc::MmcOutLogger::Instance().SetExternalLogFunction(SPDLOG_LogMessage);
    MMC_RETURN_ERROR(SPDLOG_AuditInit(logAuditPath.c_str(), OBJ_MAX_LOG_FILE_SIZE, OBJ_MAX_LOG_FILE_NUM),
                     "failed to init spdlog, error: " << SPDLOG_GetLastErrorMessage());

    MMC_VALIDATE_RETURN(options.evictThresholdHigh > options.evictThresholdLow,
        "invalid param, evictThresholdHigh must large than evictThresholdLow", MMC_INVALID_PARAM);

    metaNetServer_ = MmcMakeRef<MetaNetServer>(this, name_ + "_MetaServer").Get();
    MMC_ASSERT_RETURN(metaNetServer_.Get() != nullptr, MMC_NEW_OBJECT_FAILED);
    /* init engine */
    NetEngineOptions netOptions;
    std::string url{options_.discoveryURL};
    NetEngineOptions::ExtractIpPortFromUrl(url, netOptions);
    netOptions.name = name_;
    netOptions.threadCount = 4;
    netOptions.rankId = 0;
    netOptions.startListener = true;
    netOptions.tlsOption = options_.tlsConfig;
    netOptions.logFunc = SPDLOG_LogMessage;
    netOptions.logLevel = options_.logLevel;
    MMC_RETURN_ERROR(metaNetServer_->Start(netOptions), "Failed to start net server of meta service " << name_);

    metaBackUpMgrPtr_ = MMCMetaBackUpMgrFactory::GetInstance("DefaultMetaBackup");
    MMCMetaBackUpConfPtr defaultPtr = MmcMakeRef<MMCMetaBackUpConfDefault>(metaNetServer_).Get();
    MMC_ASSERT_RETURN(metaBackUpMgrPtr_ != nullptr, MMC_MALLOC_FAILED);
    if (options.metaRebuildEnable) {
        MMC_RETURN_ERROR(metaBackUpMgrPtr_->Start(defaultPtr), "metaBackUpMgr start failed");
    }

    metaMgrProxy_ = MmcMakeRef<MmcMetaMgrProxyDefault>(metaNetServer_).Get();
    MMC_RETURN_ERROR(metaMgrProxy_->Start(MMC_DATA_TTL_MS, options.evictThresholdHigh, options.evictThresholdLow),
                     "Failed to start meta mgr proxy of meta service " << name_);

    started_ = true;
    MMC_LOG_INFO("Started MetaService (" << name_ << ") at " << options_.discoveryURL);
    return MMC_OK;
}

Result MmcMetaServiceDefault::BmRegister(uint32_t rank, uint16_t mediaType, uint64_t bm, uint64_t capacity,
    std::map<std::string, MmcMemBlobDesc> &blobMap)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!started_) {
        MMC_LOG_ERROR("MetaService (" << name_ << ") is not started");
        return MMC_NOT_STARTED;
    }

    MmcLocation loc{rank, static_cast<MediaType>(mediaType)};
    MmcLocalMemlInitInfo locInfo{bm, capacity};
    MMC_RETURN_ERROR(metaMgrProxy_->Mount(loc, locInfo, blobMap), "Mount loc { " << rank << ", " << mediaType << " } failed");
    MMC_LOG_INFO("Mount loc {rank:" << rank << ", media:" << mediaType << ", cap:" << capacity << "} finish");
    if (blobMap.size() == 0) {
        registerRank_++;
        if (rankMediaTypeMap_.find(rank) == rankMediaTypeMap_.end()) {
            rankMediaTypeMap_.insert({rank, {}});
        }
        rankMediaTypeMap_[rank].insert(mediaType);
    }
    return MMC_OK;
}

Result MmcMetaServiceDefault::BmUnregister(uint32_t rank, uint16_t mediaType)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!started_) {
        MMC_LOG_ERROR("MetaService (" << name_ << ") is not started");
        return MMC_NOT_STARTED;
    }

    MmcLocation loc{rank, static_cast<MediaType>(mediaType)};
    MMC_RETURN_ERROR(metaMgrProxy_->Unmount(loc), "Unmount loc { " << rank << ", " << mediaType << " } failed");
    MMC_LOG_INFO("Unmount loc { " << rank << ", " << mediaType << " } finish");
    --registerRank_;
    if (rankMediaTypeMap_.find(rank) != rankMediaTypeMap_.end() &&
        rankMediaTypeMap_[rank].find(mediaType) != rankMediaTypeMap_[rank].end()) {
        rankMediaTypeMap_[rank].erase(mediaType);
    }
    if (rankMediaTypeMap_.find(rank) != rankMediaTypeMap_.end() &&
        rankMediaTypeMap_[rank].empty()) {
        rankMediaTypeMap_.erase(rank);
    }
    return MMC_OK;
}

Result MmcMetaServiceDefault::ClearResource(uint32_t rank) {
    if (!started_) {
        MMC_LOG_ERROR("MetaService (" << name_ << ") is not started.");
        return MMC_NOT_STARTED;
    }
    std::unordered_set<uint16_t> mediaTypes;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (rankMediaTypeMap_.find(rank) == rankMediaTypeMap_.end()) {
            MMC_LOG_INFO("Rank " << rank << " has no resources.");
            return MMC_OK;
        }
        mediaTypes = rankMediaTypeMap_[rank];
    }

    for (const auto& mediaType : mediaTypes) {
        MMC_LOG_INFO("Clear resource {rank, mediaType} -> { " << rank << ", " << mediaType << " }");
        BmUnregister(rank, mediaType);
    }
    return MMC_OK;
}

void MmcMetaServiceDefault::Stop()
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!started_) {
        MMC_LOG_WARN("MmcClientDefault has not been started");
        return;
    }
    metaBackUpMgrPtr_->Stop();
    metaMgrProxy_->Stop();
    metaNetServer_->Stop();
    MMC_LOG_INFO("Stop MmcMetaServiceDefault (" << name_ << ") at " << options_.discoveryURL);
    started_ = false;
}

Result MmcMetaServiceDefault::GetLogPath(std::string& logPath, std::string& logAuditPath)
{
    char pathBuf[PATH_MAX] = {0};
    ssize_t count = readlink("/proc/self/exe", pathBuf, PATH_MAX);
    if (count == -1) {
        MMC_LOG_ERROR("mmc meta service not found bin path");
    }
    pathBuf[count] = '\0';
    std::string binPath = pathBuf;
    binPath = binPath.substr(0, binPath.find_last_of('/'));
    logPath = binPath + "/../logs/mmc-meta.log";
    logAuditPath = binPath + "/../logs/mmc-meta-audit.log";
    return MMC_OK;
}
}  // namespace mmc
}  // namespace ock