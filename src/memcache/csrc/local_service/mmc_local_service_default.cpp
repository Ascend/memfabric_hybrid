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
    options_ = config;
    metaNetClient_ = MetaNetClientFactory::GetInstance(this->options_.discoveryURL, "MetaClientCommon").Get();
    MMC_ASSERT_RETURN(metaNetClient_.Get() != nullptr, MMC_NEW_OBJECT_FAILED);
    if (!metaNetClient_->Status()) {
        MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetClient_->Start(),
                                        "Failed to start net server of local service " << name_);
        MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetClient_->Connect(config.discoveryURL),
                                        "Failed to connect net server of local service " << name_);
    }
    pid_ = getpid();

    MMC_LOG_ERROR_AND_RETURN_NOT_OK(InitBm(config), "Failed to init bm of local service " << name_);

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
    metaNetClient_->Stop();
    MmcBmProxy::GetInstance().DestoryBm();
    MMC_LOG_INFO("Stop MmcClientDefault (" << name_ << ") server " << options_.discoveryURL);
    started_ = false;
}

Result MmcLocalServiceDefault::InitBm(const mmc_local_service_config_t &config)
{
    mmc_bm_init_config_t initConfig = {config.deviceId, config.rankId, config.worldSize,
                                       config.bmIpPort, config.autoRanking};
    mmc_bm_create_config_t createConfig = {config.createId, config.worldSize, config.dataOpType,
                                           config.localDRAMSize, config.localHBMSize, config.flags};

    MmcBmProxy& bmProxy = MmcBmProxy::GetInstance();
    Result ret = bmProxy.InitBm(initConfig, createConfig);
    if (ret != MMC_OK) {
        return ret;
    }

    BmRegisterRequest req;
    req.rank_ = initConfig.rankId;
    req.mediaType_ = (uint16_t)(createConfig.localHBMSize == 0);
    req.bm_ = bmProxy.GetGva();
    req.capacity_ = createConfig.localDRAMSize + createConfig.localHBMSize;

    Response resp;
    int16_t respRet;

    return SyncCallMeta(req, resp, respRet, 30);
}
}
}