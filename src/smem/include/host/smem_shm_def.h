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
    SMEMS_DATA_OP_MTE = 0x01,
} smem_shm_data_op_type;

typedef enum {
    SMEM_TRANSPORT_CAP_MAP  = 1U << 0,       // memory has mapped
    SMEM_TRANSPORT_CAP_MTE  = 1U << 1,       // can access by MTE
    SMEM_TRANSPORT_CAP_ROCE = 1U << 2,       // can access by ROCE
    SMEM_TRANSPORT_CAP_SDMA = 1U << 3,       // can access by SDMA
} smem_shm_transport_capabilities_type;

/**
 * shm config, include operation timeout
 * controlOperationTimeout: control operation timeout in second, i.e. barrier, allgather, topology_can_reach etc
 */
typedef struct {
    uint32_t shmInitTimeout;            /* func smem_shm_init timeout, default 120 second */
    uint32_t shmCreateTimeout;          /* func smem_shm_create timeout, default 120 second */
    uint32_t controlOperationTimeout;   /* control operation timeout, i.e. barrier, allgather, topology_can_reach etc, default 120 second */
    bool startConfigStore;              /* whether to start config store, default true */
    uint32_t flags;                     /* other flag, default 0 */
} smem_shm_config_t;

#ifdef __cplusplus
}
#endif
#endif // __MEMFABRIC_SMEM_DEF_H__