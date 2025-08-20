/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <iostream>

#include "acc_includes.h"

ACC_API int32_t AccSetExternalLog(void (*func)(int level, const char* msg))
{
    ock::mf::OutLogger::Instance().SetExternalLogFunction(func);
    return ock::acc::ACC_OK;
}

ACC_API int32_t AccSetLogLevel(int level)
{
    VALIDATE_RETURN(ock::mf::OutLogger::ValidateLevel(level),
                    "set log level failed, invalid param, level should be 0~3", ock::acc::ACC_INVALID_PARAM);
    ock::mf::OutLogger::Instance().SetLogLevel(static_cast<ock::mf::LogLevel>(level));
    LOG_INFO("Log level set to " << level);

    return ock::acc::ACC_OK;
}