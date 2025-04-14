/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "smem_common_includes.h"
#include "smem.h"
#include "hybm.h"

int32_t smem_init(uint64_t globalSize, uint32_t flags)
{
    int32_t ret = hybm_init(globalSize, flags);
    if (ret != 0) {
        SM_LOG_ERROR("init hybm_core failed(" << ret << "), flags(0x" << std::hex << flags << ")");
        return -1;
    }
    SM_LOG_WARN("hybm_core init success");
    return 0;
}

void smem_uninit()
{
    hybm_uninit();
    SM_LOG_WARN("hybm_core uninit finish");
}

int32_t smem_set_extern_logger(void (*fun)(int, const char *))
{
    SM_PARAM_VALIDATE(fun == nullptr, "Invalid param, fun is NULL", ock::smem::SM_INVALID_PARAM);

    ock::smem::SMOutLogger::Instance()->SetExternalLogFunction(fun, true);
    // 设置 hybm logger
    return hybm_set_extern_logger(fun);
}

int32_t smem_set_log_level(int level)
{
    if (level < ock::smem::BUTT_LEVEL) {
        ock::smem::SMOutLogger::Instance()->SetLogLevel(static_cast<ock::smem::LogLevel>(level));
        return 0;
    }

    return ock::smem::SM_INVALID_PARAM;
}

const char *smem_get_error_msg(int32_t errCode)
{
    return hybm_get_error_string(errCode);
}