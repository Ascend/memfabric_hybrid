/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "zbal_version.h"
#include "zbal_common_includes.h"
#include "zbal_bootstrap_default.h"

using namespace zbal;
using namespace zbal::bootstrap;

#ifdef __cplusplus
extern "C" {
#endif

ZBAL_API const char *zbal_version()
{
    /* log full version */
    ZBAL_LOG_INFO("full version: " << ZBAL_LIB_VERSION_FULL);
    /* return short version */
    return ZBAL_LIB_VERSION;
}

ZBAL_API int32_t zbal_set_logger(void (*func)(int, const char *))
{
    ZBAL_VALIDATE_RETURN(func != nullptr, "invalid param, logger function should not be null", Z_INVALID_PARAM);

    OutLogger::Instance().SetExternalLogFunction(func);

    return Z_OK;
}

ZBAL_API int32_t zbal_set_logger_level(int level)
{
    if (!OutLogger::ValidateLevel(level)) {
        ZBAL_LOG_AND_SET_LAST_ERROR("invalid param, level " << level << " is not supported");
        return Z_INVALID_PARAM;
    }

    OutLogger::Instance().SetLogLevel(LogLevel(level));

    auto bootstrap = Bootstrap::Get();
    if (bootstrap != nullptr) {
        bootstrap->SetLoggerLevel(level);
    }

    return Z_OK;
}

ZBAL_API const char *zbal_get_last_error_msg()
{
    return ZBLastError::GetAndClear(false);
}

ZBAL_API const char *zbal_get_and_clear_last_error_msg()
{
    return ZBLastError::GetAndClear(true);
}

#ifdef __cplusplus
}
#endif
