/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "smem_bm.h"

static uint64_t g_spaces[SMEM_MEM_TYPE_BUTT] = {0};

// 实现所有函数返回0或默认值
int32_t smem_bm_config_init(smem_bm_config_t *config)
{
    return 0;
}

int32_t smem_bm_init(const char *storeURL, uint32_t worldSize,
                    uint16_t deviceId, const smem_bm_config_t *config)
{
    return 0;
}

void smem_bm_uninit(uint32_t flags)
{
    return;
}

uint32_t smem_bm_get_rank_id(void)
{
    return 0; // 默认返回rank 0
}

smem_bm_t smem_bm_create(uint32_t id, uint32_t memberSize,
                        smem_bm_data_op_type dataOpType, uint64_t localDRAMSize,
                        uint64_t localHBMSize, uint32_t flags)
{
    g_spaces[SMEM_MEM_TYPE_DEVICE] = localHBMSize;
    g_spaces[SMEM_MEM_TYPE_HOST] = localDRAMSize;
    return reinterpret_cast<smem_bm_t>(0x1234); // 返回非空伪句柄
}

void smem_bm_destroy(smem_bm_t handle)
{
    return;
}

int32_t smem_bm_join(smem_bm_t handle, uint32_t flags, void **localGvaAddress)
{
    return 0;
}

int32_t smem_bm_leave(smem_bm_t handle, uint32_t flags)
{
    return 0;
}

uint64_t smem_bm_get_local_mem_size(smem_bm_t handle)
{
    return 0;
}

void *smem_bm_ptr(smem_bm_t handle, uint16_t peerRankId)
{
    return reinterpret_cast<void*>(0x5678);
}

void* smem_bm_ptr_by_mem_type(smem_bm_t handle, smem_bm_mem_type memType, uint16_t peerRankId)
{
    if (memType == SMEM_MEM_TYPE_DEVICE) {
        return reinterpret_cast<void*>(0x200000000);
    } else if (memType == SMEM_MEM_TYPE_HOST) {
        return reinterpret_cast<void*>(0x800000000);
    }
    return reinterpret_cast<void*>(0x5678);
}

uint64_t smem_bm_get_local_mem_size_by_mem_type(smem_bm_t handle, smem_bm_mem_type memType)
{
    if (memType >= SMEM_MEM_TYPE_BUTT) {
        return 0;
    }
    return g_spaces[memType];
}

int32_t smem_bm_copy(smem_bm_t handle, const void *src, void *dest,
                     uint64_t size, smem_bm_copy_type t, uint32_t flags)
{
    return 0;
}

int32_t smem_bm_copy_2d(smem_bm_t handle, const void *src, uint64_t spitch,
                        void *dest, uint64_t dpitch, uint64_t width, uint64_t height,
                        smem_bm_copy_type t, uint32_t flags)
{
    return 0;
}

int32_t smem_bm_register_into_svsp(uint64_t addr, uint64_t size)
{
    return 0;
}

int32_t smem_bm_copy_batch(smem_bm_t handle, smem_batch_copy_params *params, smem_bm_copy_type t, uint32_t flags)
{
    return 0;
}

int32_t smem_bm_wait(smem_bm_t handle)
{
    return 0;
}