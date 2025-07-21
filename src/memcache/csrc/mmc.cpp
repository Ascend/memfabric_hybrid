/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc.h"

#include "mmc_client.h"
#include "mmc_common_includes.h"
#include "mmc_configuration.h"
#include "mmc_def.h"
#include "mmc_env.h"
#include "mmc_service.h"

using namespace ock::mmc;
static ClientConfig *g_clientConfig;
static mmc_local_service_t g_localService;

MMC_API int32_t mmc_init()
{
    MMC_VALIDATE_RETURN(MMC_CONFIG_PATH != nullptr, "MEMCACHE_CONFIG_PATH is not set", MMC_INVALID_PARAM);

    g_clientConfig = new ClientConfig();
    MMC_VALIDATE_RETURN(g_clientConfig != nullptr, "Failed to initialize client config", MMC_ERROR);

    MMC_VALIDATE_RETURN(g_clientConfig->LoadFromFile(MMC_CONFIG_PATH), "Failed to load client config", MMC_ERROR);

    const std::vector<std::string> validationError = g_clientConfig->ValidateConf();
    if (!validationError.empty()) {
        MMC_LOG_ERROR("Wrong configuration in file <" << std::string(MMC_CONFIG_PATH) << ">, because of following mistakes:");
        for (auto &item : validationError) {
            MMC_LOG_ERROR(item);
        }
        return MMC_INVALID_PARAM;
    }

    mmc_local_service_config_t localServiceConfig;
    g_clientConfig->GetLocalServiceConfig(localServiceConfig);
    g_localService = mmcs_local_service_start(&localServiceConfig);
    MMC_VALIDATE_RETURN(g_localService != nullptr, "failed to create or start local service", MMC_ERROR);

    mmc_client_config_t clientConfig;
    g_clientConfig->GetClientConfig(clientConfig);
    return mmcc_init(&clientConfig);
}

MMC_API int32_t mmc_set_extern_logger(void (*func)(int level, const char *msg))
{
    return MMC_OK;
}

MMC_API int32_t mmc_set_log_level(int level)
{
    return MMC_OK;
}

MMC_API void mmc_uninit()
{
    if (g_localService != nullptr) {
        mmcs_local_service_stop(g_localService);
        g_localService = nullptr;
    }
    mmcc_uninit();
}

MMC_API const char *mmc_get_last_err_msg()
{
    return NULL;
}

MMC_API const char *mmc_get_and_clear_last_err_msg()
{
    return NULL;
}