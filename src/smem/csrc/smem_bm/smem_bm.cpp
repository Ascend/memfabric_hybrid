/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "smem_common_includes.h"
#include "hybm_core_api.h"
#include "smem_bm.h"
#include "smem_logger.h"

using namespace ock::smem;
#ifdef UT_ENABLED
thread_local std::mutex g_smemBmMutex_;
thread_local bool g_smemBmInited = false;
#else
std::mutex g_smemBmMutex_;
bool g_smemBmInited = false;
#endif

SMEM_API int32_t smem_bm_config_init(smem_bm_config_t *config)
{
    SM_PARAM_VALIDATE(config == nullptr, "Invalid config", SM_INVALID_PARAM);

    // TODO

    return SM_OK;
}

SMEM_API int32_t smem_bm_init(const char *configStoreIpPort, smem_bm_mem_type memType, smem_bm_data_op_type dataOpType,
                              uint32_t worldSize, uint32_t rankId, uint16_t deviceId, uint64_t gvaSpaceSize,
                              smem_bm_config_t *config)
{
    // TODO
    return SM_OK;
}

SMEM_API void smem_bm_uninit(uint32_t flags)
{
}

SMEM_API smem_bm_t smem_bm_create(uint32_t id)
{
    return nullptr;
}

SMEM_API void smem_bm_destroy(smem_bm_t handle)
{
}

SMEM_API int32_t smem_bm_join(smem_bm_t handle, uint32_t flags)
{
    return SM_OK;
}

SMEM_API int32_t smem_bm_leave(smem_bm_t handle, uint32_t flags)
{
    return SM_OK;
}

SMEM_API int32_t smem_bm_copy(smem_bm_t handle, void *src, void *dest, uint64_t size, smem_bm_copy_type t,
                              uint32_t flags)
{
    return SM_OK;
}
