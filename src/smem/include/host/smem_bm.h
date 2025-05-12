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

int32_t smem_bm_init(const char *configStoreIpPort, smem_bm_mem_type memType, smem_bm_data_op_type dataOpType,
                     uint32_t worldSize, uint32_t rankId, uint16_t deviceId, smem_bm_config_t *config);

void smem_bm_uninit(uint32_t flags);

smem_bm_t smem_bm_create(uint32_t id);

void smem_bm_destroy(smem_bm_t handle);

int32_t smem_bm_join(smem_bm_t handle, uint32_t flags);

int32_t smem_bm_leave(smem_bm_t handle, uint32_t flags);

int32_t smem_bm_copy(smem_bm_t handle, void *src, void *dest, uint64_t size, smem_bm_copy_type t, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif  //__MEMFABRIC_SMEM_BM_H__