/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef __MEMFABRIC_SMEM_DEF_H__
#define __MEMFABRIC_SMEM_DEF_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *smem_shm_t;
#define SMEM_SHM_TIMEOUT_MAX     UINT32_MAX /* all timeout must <= UINT32_MAX */

/**
 * @brief NPU initiated data operation type, currently only support MTE
 */
typedef enum {
    SMEMS_DATA_OP_MTE  = 1U << 0,
    SMEMS_DATA_OP_SDMA = 1U << 1,
    SMEMS_DATA_OP_RDMA = 1U << 2,
} smem_shm_data_op_type;

/**
 * shm config, include operation timeout
 * controlOperationTimeout: control operation timeout in second, i.e. barrier, allgather, topology_can_reach etc
 */
typedef struct {
    uint32_t shmInitTimeout;          /* func smem_shm_init timeout, default 120 second
                                         (min is 1, max is SMEM_BM_TIMEOUT_MAX) */
    uint32_t shmCreateTimeout;        /* func smem_shm_create timeout, default 120 second
                                         (min is 1, max is SMEM_BM_TIMEOUT_MAX) */
    uint32_t controlOperationTimeout; /* control operation timeout, i.e. barrier, allgather,topology_can_reach etc,
                                         default 120 second (min is 1, max is SMEM_BM_TIMEOUT_MAX) */
    bool startConfigStore;            /* whether to start config store, default true */
    uint32_t flags;                   /* other flag, default 0 */
} smem_shm_config_t;

#ifdef __cplusplus
}
#endif
#endif  // __MEMFABRIC_SMEM_DEF_H__