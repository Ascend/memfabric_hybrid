/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "smem.h"
#include "smem_common_includes.h"
#include "hybm_core_api.h"
#include "acc_links/net/acc_log.h"

namespace {
bool g_smemInited = false;
}

int32_t smem_init(uint32_t flags)
{
    using namespace ock::smem;

    /* create logger instance */
    SMOutLogger::Instance();

    /* get hybm core env */
    std::string path;
    auto corePath = std::getenv("SMEM_SHM_HYBM_CORE_PATH");
    if (corePath != nullptr) {
        path = corePath;
    } else {
        SM_LOG_INFO("env SMEM_SHM_HYBM_CORE_PATH is not set, use default lib path.");
    }

    /* load libraries in under_api */
    auto result = HybmCoreApi::LoadLibrary(path);
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("smem init failed as load library failed, result: " << result);
        return result;
    }
    g_smemInited = true;

    // 未init时不能给hybm更新log配置,所以在init后延迟更新
    auto func = SMOutLogger::Instance().GetLogExtraFunc();
    if (func != nullptr) {
        (void)smem_set_extern_logger(func);
    }
    (void)smem_set_log_level(SMOutLogger::Instance().GetLogLevel());

    return SM_OK;
}

void smem_uninit()
{
    g_smemInited = false;
    return;
}

int32_t smem_set_extern_logger(void (*fun)(int, const char *))
{
    using namespace ock::smem;

    SM_PARAM_VALIDATE(fun == nullptr, "set extern logger failed, invalid func which is NULL", SM_INVALID_PARAM);

    /* set my out logger */
    SMOutLogger::Instance().SetExternalLogFunction(fun, true);

    /* set dependent acc_links log function */
    auto result = AccSetExternalLog(fun);
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("set acc_links log function failed, result: " << result);
        return result;
    }

    /* set dependent hybm core log function */
    if (g_smemInited) {
        result = HybmCoreApi::HybmCoreSetExternLogger(fun);
        if (result != SM_OK) {
            SM_LOG_AND_SET_LAST_ERROR("set hybm core log function failed, result: " << result);
            return result;
        }
    }

    return SM_OK;
}

int32_t smem_set_log_level(int level)
{
    using namespace ock::smem;

    SM_PARAM_VALIDATE(level < 0 || level > LogLevel::BUTT_LEVEL,
                      "set log level failed, invalid param, level should be 0~3", SM_INVALID_PARAM);

    /* set my logger's level */
    SMOutLogger::Instance().SetLogLevel(static_cast<LogLevel>(level));

    /* set acc_links log level */
    auto result = AccSetLogLevel(level);
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("set acc_links log level failed, result: " << result);
        return result;
    }

    /* set hybm core log level */
    if (g_smemInited) {
        result = HybmCoreApi::HybmCoreSetLogLevel(level);
        if (result != SM_OK) {
            SM_LOG_AND_SET_LAST_ERROR("set hybm core log level failed, result: " << result);
            return result;
        }
    }

    return SM_OK;
}

const char *smem_get_last_err_msg()
{
    return ock::smem::SmLastError::GetAndClear(false);
}

const char *smem_get_and_clear_last_err_msg()
{
    return ock::smem::SmLastError::GetAndClear(true);
}