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

#ifndef MF_HYBRID_SMEM_TRANS_H
#define MF_HYBRID_SMEM_TRANS_H

#include "stddef.h"
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
 * @param handle           [in] transfer object handle
 * @param address          [in] start address of the contiguous memory space
 * @param capacity         [in] size of contiguous memory space
 * @param flags            [in] optional flags
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
int32_t smem_trans_batch_register_mem(smem_trans_t handle, void *addresses[], size_t capacities[], uint32_t count,
                                      uint32_t flags);

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
 * @param localAddr        [in] Pointer to the start address of local source data storage
 * @param remoteUniqueId   [in] Unique identifier of the remote TRANS instance
 * @param remoteAddr       [in] Pointer to the start address of remote target storage
 * @param dataSize         [in] data size to be transferred
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t smem_trans_write(smem_trans_t handle, const void *localAddr, const char *remoteUniqueId, void *remoteAddr,
                         size_t dataSize, uint32_t flags);

/**
 * @brief Transfer data to peer with write in batch
 *
 * @param handle           [in] transfer object handle
 * @param localAddrs       [in] Array of pointers to the start addresses of batch local source data storage
 * @param remoteUniqueId   [in] Unique identifier of the remote TRANS instance
 * @param remoteAddrs      [in] Array of pointers to the start addresses of batch remote target storage
 * @param dataSizes        [in] Array of byte counts corresponding to batch transmitted data
 * @param batchSize        [in] Total number of tasks in batch transmission
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t smem_trans_batch_write(smem_trans_t handle, const void *localAddrs[], const char *remoteUniqueId,
                               void *remoteAddrs[], size_t dataSizes[], uint32_t batchSize, uint32_t flags);

/**
 * @brief Read data from peer to local
 *
 * @param handle           [in] transfer object handle
 * @param localAddr        [in] Pointer to the start address of local received data storage
 * @param remoteUniqueId   [in] Unique identifier of the remote TRANS instance
 * @param remoteAddr       [in] Pointer to the start address of remote data to be read
 * @param dataSize         [in] Number of bytes for single read operation
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t smem_trans_read(smem_trans_t handle, void *localAddr, const char *remoteUniqueId, const void *remoteAddr,
                        size_t dataSize, uint32_t flags);

/**
 * @brief Read data from peer to local in batch
 *
 * @param handle           [in] transfer object handle
 * @param localAddrs       [in] Array of pointers to the start addresses of batch local received data storage
 * @param remoteUniqueId   [in] Unique identifier of the remote TRANS instance
 * @param remoteAddrs      [in] Array of pointers to the start addresses of batch remote data to be read
 * @param dataSizes        [in] Array of byte counts corresponding to batch read data
 * @param batchSize        [in] Total number of tasks in batch read operation
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t smem_trans_batch_read(smem_trans_t handle, void *localAddrs[], const char *remoteUniqueId,
                              const void *remoteAddrs[], size_t dataSizes[], uint32_t batchSize, uint32_t flags);

/**
 * @brief submit read task into stream, only support dataOpType == SMEMB_DATA_OP_SDMA now
 *
 * @param handle           [in] transfer object handle
 * @param localAddr        [in] Pointer to the start address of local received data storage
 * @param remoteUniqueId   [in] Unique identifier of the remote TRANS instance
 * @param remoteAddr       [in] Pointer to the start address of remote data to be read
 * @param dataSize         [in] Number of bytes for single read operation
 * @param stream           [in] acl rt stream
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t smem_trans_read_submit(smem_trans_t handle, void *localAddr, const char *remoteUniqueId, const void *remoteAddr,
                               size_t dataSize, void *stream, uint32_t flags);

/**
 * @brief submit write task into stream, only support dataOpType == SMEMB_DATA_OP_SDMA now
 *
 * @param handle           [in] transfer object handle
 * @param localAddr        [in] Pointer to the start address of local source data storage
 * @param remoteUniqueId   [in] Unique identifier of the remote TRANS instance
 * @param remoteAddr       [in] Pointer to the start address of remote target storage
 * @param dataSize         [in] data size to be transferred
 * @param stream           [in] acl rt stream
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t smem_trans_write_submit(smem_trans_t handle, const void *localAddr, const char *remoteUniqueId,
                                void *remoteAddr, size_t dataSize, void *stream, uint32_t flags);

/**
 * @brief submit batch read task into stream, only support dataOpType == SMEMB_DATA_OP_SDMA now
 *
 * @param handle           [in] transfer object handle
 * @param localAddrs       [in] Array of pointers to the start addresses of batch local received data storage
 * @param remoteUniqueId   [in] Unique identifier of the remote TRANS instance
 * @param remoteAddrs      [in] Array of pointers to the start addresses of batch remote data to be read
 * @param dataSizes        [in] Array of byte counts corresponding to batch read data
 * @param batchSize        [in] Total number of tasks in batch read operation
 * @param stream           [in] acl rt stream
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t smem_trans_batch_read_submit(smem_trans_t handle, void *localAddrs[], const char *remoteUniqueId,
                                     const void *remoteAddrs[], size_t dataSizes[], uint32_t batchSize,
                                     void *stream, uint32_t flags);

/**
 * @brief submit batch write task into stream, only support dataOpType == SMEMB_DATA_OP_SDMA now
 *
 * @param handle           [in] transfer object handle
 * @param localAddrs       [in] Array of pointers to the start addresses of batch local source data storage
 * @param remoteUniqueId   [in] Unique identifier of the remote TRANS instance
 * @param remoteAddrs      [in] Array of pointers to the start addresses of batch remote target storage
 * @param dataSizes        [in] Array of byte counts corresponding to batch transmitted data
 * @param batchSize        [in] Total number of tasks in batch transmission
 * @param stream           [in] acl rt stream
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t smem_trans_batch_write_submit(smem_trans_t handle, const void *localAddrs[],
                                      const char *remoteUniqueId, void *remoteAddrs[], size_t dataSizes[],
                                      uint32_t batchSize, void *stream, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif // MF_HYBRID_SMEM_TRANS_H
