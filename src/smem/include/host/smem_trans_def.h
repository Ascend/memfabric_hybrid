/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/
#ifndef MF_HYBRID_SMEM_TRANS_DEF_H
#define MF_HYBRID_SMEM_TRANS_DEF_H

#include <stdint.h>
#include "smem_bm_def.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *smem_trans_t;

/*
 * @brief Transfer role, i.e. sender/receiver
 */
typedef enum {
    SMEM_TRANS_NONE = 0, /* no role */
    SMEM_TRANS_SENDER,   /* sender */
    SMEM_TRANS_RECEIVER, /* receiver */
    SMEM_TRANS_BOTH,     /* both sender and receiver */
    SMEM_TRANS_BUTT
} smem_trans_role_t;

/**
 * @brief Transfer config
 */
typedef struct {
    smem_trans_role_t role;          /* transfer role */
    uint32_t initTimeout;            /* func timeout, default 120 seconds */
    uint32_t deviceId;               /* npu device id */
    uint32_t flags;                  /* optional flags */
    smem_bm_data_op_type dataOpType; /* data operation type */
    bool startConfigServer;          /* whether to start config store, default false */
} smem_trans_config_t;

#ifdef __cplusplus
}
#endif

#endif // MF_HYBRID_SMEM_TRANS_DEF_H
