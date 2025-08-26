/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef __MEMFABRIC_SMEM_COMMON_DEF_H__
#define __MEMFABRIC_SMEM_COMMON_DEF_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * when dataOpType is SMEMB_DATA_OP_ROCE(smem_bm_create) or SMEMS_DATA_OP_ROCE(smem_shm_create)
 * one of config flags below must be set during smem_bm_init or smem_shm_init
 */
typedef enum {
    /*
     * for smem_bm_init
     */
    SMEM_INIT_FLAG_NEED_HOST_RDMA  = 1U << 16,
    /*
     * for smem_shm_init or smem_bm_init
     */
    SMEM_INIT_FLAG_NEED_DEVICE_RDMA  = 1U << 17,
} smem_init_flags;

#ifdef __cplusplus
}
#endif
#endif // __MEMFABRIC_SMEM_COMMON_DEF_H__