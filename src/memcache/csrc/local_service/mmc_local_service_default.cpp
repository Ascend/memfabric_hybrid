/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc_local_service_default.h"
#include "mmc_meta_net_client.h"
#include "mmc_msg_client_meta.h"
#include "mmc_bm_proxy.h"


namespace ock {
namespace mmc {
MmcLocalServiceDefault::~MmcLocalServiceDefault() {}
Result MmcLocalServiceDefault::Start(const mmc_local_service_config_t &config)
{
    MMC_LOG_INFO("Starting meta service " << name_);
    std::lock_guard<std::mutex> guard(mutex_);
    if (started_) {
        MMC_LOG_INFO("MetaService " << name_ << " already started");
        return MMC_OK;
    }

    // 初始化BM，并更新bmRankId
    options_ = config;
    MMC_RETURN_ERROR(ock::mmc::MmcOutLogger::Instance().SetLogLevel(static_cast<LogLevel>(options_.logLevel)),
                     "failed to set log level " << options_.logLevel);
    if (options_.logFunc != nullptr) {
        ock::mmc::MmcOutLogger::Instance().SetExternalLogFunction(options_.logFunc);
    }
    MMC_RETURN_ERROR(InitBm(), "Failed to init bm of local service " << name_);

    metaNetClient_ = MetaNetClientFactory::GetInstance(this->options_.discoveryURL, "MetaClientCommon").Get();
    MMC_ASSERT_RETURN(metaNetClient_.Get() != nullptr, MMC_NEW_OBJECT_FAILED);
    if (!metaNetClient_->Status()) {
        mmc_client_config_t clientConfig;
        clientConfig.rankId = options_.rankId;
        clientConfig.tlsConfig = options_.tlsConfig;
        NetEngineOptions options;
        options.name = name_;
        options.threadCount = 2;
        options.rankId = options_.rankId;
        options.startListener = false;
        options.tlsOption = options_.tlsConfig;
        options.logLevel = options_.logLevel;
        options.logFunc = options_.logFunc;
        MMC_RETURN_ERROR(metaNetClient_->Start(options),
                         "Failed to start net server of local service, name=" << name_ << ", bmRankId=" << options_.rankId);
        MMC_RETURN_ERROR(metaNetClient_->Connect(options_.discoveryURL),
                         "Failed to connect net server of local service, name=" << name_ << ", bmRankId=" << options_.rankId);
    }
    pid_ = getpid();

    MMC_RETURN_ERROR(RegisterBm(),  "Failed to register bm, name=" << name_ << ", bmRankId=" << options_.rankId);
    metaNetClient_->RegisterRetryHandler(
        std::bind(&MmcLocalServiceDefault::RegisterBm, this),
        std::bind(&MmcLocalServiceDefault::UpdateMetaBackup, this,
                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
        std::bind(&MmcLocalServiceDefault::CopyBlob, this, std::placeholders::_1, std::placeholders::_2)
    );
    started_ = true;
    MMC_LOG_INFO("Started LocalService (" << name_ << ") server " << options_.discoveryURL);
    return MMC_OK;
}

void MmcLocalServiceDefault::Stop()
{    std::lock_guard<std::mutex> guard(mutex_);
    if (!started_) {
        MMC_LOG_WARN("MmcClientDefault has not been started");
        return;
    }
    DestroyBm();
    metaNetClient_->Stop();
    std::lock_guard<std::mutex> guardBlob(blobMutex_);
    blobMap_.clear();
    MMC_LOG_INFO("Stop MmcClientDefault (" << name_ << ") server " << options_.discoveryURL);
    started_ = false;
}

Result MmcLocalServiceDefault::InitBm()
{
    mmc_bm_init_config_t initConfig = {options_.deviceId, options_.worldSize, options_.bmIpPort,
                                       options_.bmHcomUrl, options_.logLevel, options_.logFunc};
    mmc_bm_create_config_t createConfig = {options_.createId, options_.worldSize, options_.dataOpType,
                                           options_.localDRAMSize, options_.localHBMSize, options_.flags};

    MmcBmProxyPtr bmProxy = MmcBmProxyFactory::GetInstance("bmProxyDefault");
    Result ret = bmProxy->InitBm(initConfig, createConfig);
    if (ret != MMC_OK) {
        return ret;
    }
    options_.rankId = bmProxy->RankId();
    bmProxyPtr_ = bmProxy;
    return ret;
}

Result MmcLocalServiceDefault::DestroyBm()
{
    MMC_RETURN_ERROR(bmProxyPtr_ == nullptr, "bm proxy has not been initialized.");
    bmProxyPtr_->DestroyBm();

    BmUnregisterRequest req;
    req.rank_ = options_.rankId;
    for (MediaType type = MEDIA_DRAM; type != MEDIA_NONE;) {
        if (bmProxyPtr_->GetGva(type) != 0) {
            req.mediaType_.emplace_back(type);
        }
        type = MoveUp(type);
    }
    Response resp;
    Result ret = SyncCallMeta(req, resp, 30);
    MMC_RETURN_ERROR(ret, "bm destroy failed!");
    MMC_RETURN_ERROR(resp.ret_, "bm destroy failed!");
    bmProxyPtr_ = nullptr;
    return ret;
}

Result MmcLocalServiceDefault::RegisterBm()
{
    BmRegisterRequest req;
    req.rank_ = options_.rankId;
    for (MediaType type = MEDIA_DRAM; type != MEDIA_NONE;) {
        uint64_t gva = bmProxyPtr_->GetGva(type);
        if (gva != 0) {
            req.addr_.emplace_back(gva);
            req.mediaType_.emplace_back(type);
            req.capacity_.emplace_back(bmProxyPtr_->GetCapacity(type));
            MMC_LOG_INFO("mmc local register capacity:" << req.capacity_.back() << ", type:" << req.mediaType_.back());
        }
        type = MoveUp(type);
    }
    req.blobMap_.clear();

    Response resp;
    auto chunk_start = blobMap_.begin();
    const auto end = blobMap_.end();

    while (chunk_start != end) {
        auto chunk_end = chunk_start;
        std::advance(chunk_end, std::min(blobRebuildSendMaxCount,
                                         static_cast<int>(std::distance(chunk_start, end))));

        req.blobMap_.insert(chunk_start, chunk_end);
        MMC_LOG_INFO("mmc meta blob rebuild count " << req.blobMap_.size());
        MMC_RETURN_ERROR(SyncCallMeta(req, resp, 30), "bm register failed, bmRankId=" << req.rank_);
        MMC_RETURN_ERROR(resp.ret_,
                         "bm register failed, bmRankId=" << req.rank_ << ", retCode=" << resp.ret_);
        chunk_start = chunk_end;
        req.blobMap_.clear();
    }
    MMC_RETURN_ERROR(SyncCallMeta(req, resp, 30), "bm register failed, bmRankId=" << req.rank_);
    MMC_RETURN_ERROR(resp.ret_,
                     "bm register failed, bmRankId=" << req.rank_ << ", retCode=" << resp.ret_);
    MMC_LOG_INFO("bm register succeed, bmRankId=" << req.rank_ << ", type num=" << req.mediaType_.size());
    return MMC_OK;
}

Result MmcLocalServiceDefault::UpdateMetaBackup(
    const std::vector<uint32_t> &ops, const std::vector<std::string> &keys, const std::vector<MmcMemBlobDesc> &blobs)
{
    std::lock_guard<std::mutex> guard(blobMutex_);

    const auto opCount = ops.size();
    const auto keyCount = keys.size();
    const auto blobCount = blobs.size();
    auto length = keyCount;
    // 检查keys和blobs大小是否一致，避免越界访问
    if (keyCount != blobCount) {
        MMC_LOG_ERROR("Local service replicate warning, length is not equal: opSize=" << opCount
            << ", keySize=" << keyCount << ", blobSize=" << blobCount);
        length = std::min({opCount, keyCount, blobCount});
    }

    for (size_t i = 0; i < length; i++) {
        if (ops[i] == 0) {
            auto result = blobMap_.emplace(keys[i], blobs[i]);
            if (!result.second) {
                MMC_LOG_WARN("Local service replicate fail, key: " << keys[i] << " already exists");
            }
        } else if (ops[i] == 1) {
            blobMap_.erase(keys[i]);
        } else {
            // 永远走不到这里
            MMC_LOG_ERROR("UpdateMetaBackup error, key: " << keys[i]);
        }
    }
    return MMC_OK;
}

Result MmcLocalServiceDefault::CopyBlob(const MmcMemBlobDesc& src, const MmcMemBlobDesc& dst)
{
    MmcBmProxyPtr bmProxy = MmcBmProxyFactory::GetInstance("bmProxyDefault");
    if (bmProxy == nullptr) {
        MMC_LOG_ERROR("bm proxy is null, src=" << src << ", dst=" << dst);
        return MMC_ERROR;
    }

    auto ret = bmProxy->Put(src.gva_, dst.gva_, dst.size_, SMEMB_COPY_G2G);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("bm put failed:" << ret << ", src=" << src << ", dst=" << dst);
        return MMC_ERROR;
    }
    return MMC_OK;
}
}
}