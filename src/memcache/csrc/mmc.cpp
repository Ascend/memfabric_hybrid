/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "mmc.h"

#include "mmc_client.h"
#include "mmc_common_includes.h"
#include "mmc_configuration.h"
#include "mmc_def.h"
#include "mmc_env.h"
#include "mmc_service.h"
#include "mmc_ptracer.h"

using namespace ock::mmc;
static ClientConfig *g_clientConfig;
static mmc_local_service_t g_localService;

static std::mutex gMmcMutex;
static bool mmcInit = false;

MMC_API int32_t mmc_init(const mmc_init_config &config)
{
    MMC_VALIDATE_RETURN(config.deviceId <= MAX_DEVICE_ID, "Invalid param deviceId: "
                        << config.deviceId, MMC_INVALID_PARAM);
    std::lock_guard<std::mutex> lock(gMmcMutex);
    if (mmcInit) {
        MMC_LOG_INFO("mmc is already init");
        return MMC_OK;
    }

    MMC_VALIDATE_RETURN(MMC_LOCAL_CONF_PATH != nullptr, "MMC_LOCAL_CONFIG_PATH is not set", MMC_INVALID_PARAM);

    g_clientConfig = new ClientConfig();
    MMC_VALIDATE_RETURN(g_clientConfig != nullptr, "Failed to initialize client config", MMC_ERROR);

    MMC_VALIDATE_RETURN(g_clientConfig->LoadFromFile(MMC_LOCAL_CONF_PATH), "Failed to load client config", MMC_ERROR);

    const std::vector<std::string> validationError = g_clientConfig->ValidateConf();
    if (!validationError.empty()) {
        MMC_LOG_ERROR("Wrong configuration in file <" << std::string(MMC_LOCAL_CONF_PATH)
            << ">, because of following mistakes:");
        for (auto &item : validationError) {
            MMC_LOG_ERROR(item);
        }
        return MMC_INVALID_PARAM;
    }

    mmc_local_service_config_t localServiceConfig{};
    localServiceConfig.flags = 0;
    localServiceConfig.deviceId = config.deviceId;
    g_clientConfig->GetLocalServiceConfig(localServiceConfig);
    MMC_RETURN_ERROR(ock::mmc::MmcOutLogger::Instance().SetLogLevel(static_cast<LogLevel>(localServiceConfig.logLevel)),
                     "failed to set log level " << localServiceConfig.logLevel);
    if (localServiceConfig.logFunc != nullptr) {
        MmcOutLogger::Instance().SetExternalLogFunction(localServiceConfig.logFunc);
    }

    MMC_VALIDATE_RETURN(g_clientConfig->ValidateLocalServiceConfig(localServiceConfig) == MMC_OK,
        "Invalid local service config", MMC_INVALID_PARAM);
    MMC_VALIDATE_RETURN(g_clientConfig->ValidateTLSConfig(localServiceConfig.tlsConfig) == MMC_OK,
        "Invalid TLS config", MMC_INVALID_PARAM);
    g_localService = mmcs_local_service_start(&localServiceConfig);
    MMC_VALIDATE_RETURN(g_localService != nullptr, "failed to create or start local service", MMC_ERROR);

    mmc_client_config_t clientConfig{};
    g_clientConfig->GetClientConfig(clientConfig);
    auto ret = mmcc_init(&clientConfig);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("mmcc init failed, ret:" << ret);
        // todo 回滚资源
        return ret;
    }
    mmcInit = true;
    return ret;
}

MMC_API int32_t mmc_set_extern_logger(void (*func)(int level, const char *msg))
{
    ock::mmc::MmcOutLogger::Instance().SetExternalLogFunction(func);
    return MMC_OK;
}

MMC_API int32_t mmc_set_log_level(int level)
{
    ock::mmc::MmcOutLogger::Instance().SetLogLevel(static_cast<LogLevel>(level));
    return MMC_OK;
}

MMC_API void mmc_uninit()
{
    std::lock_guard<std::mutex> lock(gMmcMutex);
    if (!mmcInit) {
        MMC_LOG_INFO("mmc is not init");
        return;
    }

    if (g_localService != nullptr) {
        mmcs_local_service_stop(g_localService);
        g_localService = nullptr;
    }
    mmcc_uninit();
    mmcInit = false;

    if (g_clientConfig != nullptr) {
        delete g_clientConfig;
        g_clientConfig = nullptr;
    }
}

MMC_API const char *mmc_get_last_err_msg()
{
    return NULL;
}

MMC_API const char *mmc_get_and_clear_last_err_msg()
{
    return NULL;
}