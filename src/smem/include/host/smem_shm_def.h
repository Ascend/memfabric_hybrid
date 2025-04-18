/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __MEMFABRIC_SMEM_DEF_H__
#define __MEMFABRIC_SMEM_DEF_H__

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *smem_shm_t;
typedef void *smem_shm_team_t;

typedef enum {
    SMEMS_DATA_OP_MTE  = 1U << 0,
    SMEMS_DATA_OP_SDMA = 1U << 1,
    SMEMS_DATA_OP_ROCE = 1U << 2,
} smem_shm_data_op_type;

/**
 * shm config, include operation timeout
 * controlOperationTimeout: control operation timeout in second, i.e. barrier, allgather, topology_can_reach etc
 */
typedef struct {
    uint32_t shmInitTimeout;          /* func smem_shm_init timeout, default 120 second */
    uint32_t shmCreateTimeout;        /* func smem_shm_create timeout, default 120 second */
    uint32_t controlOperationTimeout; /* control operation timeout, i.e. barrier, allgather,topology_can_reach etc,
                                         default 120 second */
    bool startConfigStore;            /* whether to start config store, default true */
    uint32_t flags;                   /* other flag, default 0 */
} smem_shm_config_t;

#ifdef __cplusplus
}
#endif
#endif  // __MEMFABRIC_SMEM_DEF_H__