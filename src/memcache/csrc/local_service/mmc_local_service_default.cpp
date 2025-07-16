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
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(InitBm(config), "Failed to init bm of local service " << name_);

    options_ = config;
    metaNetClient_ = MetaNetClientFactory::GetInstance(this->options_.discoveryURL, "MetaClientCommon").Get();
    MMC_ASSERT_RETURN(metaNetClient_.Get() != nullptr, MMC_NEW_OBJECT_FAILED);
    if (!metaNetClient_->Status()) {
        MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetClient_->Start(config.rankId),
            "Failed to start net server of local service, name=" << name_ << ", bmRankId=" << config.rankId);
        MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetClient_->Connect(config.discoveryURL),
            "Failed to connect net server of local service, name=" << name_ << ", bmRankId=" << config.rankId);
    }
    pid_ = getpid();

    BmRegisterRequest req;
    req.rank_ = config.rankId;
    req.mediaType_ = static_cast<uint16_t>(config.localHBMSize == 0);
    req.addr_ = bmProxyPtr_->GetGva();
    req.capacity_ = config.localDRAMSize + config.localHBMSize;

    Response resp;
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(SyncCallMeta(req, resp, 30), "bm register failed, bmRankId=" << req.rank_);
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(resp.ret_,
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

Result MmcLocalServiceDefault::InitBm(const mmc_local_service_config_t &config)
{
    mmc_bm_init_config_t initConfig = {config.deviceId, config.rankId, config.worldSize,
                                       config.bmIpPort, config.autoRanking};
    mmc_bm_create_config_t createConfig = {config.createId, config.worldSize, config.dataOpType,
                                           config.localDRAMSize, config.localHBMSize, config.flags};

    MmcBmProxyPtr bmProxy = MmcBmProxyFactory::GetInstance("bmProxyDefault");
    Result ret = bmProxy->InitBm(initConfig, createConfig);
    if (ret != MMC_OK) {
        return ret;
    }
    if (config.autoRanking == 1) {
        config.rankId = initConfig.rankId;
    }
    bmProxyPtr_ = bmProxy;
    return ret;
}

Result MmcLocalServiceDefault::DestroyBm()
{
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(bmProxyPtr_ == nullptr, "bm proxy has not been initialized.");
    bmProxyPtr_->DestoryBm();

    BmUnregisterRequest req;
    req.rank_ = options_.rankId;
    req.mediaType_ = (uint16_t)(options_.localHBMSize == 0);

    Response resp;

    Result ret = SyncCallMeta(req, resp, 30);
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(ret, "bm destroy failed!");
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(resp.ret_, "bm destroy failed!");
    bmProxyPtr_ = nullptr;
    return ret;
}
}
}