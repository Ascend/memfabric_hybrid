/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mobs.h"
#include "mobs_common_includes.h"

MOBS_API int32_t mobs_init(uint32_t flags)
{
    return MO_OK;
}

MOBS_API int32_t mobs_set_extern_logger(void (*func)(int level, const char *msg))
{
    return MO_OK;
}

MOBS_API int32_t mobs_set_log_level(int level)
{
    return MO_OK;
}

MOBS_API void mobs_uninit()
{
    return MO_OK;
}

MOBS_API const char *mobs_get_last_err_msg()
{
    return NULL;
}

MOBS_API const char *mobs_get_and_clear_last_err_msg()
{
    return NULL;
}