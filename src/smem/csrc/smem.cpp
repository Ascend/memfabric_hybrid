/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm.h"
#include "smem.h"
#include "smem_common_includes.h"
#include "acc_links/net/acc_log.h"

int32_t smem_init(uint32_t flags)
{
    return 0;
}

void smem_uninit()
{
    return;
}

int32_t smem_set_extern_logger(void (*fun)(int, const char *))
{
    using namespace ock::smem;
    SM_PARAM_VALIDATE(fun == nullptr, "invalid param, fun is NULL", ock::smem::SM_INVALID_PARAM);

    /* set my out logger */
    auto instance = SMOutLogger::Instance();
    if (instance == nullptr) {
        std::cout << "create logger instance failed, probably out of memory";
        return SM_NEW_OBJECT_FAILED;
    }
    instance->SetExternalLogFunction(fun, true);

    /* set dependent acc_links log function */
    auto result = AccSetExternalLog(fun);
    if (result != SM_OK) {
        std::cout << "set acc_links log function failed(" << result << ")" << std::endl;
        return result;
    }

    /* set dependent hybm core log function */
    result = hybm_set_extern_logger(fun);
    if (result != SM_OK) {
        std::cout << "set hybm core log function failed(" << result << ")" << std::endl;
        return result;
    }

    return SM_OK;
}

int32_t smem_set_log_level(int level)
{
    using namespace ock::smem;
    SM_PARAM_VALIDATE(level < 0 || level > LogLevel::BUTT_LEVEL, "invalid param, level should be 0~3",
                      ock::smem::SM_INVALID_PARAM);

    /* set my logger's level */
    auto instance = SMOutLogger::Instance();
    if (instance == nullptr) {
        std::cout << "create logger instance failed, probably out of memory";
        return SM_NEW_OBJECT_FAILED;
    }
    SMOutLogger::Instance()->SetLogLevel(static_cast<ock::smem::LogLevel>(level));

    /* set acc_links log level */
    auto result = AccSetLogLevel(level);
    if (result != SM_OK) {
        std::cout << "set acc_links log level failed(" << result << ")" << std::endl;
        return result;
    }

    /* set hybm core log level */
    result = hybm_set_log_level(level);
    if (result != SM_OK) {
        std::cout << "set hybm core log level failed(" << result << ")" << std::endl;
        return result;
    }

    return SM_OK;
}

const char *smem_get_error_msg(int32_t errCode)
{
    return hybm_get_error_string(errCode);
}