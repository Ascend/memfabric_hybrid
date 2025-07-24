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
    MMC_RETURN_ERROR(InitBm(), "Failed to init bm of local service " << name_);

    metaNetClient_ = MetaNetClientFactory::GetInstance(this->options_.discoveryURL, "MetaClientCommon").Get();
    MMC_ASSERT_RETURN(metaNetClient_.Get() != nullptr, MMC_NEW_OBJECT_FAILED);
    if (!metaNetClient_->Status()) {
        mmc_client_config_t clientConfig;
        clientConfig.rankId = options_.rankId;
        clientConfig.tlsConfig = options_.tlsConfig;
        MMC_RETURN_ERROR(metaNetClient_->Start(clientConfig),
                         "Failed to start net server of local service, name=" << name_ << ", bmRankId=" << options_.rankId);
        MMC_RETURN_ERROR(metaNetClient_->Connect(options_.discoveryURL),
                         "Failed to connect net server of local service, name=" << name_ << ", bmRankId=" << options_.rankId);
    }
    pid_ = getpid();

    BmRegisterRequest req;
    req.rank_ = options_.rankId;
    req.mediaType_ = static_cast<uint16_t>(options_.localHBMSize == 0);
    req.addr_ = bmProxyPtr_->GetGva();
    req.capacity_ = options_.localDRAMSize + options_.localHBMSize;

    Response resp;
    MMC_RETURN_ERROR(SyncCallMeta(req, resp, 30), "bm register failed, bmRankId=" << req.rank_);
    MMC_RETURN_ERROR(resp.ret_,
                     "bm register failed, bmRankId=" << req.rank_ << ", retCode=" << resp.ret_);

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
    MMC_LOG_INFO("Stop MmcClientDefault (" << name_ << ") server " << options_.discoveryURL);
    started_ = false;
}

Result MmcLocalServiceDefault::InitBm()
{
    mmc_bm_init_config_t initConfig = {options_.deviceId, options_.rankId, options_.worldSize,
                                       options_.bmIpPort, options_.autoRanking};
    mmc_bm_create_config_t createConfig = {options_.createId, options_.worldSize, options_.dataOpType,
                                           options_.localDRAMSize, options_.localHBMSize, options_.flags};

    MmcBmProxyPtr bmProxy = MmcBmProxyFactory::GetInstance("bmProxyDefault");
    Result ret = bmProxy->InitBm(initConfig, createConfig);
    if (ret != MMC_OK) {
        return ret;
    }
    if (options_.autoRanking == 1) {
        options_.rankId = bmProxy->RandId();
    }
    bmProxyPtr_ = bmProxy;
    return ret;
}

Result MmcLocalServiceDefault::DestroyBm()
{
    MMC_RETURN_ERROR(bmProxyPtr_ == nullptr, "bm proxy has not been initialized.");
    bmProxyPtr_->DestoryBm();

    BmUnregisterRequest req;
    req.rank_ = options_.rankId;
    req.mediaType_ = (uint16_t)(options_.localHBMSize == 0);

    Response resp;

    Result ret = SyncCallMeta(req, resp, 30);
    MMC_RETURN_ERROR(ret, "bm destroy failed!");
    MMC_RETURN_ERROR(resp.ret_, "bm destroy failed!");
    bmProxyPtr_ = nullptr;
    return ret;
}
}
}