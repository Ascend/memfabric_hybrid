/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __MEMFABRIC_SMEM_BM_H__
#define __MEMFABRIC_SMEM_BM_H__

#include <smem_bm_def.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t smem_bm_config_init(smem_bm_config_t *config);

int32_t smem_bm_init(const char *storeURL, uint32_t worldSize, uint16_t deviceId, const smem_bm_config_t *config);

void smem_bm_uninit(uint32_t flags);

uint32_t smem_bm_get_rank_id(void);

smem_bm_t smem_bm_create(uint32_t id, uint32_t memberSize,
        smem_bm_mem_type memType, smem_bm_data_op_type dataOpType,
        uint64_t localMemorySize, uint32_t flags);

void smem_bm_destroy(smem_bm_t handle);

int32_t smem_bm_join(smem_bm_t handle, uint32_t flags, void **localGvaAddress);

int32_t smem_bm_leave(smem_bm_t handle, uint32_t flags);

uint64_t smem_bm_get_local_mem_size(smem_bm_t handle);

void *smem_bm_ptr(smem_bm_t handle, uint16_t peerRankId);

int32_t smem_bm_copy(smem_bm_t handle, void *src, void *dest, uint64_t size, smem_bm_copy_type t, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif  //__MEMFABRIC_SMEM_BM_H__