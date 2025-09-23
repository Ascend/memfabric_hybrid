/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef MF_HYBRID_SMEM_TRANS_H
#define MF_HYBRID_SMEM_TRANS_H

#include "smem.h"
#include "smem_trans_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the config struct
 *
 * @param config           [in] ptr of config to be initialized
 * @return 0 if successful
 */
int32_t smem_trans_config_init(smem_trans_config_t *config);

/**
 * @brief Initialize transfer
 * all processes need to call this function before creating a trans object
 *
 * @param config           [in] the config for config
 * @return 0 if successful
 */
int32_t smem_trans_init(const smem_trans_config_t *config);

/**
 * @brief Un-initialize transfer
 *
 * @param flags            [in] optional flags
 */
void smem_trans_uninit(uint32_t flags);

/**
 * @brief Create a transfer object with config, this transfer need to connect to global config store to exchange
 * inner information for various protocols on different hardware
 *
 * @param storeUrl         [in] the url of config store, the store is created by <i>smem_create_config_store</i>
 * @param uniqueId         [in] unique id for data transfer, which should be unique, a better practice is using ip:port
 * @param config           [in] the config for config
 * @return transfer object created if successful
 */
smem_trans_t smem_trans_create(const char *storeUrl, const char *uniqueId, const smem_trans_config_t *config);

/**
 * @brief Destroy the transfer created by <i>smem_trans_create</i>
 *
 * @param handle           [in] the transfer object created
 * @param flags            [in] optional flags
 */
void smem_trans_destroy(smem_trans_t handle, uint32_t flags);

/**
 * @brief Register a contiguous memory space to be transferred
 *
 * @param handle            [in] transfer object handle
 * @param address           [in] start address of the contiguous memory space
 * @param capacity          [in] size of contiguous memory space
 * @param flags             [in] optional flags
 * @return 0 if successful
 */
int32_t smem_trans_register_mem(smem_trans_t handle, void *address, size_t capacity, uint32_t flags);

/**
 * @brief Register multiple contiguous memory spaces to be transferred
 *
 * @param handle           [in] transfer object handle
 * @param addresses        [in] starts addresses of the contiguous memory spaces
 * @param capacities       [in] sizes of the contiguous memory spaces
 * @param count            [in] count of the contiguous memory spaces
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t smem_trans_batch_register_mem(smem_trans_t handle, void *addresses[], size_t capacities[],
                                      uint32_t count, uint32_t flags);

/**
 * @brief De-register contiguous memory spaces that registered by smem_trans_register_mem(s)
 *
 * @param handle           [in] transfer object handle
 * @param address          [in] start address of the contiguous memory space
 * @return 0 if successful
 */
int32_t smem_trans_deregister_mem(smem_trans_t handle, void *address);

/**
 * @brief Transfer data to peer with write
 *
 * @param handle           [in] transfer object handle
 * @param srcAddress       [in] address of src data to be written to peer
 * @param destUniqueId     [in] uniqueId of dst
 * @param destAddress      [in] address of dst
 * @param dataSize         [in] data size to be transfered
 * @return 0 if successful
 */
int32_t smem_trans_write(smem_trans_t handle, const void *srcAddress, const char *destUniqueId, void *destAddress,
                         size_t dataSize);

/**
 * @brief Transfer data to peer with write in batch
 *
 * @param handle           [in] transfer object handle
 * @param srcAddresses     [in] addresses of src data to be written to peer
 * @param destUniqueId     [in] uniqueId of dst
 * @param destAddresses    [in] addresses of data
 * @param dataSizes        [in] sizes of data
 * @param batchSize        [in] batch size
 * @return 0 if successful
 */
int32_t smem_trans_batch_write(smem_trans_t handle, const void *srcAddresses[], const char *destUniqueId,
                               void *destAddresses[], size_t dataSizes[], uint32_t batchSize);

#ifdef __cplusplus
}
#endif

#endif  // MF_HYBRID_SMEM_TRANS_H
