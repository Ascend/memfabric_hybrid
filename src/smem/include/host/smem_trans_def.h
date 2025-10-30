/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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
    smem_trans_role_t role; /* transfer role */
    uint32_t initTimeout;   /* func timeout, default 120 seconds */
    uint32_t deviceId;      /* npu device id */
    uint32_t flags;         /* optional flags */
    smem_bm_data_op_type dataOpType;  /* data operation type */
} smem_trans_config_t;

#ifdef __cplusplus
}
#endif

#endif  // MF_HYBRID_SMEM_TRANS_DEF_H
