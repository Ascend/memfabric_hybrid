/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <iostream>

#include "acc_includes.h"

ACC_API int32_t AccSetExternalLog(void (*func)(int level, const char* msg))
{
    using namespace ock::acc;

    auto instance = AccOutLogger::Instance();
    if (instance == nullptr) {
        std::cout << "Failed to get logger instance" << std::endl;
        return ACC_ERROR;
    }

    instance->SetExternalLogFunction(func);

    return ACC_OK;
}

ACC_API int32_t AccSetLogLevel(int level)
{
    using namespace ock::acc;

    if (level < AccLogLevel::DEBUG_LEVEL || level >= AccLogLevel::BUTT_LEVEL) {
        std::cout << "Failed to set log level to " << level << "which should be 0,1,2,3" << std::endl;
        return ACC_INVALID_PARAM;
    }

    auto instance = AccOutLogger::Instance();
    if (instance == nullptr) {
        std::cout << "Failed to get logger instance" << std::endl;
        return ACC_ERROR;
    }

    instance->SetLogLevel(static_cast<AccLogLevel>(level));

    LOG_INFO("Log level set to " << level);

    return ACC_OK;
}