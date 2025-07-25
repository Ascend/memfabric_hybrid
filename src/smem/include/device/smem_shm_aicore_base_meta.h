/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __MEMFABRIC_SMEM_AI_CORE_BASE_META_H__
#define __MEMFABRIC_SMEM_AI_CORE_BASE_META_H__

#include "smem_shm_aicore_base_define.h"

constexpr uint64_t SMEM_SHM_DEVICE_END_ADDR = 0x180000000000ULL - (1UL << 30UL);
constexpr uint64_t SMEM_SHM_DEVICE_PRE_META_SIZE = 128UL; // 128B
constexpr uint64_t SMEM_SHM_DEVICE_GLOBAL_META_SIZE = SMEM_SHM_DEVICE_PRE_META_SIZE; // 128B
constexpr uint64_t SMEM_OBJECT_NUM_MAX = 511UL; // entity最大数量
constexpr uint64_t SMEM_SHM_DEVICE_META_SIZE = SMEM_SHM_DEVICE_PRE_META_SIZE * SMEM_OBJECT_NUM_MAX
                                           + SMEM_SHM_DEVICE_GLOBAL_META_SIZE; // 64K

constexpr uint64_t SMEM_SHM_DEVICE_USER_CONTEXT_PRE_SIZE = 64UL * 1024UL; // 64K
constexpr uint64_t SMEM_SHM_DEVICE_INFO_SIZE = SMEM_SHM_DEVICE_USER_CONTEXT_PRE_SIZE * SMEM_OBJECT_NUM_MAX
                                           + SMEM_SHM_DEVICE_META_SIZE; // 元数据+用户context,总大小32M, 对齐2M
constexpr uint64_t SMEM_SHM_DEVICE_META_ADDR = SMEM_SHM_DEVICE_END_ADDR - SMEM_SHM_DEVICE_INFO_SIZE;
constexpr uint64_t SMEM_SHM_DEVICE_USER_CONTEXT_ADDR = SMEM_SHM_DEVICE_META_ADDR + SMEM_SHM_DEVICE_META_SIZE;

constexpr uint64_t SMEM_SHM_DEVICE_META_OBJ_ID_OFFSET = 0;
constexpr uint64_t SMEM_SHM_DEVICE_META_RANK_OFFSET = SMEM_SHM_DEVICE_META_OBJ_ID_OFFSET + sizeof(uint32_t);
constexpr uint64_t SMEM_SHM_DEVICE_META_RANK_SIZE_OFFSET = SMEM_SHM_DEVICE_META_RANK_OFFSET + sizeof(uint32_t);
constexpr uint64_t SMEM_SHM_DEVICE_META_CONTEXT_OFFSET = SMEM_SHM_DEVICE_META_RANK_SIZE_OFFSET + sizeof(uint32_t);
constexpr uint64_t SMEM_SHM_DEVICE_META_SYMM_OFFSET = SMEM_SHM_DEVICE_META_CONTEXT_OFFSET + sizeof(uint32_t);

SMEM_SHM_INLINE_AICORE uint32_t smem_shm_get_global_rank(uint32_t shmemId)
{
    if (shmemId >= SMEM_OBJECT_NUM_MAX) {
        return UINT32_MAX;
    }
    uint64_t metaAddr = SMEM_SHM_DEVICE_META_ADDR + SMEM_SHM_DEVICE_GLOBAL_META_SIZE +
        shmemId * SMEM_SHM_DEVICE_PRE_META_SIZE;
    return (*(__gm__ uint32_t*)(metaAddr + SMEM_SHM_DEVICE_META_RANK_OFFSET));
}

SMEM_SHM_INLINE_AICORE uint32_t smem_shm_get_global_rank_size(uint32_t shmemId)
{
    if (shmemId >= SMEM_OBJECT_NUM_MAX) {
        return UINT32_MAX;
    }
    uint64_t metaAddr = SMEM_SHM_DEVICE_META_ADDR + SMEM_SHM_DEVICE_GLOBAL_META_SIZE +
        shmemId * SMEM_SHM_DEVICE_PRE_META_SIZE;
    return (*(__gm__ uint32_t*)(metaAddr + SMEM_SHM_DEVICE_META_RANK_SIZE_OFFSET));
}

SMEM_SHM_INLINE_AICORE uint64_t smem_shm_get_symmetric_size(uint32_t shmemId)
{
    if (shmemId >= SMEM_OBJECT_NUM_MAX) {
        return 0;
    }
    uint64_t metaAddr = SMEM_SHM_DEVICE_META_ADDR + SMEM_SHM_DEVICE_GLOBAL_META_SIZE +
        shmemId * SMEM_SHM_DEVICE_PRE_META_SIZE;
    return (*(__gm__ uint64_t*)(metaAddr + SMEM_SHM_DEVICE_META_SYMM_OFFSET));
}

SMEM_SHM_INLINE_AICORE __gm__ void* smem_shm_get_extra_context_addr(uint32_t shmemId)
{
    if (shmemId >= SMEM_OBJECT_NUM_MAX) {
        return NULL;
    }
    uint64_t ctxAddr = SMEM_SHM_DEVICE_USER_CONTEXT_ADDR + shmemId * SMEM_SHM_DEVICE_USER_CONTEXT_PRE_SIZE;
    return ((__gm__ void*)(ctxAddr));
}

SMEM_SHM_INLINE_AICORE uint32_t smem_shm_get_extra_context_size(uint32_t shmemId)
{
    if (shmemId >= SMEM_OBJECT_NUM_MAX) {
        return 0;
    }
    uint64_t metaAddr = SMEM_SHM_DEVICE_META_ADDR + SMEM_SHM_DEVICE_GLOBAL_META_SIZE +
        shmemId * SMEM_SHM_DEVICE_PRE_META_SIZE;
    return (*(__gm__ uint32_t*)(metaAddr + SMEM_SHM_DEVICE_META_CONTEXT_OFFSET));
}

#endif // __MEMFABRIC_SMEM_AI_CORE_BASE_META_H__
