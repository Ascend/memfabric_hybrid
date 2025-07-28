/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_SMEM_TRANS_H
#define MF_HYBRID_SMEM_TRANS_H

#include <stddef.h>
#include "smem.h"
#include "smem_trans_def.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t smem_trans_config_init(smem_trans_config_t *config);

smem_trans_t smem_trans_create(const char *storeUrl, const char *sessionId, const smem_trans_config_t *config);

void smem_trans_destroy(smem_trans_t handle, uint32_t flags);

void smem_trans_uninit(uint32_t flags);

int32_t smem_trans_register_mem(smem_trans_t handle, void *address, size_t capacity, uint32_t flags);

int32_t smem_trans_register_mems(smem_trans_t handle, void *addresses[], size_t capacities[], uint32_t count, uint32_t flags);

int32_t smem_trans_deregister_mem(smem_trans_t handle, void *address);

int32_t smem_trans_write(smem_trans_t handle, const void *srcAddress, const char *destSession, void *destAddress,
                         size_t dataSize);

int32_t smem_trans_batch_write(smem_trans_t handle, const void *srcAddresses[], const char *destSession,
                               void *destAddresses[], size_t dataSizes[], uint32_t batchSize);

#ifdef __cplusplus
}
#endif

#endif  // MF_HYBRID_SMEM_TRANS_H
