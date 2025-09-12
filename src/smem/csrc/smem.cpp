/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "smem.h"
#include "smem_common_includes.h"
#include "smem_version.h"
#include "hybm.h"
#include "acc_links/net/acc_log.h"

namespace {
bool g_smemInited = false;
}

SMEM_API int32_t smem_init(uint32_t flags)
{
    using namespace ock::smem;

    /* create logger instance */
    SMOutLogger::Instance();

    g_smemInited = true;
    SM_LOG_INFO("smem init successfully, " << LIB_VERSION);
    return SM_OK;
}

SMEM_API void smem_uninit()
{
    g_smemInited = false;
}

SMEM_API int32_t smem_set_extern_logger(void (*fun)(int, const char *))
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
    result = hybm_set_extern_logger(fun);
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("set hybm core log function failed, result: " << result);
        return result;
    }

    return SM_OK;
}

SMEM_API int32_t smem_set_log_level(int level)
{
    using namespace ock::smem;

    SM_PARAM_VALIDATE(level < 0 || level >= LogLevel::BUTT_LEVEL,
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
    result = hybm_set_log_level(level);
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("set hybm core log level failed, result: " << result);
        return result;
    }

    return SM_OK;
}

SMEM_API const char *smem_get_last_err_msg()
{
    return ock::smem::SmLastError::GetAndClear(false);
}

SMEM_API const char *smem_get_and_clear_last_err_msg()
{
    return ock::smem::SmLastError::GetAndClear(true);
}