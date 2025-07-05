/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc_meta_service_default.h"
#include "mmc_meta_net_server.h"
#include "mmc_meta_mgr_proxy_impl.h"

namespace ock {
namespace mmc {
Result MmcMetaServiceDefault::Start(const mmc_meta_service_config_t &options)
{
    MMC_LOG_INFO("Starting meta service " << name_);
    std::lock_guard<std::mutex> guard(mutex_);
    if (started_) {
        MMC_LOG_INFO("MetaService " << name_ << " already started");
        return MMC_OK;
    }
    options_ = options;
    worldSize_ = options_.worldSize;
    metaNetServer_ = MmcMakeRef<MetaNetServer>(this, name_ + "_MetaServer").Get();
    MMC_ASSERT_RETURN(metaNetServer_.Get() != nullptr, MMC_NEW_OBJECT_FAILED);
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetServer_->Start(), "Failed to start net server of meta service " << name_);

    started_ = true;
    MMC_LOG_INFO("Started MetaService (" << name_ << ") at " << options_.discoveryURL);
    return MMC_OK;
}

MmcMetaMgrProxyPtr MmcMetaServiceDefault::GetMetaMgrProxy() const {
    return metaMgrProxy_;
}

Result MmcMetaServiceDefault::BmRegister(uint32_t rank, uint16_t mediaType, uint64_t bm, uint64_t capacity)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!started_) {
        MMC_LOG_ERROR("MetaService (" << name_ << ") is not started");
        return MMC_NOT_STARTED;
    }

    MmcLocation loc{rank, mediaType};
    MmcLocalMemlInitInfo locInfo{bm, capacity};
    poolInitInfo_[loc] = locInfo;
    registerRank_++;
    if (registerRank_ == worldSize_) {
        uint64_t defaultTtl = 2000;
        metaMgrProxy_ = MmcMakeRef<MmcMetaMgrProxyImpl>(poolInitInfo_, defaultTtl).Get();
        bmStart_ = true;
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
    metaNetServer_->Stop();
    MMC_LOG_INFO("Stop MmcMetaServiceDefault (" << name_ << ") at " << options_.discoveryURL);
    started_ = false;
}
}
}