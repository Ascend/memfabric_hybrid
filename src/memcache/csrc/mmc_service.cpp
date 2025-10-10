/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "mmc_service.h"
#include "mmc_common_includes.h"
#include "mmc_meta_service_default.h"
#include "mmc_local_service_default.h"

using namespace ock::mmc;

MMC_API mmc_meta_service_t mmcs_meta_service_start(mmc_meta_service_config_t *config)
{
    MMC_VALIDATE_RETURN(config != nullptr, "invalid param, config is nullptr", nullptr);
    auto *serviceDefault = new (std::nothrow) MmcMetaServiceDefault("meta_service");
    if (serviceDefault == nullptr) {
        MMC_LOG_AND_SET_LAST_ERROR("create or start meta service failed");
        return nullptr;
    }
    if (serviceDefault->Start(*config) == MMC_OK) {
        return serviceDefault;
    }
    delete serviceDefault;
    MMC_LOG_AND_SET_LAST_ERROR("create or start meta service failed");
    return nullptr;
}

MMC_API void mmcs_meta_service_stop(mmc_meta_service_t handle)
{
    MMC_VALIDATE_RETURN_VOID(handle != nullptr, "invalid param, handle is nullptr");
    static_cast<MmcMetaServiceDefault *>(handle)->Stop();
}

MMC_API mmc_local_service_t mmcs_local_service_start(mmc_local_service_config_t *config)
{
    MMC_VALIDATE_RETURN(config != nullptr, "invalid param, config is nullptr", nullptr);
    auto *serviceDefault = new (std::nothrow) MmcLocalServiceDefault("local_service");
    if (serviceDefault == nullptr) {
        MMC_LOG_AND_SET_LAST_ERROR("create or start local service failed");
        return nullptr;
    }
    if (serviceDefault->Start(*config) == MMC_OK) {
        return serviceDefault;
    }
    delete serviceDefault;
    MMC_LOG_AND_SET_LAST_ERROR("create or start local service failed");
    return nullptr;
}

MMC_API void mmcs_local_service_stop(mmc_local_service_t handle)
{
    MMC_VALIDATE_RETURN_VOID(handle != nullptr, "invalid param, handle is nullptr");
    auto service_default = static_cast<MmcLocalServiceDefault *>(handle);
    if (service_default != nullptr) {
        service_default->Stop();
        delete service_default;
        service_default = nullptr;
    }
}