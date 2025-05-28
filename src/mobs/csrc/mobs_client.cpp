/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mobs_client.h"
#include "mobs_common_includes.h"

MOBS_API int32_t mobs_init(mobs_meta_service_config_t *config)
{
    return MO_OK;
}

MOBS_API void mobs_uninit()
{
    return;
}

MOBS_API int32_t mobs_put(const char *key, mobs_buffer *buf, uint32_t flags)
{
    return MO_OK;
}

MOBS_API int32_t mobs_get(const char *key, mobs_buffer *buf, uint32_t flags)
{
    return MO_OK;
}

MOBS_API mobs_location_t mobs_get_location(const char *key, uint32_t flags)
{
    return {};
}

MOBS_API int32_t mobs_remove(const char *key, uint32_t flags)
{
    return MO_OK;
}