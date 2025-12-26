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
#ifndef __MEMFABRIC_SMEM_BM_H__
#define __MEMFABRIC_SMEM_BM_H__

#include "smem.h"
#include "smem_bm_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set the config to default values
 *
 * @param config           [in] the config to be set
 * @return 0 if successful
 */
int32_t smem_bm_config_init(smem_bm_config_t *config);

/**
 * @brief Initialize Big Memory library, which composes device memory on many NPUs and host memory on many hosts
 * into global shared memory space for high throughput data store or data transfer.
 * For instance, user can store KVCache into the global shared memory for reuse. i.e. Once a worker generates
 * the KVBlocks then copy to global shared memory space, other workers can read it
 * by data copy as well.
 *
 * @param storeURL         [in] configure store url for control, e.g. tcp://ip:port or tcp://[ip]:port
 * @param worldSize        [in] number of guys participating
 * @param deviceId         [in] device id
 * @param config           [in] extract config
 * @return 0 if successful
 */
int32_t smem_bm_init(const char *storeURL, uint32_t worldSize, uint16_t deviceId, const smem_bm_config_t *config);

/**
 * @brief Un-initialize the Big Memory library
 *
 * @param flags            [in] optional flags, not used yet
 */
void smem_bm_uninit(uint32_t flags);

/**
 * @brief Get the rank id, assigned during initialization i.e. after call <i>smem_bm_init</i>
 *
 * @return rank id if successful, UINT32_MAX is returned if failed
 */
uint32_t smem_bm_get_rank_id(void);

/**
 * @brief Create a Big Memory object locally after initialized, this only create local memory segment and after
 * call <i>smem_bm_join</i> the local memory segment will be joined into global space. One Big Memory object is
 * a global memory space, data operation does work across different Big Memory object.
 * We need to specify different <i>id</i> for different Big Memory object.
 *
 * @param id               [in] identity of the Big Memory object
 * @param memberSize       [in] number of guys participating, which should equal or less the world size
 * @param dataOpType       [in] data operation type, SDMA or RoCE etc
 * @param localDRAMSize    [in] the size of local DRAM memory contributes to Big Memory object
 * @param localHBMSize     [in] the size of local HBM memory contributes to Big Memory object
 * @param flags            [in] optional flags
 * @return Big Memory object handle if successful, nullptr if failed
 */
smem_bm_t smem_bm_create(uint32_t id, uint32_t memberSize,
                         smem_bm_data_op_type dataOpType, uint64_t localDRAMSize,
                         uint64_t localHBMSize, uint32_t flags);

/**
 * @brief Destroy the Big Memory object
 *
 * @param handle           [in] the Big Memory object to be destroyed
 */
void smem_bm_destroy(smem_bm_t handle);

/**
 * @brief Join to global Big Memory space actively, after this, we can operate on the global space,
 * i.e. use <i>smem_bm_ptr</i> to get peer gva address and use <i>smem_bm_copy</i> to do data copy
 *
 * @param handle           [in] Big Memory object handle created by <i>smem_bm_create</i>
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t smem_bm_join(smem_bm_t handle, uint32_t flags);

/**
 * @brief Leave the global Big Memory space actively, after this, we cannot operate on the global space anymore
 *
 * @param handle           [in] Big Memory object handle created by <i>smem_bm_create</i>
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t smem_bm_leave(smem_bm_t handle, uint32_t flags);

/**
 * @brief Get the size of memory allocated locally by memory type
 *
 * @param handle           [in] Big Memory object handle created by smem_bm_create
 * @param memType          [in] memory type, device or host
 * @return local memory size in bytes
 */
uint64_t smem_bm_get_local_mem_size_by_mem_type(smem_bm_t handle, smem_bm_mem_type memType);

/**
 * @brief Get GVA by memory type and rank id
 *
 * @param handle           [in] Big Memory object handle created by smem_bm_create
 * @param memType          [in] memory type, SMEM_MEM_TYPE_DEVICE or SMEM_MEM_TYPE_HOST
 * @param peerRankId       [in] rank id of peer
 * @return memory ptr of peer gva; start address of GVA when peerRankId = 0
 */
void *smem_bm_ptr_by_mem_type(smem_bm_t handle, smem_bm_mem_type memType, uint16_t peerRankId);

/**
 * @brief Data copy on Big Memory object, several copy types supported:
 * L2G: local memory to global space
 * G2L: global space to local memory
 * G2H: global space to host memory
 * H2G: host memory to global space
 *
 * @param handle           [in] Big Memory object handle created by <i>smem_bm_create</i>
 * @param params           [in] Pointer to the batch copy parameter structure
 *                              - src: source gva of data
 *                              - dest: target gva of data
 *                              - size: size of data to be copied
 * @param t                [in] copy type, L2G, G2L, G2H, H2G
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t smem_bm_copy(smem_bm_t handle, smem_copy_params *params, smem_bm_copy_type t, uint32_t flags);

/**
 * @brief Perform batch data copy operations on Big Memory
 * @param handle           [in] Big Memory object handle, created and returned by the smem_bm_create function,
 *                              used to identify the target shared memory instance
 * @param params           [in] Pointer to the batch copy parameter structure
 *                              - sources: Pointer to an array of source addresses
 *                              - destinations: Pointer to an array of destination addresses
 *                              - dataSizes: Pointer to an array of data lengths
 *                              - batchSize: Number of copy groups in the batch
 * @param t                [in] copy type, L2G, G2L, G2H, H2G
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t smem_bm_copy_batch(smem_bm_t handle, smem_batch_copy_params *params, smem_bm_copy_type t, uint32_t flags);

/**
 * @brief Wait all issued async copy(s) finish
 *
 * @return 0 if successful
 */
int32_t smem_bm_wait(smem_bm_t handle);

/**
 * @brief Register hbm mem, support sdma or rdma
 *
 * @param handle           [in] Big Memory object handle created by <i>smem_bm_create</i>
 * @param addr             [in] register addr
 * @param size             [in] register size
 */
int32_t smem_bm_register_user_mem(smem_bm_t handle, uint64_t addr, uint64_t size);

/**
 * @brief UnRegister hbm mem, support sdma or rdma
 *
 * @param handle           [in] Big Memory object handle created by <i>smem_bm_create</i>
 * @param addr             [in] unregister addr
 */
int32_t smem_bm_unregister_user_mem(smem_bm_t handle, uint64_t addr);

/**
 * @brief Get rank id of gva that belongs to
 *
 * @param handle           [in] Big Memory object handle created by <i>smem_bm_create</i>
 * @param gva              [in] global virtual address
 * @return rank id if successful, UINT32_MAX is returned if failed
 */
uint32_t smem_bm_get_rank_id_by_gva(smem_bm_t handle, void *gva);

#ifdef __cplusplus
}
#endif

#endif  //__MEMFABRIC_SMEM_BM_H__