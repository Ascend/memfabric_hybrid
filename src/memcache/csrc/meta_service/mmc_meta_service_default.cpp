/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "mmc_ref.h"
#include "mmc_meta_service_default.h"
#include "mmc_meta_mgr_proxy_default.h"
#include "mmc_meta_net_server.h"
#include "spdlogger4c.h"
#include "spdlogger.h"
#include "smem_store_factory.h"

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
    if (options.haEnable) {
        MMC_RETURN_ERROR(metaBackUpMgrPtr_->Start(defaultPtr), "metaBackUpMgr start failed");
    }

    metaMgrProxy_ = MmcMakeRef<MmcMetaMgrProxyDefault>(metaNetServer_).Get();
    MMC_RETURN_ERROR(metaMgrProxy_->Start(MMC_DATA_TTL_MS, options.evictThresholdHigh, options.evictThresholdLow),
                     "Failed to start meta mgr proxy of meta service " << name_);

    NetEngineOptions configStoreOpt{};
    NetEngineOptions::ExtractIpPortFromUrl(options_.configStoreURL, configStoreOpt);
    ock::smem::StoreFactory::SetLogLevel(options_.logLevel);
    ock::smem::StoreFactory::SetExternalLogFunction(SPDLOG_LogMessage);
    confStore_ = ock::smem::StoreFactory::CreateStore(configStoreOpt.ip, configStoreOpt.port, true, UN16);
    MMC_VALIDATE_RETURN(confStore_ != nullptr, "Failed to start config store server", MMC_ERROR);

    started_ = true;
    MMC_LOG_INFO("Started MetaService (" << name_ << ") at " << options_.discoveryURL);
    return MMC_OK;
}

Result MmcMetaServiceDefault::BmRegister(uint32_t rank, std::vector<uint16_t> mediaType, std::vector<uint64_t> bm,
                                         std::vector<uint64_t> capacity, std::map<std::string, MmcMemBlobDesc>& blobMap)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!started_) {
        MMC_LOG_ERROR("MetaService (" << name_ << ") is not started");
        return MMC_NOT_STARTED;
    }

    if (mediaType.size() != bm.size() || bm.size() != capacity.size()) {
        MMC_LOG_ERROR("size invalid, media size:" << mediaType.size() << ", bm size:" << bm.size()
                                                  << ", capacity size:" << capacity.size());
        return MMC_INVALID_PARAM;
    }

    std::vector<MmcLocation> locs;
    std::vector<MmcLocalMemlInitInfo> infos;
    size_t typeNum = mediaType.size();
    for (size_t i = 0; i < typeNum; i++) {
        locs.emplace_back(rank, static_cast<MediaType>(mediaType[i]));
        MmcLocalMemlInitInfo locInfo{bm[i], capacity[i]};
        infos.emplace_back(locInfo);
    }
    MMC_RETURN_ERROR(metaBackUpMgrPtr_->Load(blobMap), "Mount loc { " << rank << " } load backup failed");
    MMC_RETURN_ERROR(metaMgrProxy_->Mount(locs, infos, blobMap), "Mount loc { " << rank << " } failed");
    MMC_LOG_INFO("Mount loc {rank:" << rank << ", rebuild size:" << blobMap.size() << ", mediaNum:" << typeNum
                                    << "} finish");
    if (blobMap.size() == 0) {
        if (rankMediaTypeMap_.find(rank) == rankMediaTypeMap_.end()) {
            rankMediaTypeMap_.insert({rank, {}});
        }

        for (size_t i = 0; i < typeNum; i++) {
            rankMediaTypeMap_[rank].insert(mediaType[i]);
        }
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

}  // namespace mmc
}  // namespace ock