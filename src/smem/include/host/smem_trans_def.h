/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_SMEM_TRANS_DEF_H
#define MF_HYBRID_SMEM_TRANS_DEF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *smem_trans_t;

typedef enum {
    SMEM_TRANS_NONE = 0,
    SMEM_TRANS_SENDER,
    SMEM_TRANS_RECEIVER,
    SMEM_TRANS_BOTH,
    SMEM_TRANS_BUTT
} smem_trans_role_t;

typedef struct {
    smem_trans_role_t role;    /* transfer role */
    uint32_t initTimeout;      /* func timeout, default 120 seconds */
    uint32_t deviceId;         /* npu device id */
    uint32_t flags;            /* optional flags */
} smem_trans_config_t;

#ifdef __cplusplus
}
#endif

#endif  // MF_HYBRID_SMEM_TRANS_DEF_H
