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
#ifndef __MEMFABRIC_SMEM_SHM_H__
#define __MEMFABRIC_SMEM_SHM_H__

#include "smem.h"
#include "smem_shm_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize smem_shm_config_t, i.e. set to default value
 *
 * @param config           [in] config to be initialized
 * @return 0 if successful
 */
int32_t smem_shm_config_init(smem_shm_config_t *config);

/**
 * @brief Initialize shm library with global config store
 * all processes need to call this function before creating a shm object,
 * all processes will connect to a global config store with specified ipPort,
 * this function will finish when all processes connected or timeout;
 * the global config store will be used to exchange information about shm object and team
 *
 * @param configStoreIpPort[in] ipPort of config store, e.g. tcp://ip:port or tcp://[ip]:port
 * @param worldSize        [in] size of processes
 * @param rankId           [in] local rank id in world size
 * @param deviceId         [in] device npu id
 * @param config           [in] config, see @smem_shm_config_t
 * @return 0 if successfully, negative value if failed, use @ref smem_get_last_error_msg to get last err msg
 */
int32_t smem_shm_init(const char *configStoreIpPort, uint32_t worldSize, uint32_t rankId, uint16_t deviceId,
                      smem_shm_config_t *config);

/**
 * @brief Un-initialize shm library with destroy all things
 *
 * @param flags            [in] optional flags, set to 0
 */
void smem_shm_uninit(uint32_t flags);

/**
 * @brief Query supported data operation type
 * @return the set of smem_shm_data_op_type
 */
uint32_t smem_shm_query_support_data_operation(void);

/**
 * @brief Create shm object peer by peer
 *
 * @param id               [in] id of the shm object
 * @param rankSize         [in] rank count
 * @param rankId           [in] my rank id
 * @param symmetricSize    [in] local memory contributed to the shm object, all ranks must the same size
 * @param dataOpType       [in] data operation engine type, i.e. MTE, SDMA, RDMA etc
 * @param flags            [in] optional flags
 * @param gva              [out] global virtual address created, it can be passed to kernel to data operations
 * @return shm object created if successful, null if failed, use @ref smem_get_last_error_msg to get last error message
 */
smem_shm_t smem_shm_create(uint32_t id, uint32_t rankSize, uint32_t rankId, uint64_t symmetricSize,
                           smem_shm_data_op_type dataOpType, uint32_t flags, void **gva);

/**
 * @brief Destroy shm object
 *
 * @param handle           [in] the shm object to be destroyed
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t smem_shm_destroy(smem_shm_t handle, uint32_t flags);

/**
 * @brief Set user extra context of shm object
 *
 * @param handle            [in] the shm object to be set
 * @param context           [in] extra context ptr
 * @param size              [in] extra context size (max is 64K)
 * @return 0 if successful
 */
int32_t smem_shm_set_extra_context(smem_shm_t handle, const void *context, uint32_t size);

/**
 * @brief Get local rank of a shm object
 *
 * @param handle            [in] the shm object
 * @return local rank in the input object, return UINT32_MAX if error
 */
uint32_t smem_shm_get_global_rank(smem_shm_t handle);

/**
 * @brief Get rank size of a shm object
 *
 * @param handle            [in] the shm object
 * @return rank size in the input object, return UINT32_MAX if error
 */
uint32_t smem_shm_get_global_rank_size(smem_shm_t handle);

/**
 * @brief Do barrier on a shm object, using control network
 *
 * @param handle            [in] the shm object
 * @return 0 if successful, other is error
 */
int32_t smem_shm_control_barrier(smem_shm_t handle);

/**
 * @brief Do all gather on a shm object, using control network
 *
 * @param handle            [in] the shm object
 * @param sendBuf           [in] input data buf
 * @param sendSize          [in] input data buf size
 * @param recvBuf           [in] output data buf
 * @param recvSize          [in] output data buf size
 * @return 0 if successful
 */
int32_t smem_shm_control_allgather(smem_shm_t handle, const char *sendBuf, uint32_t sendSize, char *recvBuf,
                                   uint32_t recvSize);

/**
 * @brief Query if remote rank can ranch
 *
 * @param handle            [in] shm object
 * @param remoteRank        [in] remote rank
 * @param reachInfo         [out] reach info, the set of smem_shm_data_op_type
 * @return 0 if successful
 */
int32_t smem_shm_topology_can_reach(smem_shm_t handle, uint32_t remoteRank, uint32_t *reachInfo);

/**
 * @brief Register function of exit
 *
 * @param exit             [in] global exit option, every rank will apply this function
 *                              to complete global exit
 * @param handle           [in] shm object
 * @return 0 if successful
 */
int32_t smem_shm_register_exit(smem_shm_t handle, void (*exit)(int));

/**
 * @brief Wait for all ranks exit
 *
 * @param handle           [in] shm object
 * @param status           [in] int
 */
void smem_shm_global_exit(smem_shm_t handle, int status);

#ifdef __cplusplus
}
#endif

#endif // __MEMFABRIC_SMEM_SHM_H__
