/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __MEMFABRIC_SMEM_SHM_H__
#define __MEMFABRIC_SMEM_SHM_H__

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
    uint32_t shmInitTimeout;            // func smem_shm_init timeout, default 120 second
    uint32_t shmCreateTimeout;          // func smem_shm_create timeout, default 120 second
    uint32_t controlOperationTimeout;   // control operation timeout, i.e. barrier, allgather, topology_can_reach etc, default 120 second
    bool startConfigStore;          // whether to start config store, default true
    uint32_t flags;                     // other flag, default 0
} smem_shm_config_t;


int32_t smem_shm_config_init(smem_shm_config_t *config);

/**
 * @brief Initialize shm library with global config store
 * all processes need to call this function before creating a shm object,
 * all processes will connect to a global config store with specified ipPort,
 * this function will finish when all processes connected or timeout;
 * the global config store will be used to exchange information about shm object and team
 *
 * @param gvaSpaceSize     [in] global virtual memory space size, all shm objects will be created on this space
 * @param configStoreIpPort[in] ipPort of config store, e.g. tcp:://ip:port
 * @param worldSize        [in] size of processes
 * @param rankId           [in] local rank id in world size
 * @param deviceId         [in] device npu id
 * @param config           [in] init config, see smem_shm_config_t
 * @return 0 if successfully
 */
int32_t smem_shm_init(const char *configStoreIpPort, uint32_t worldSize, uint32_t rankId,
    uint16_t deviceId, uint64_t gvaSpaceSize, smem_shm_config_t *config);

/**
 * @brief Un-initialize shm library with destroy all things
 *
 * @param flags            [in] optional flags, set to 0
 */
void smem_shm_uninit(uint32_t flags);

/**
 * @brief Create shm object peer by peer
 *
 * @param id               [in] id of the shm object
 * @param rankSize         [in] rank count
 * @param rankId           [in] my rank id
 * @param symmetricSize    [in] local memory contributed to the shm object, all ranks must the same size
 * @param dataOpType       [in] data operation engine type, i.e. MTE, SDMA, RDMA etc
 * @param flags            [in] optional flags
 * @param gva              [out] global virtual address created
 * @return shm object created if successful, null if failed, use @ref smem_get_last_error_msg to get last error message
 */
smem_shm_t smem_shm_create(uint32_t id, uint32_t rankSize, uint32_t rankId,
    uint64_t symmetricSize, smem_shm_data_op_type dataOpType, uint32_t flags, void **gva);

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
 * @param size              [in] extra context size (max is 16M)
 * @return 0 if successful
 */
int32_t smem_shm_set_extra_context(smem_shm_t handle, const void *context, uint32_t size);

/**
 * get global shm team
 * @param handle            [in] the shm object
 * @return global team
 */
smem_shm_team_t smem_shm_get_global_team(smem_shm_t handle);

/**
 * @brief Get local rank of a shm team
 *
 * @param team              [in] query shm team
 * @return local rank in the input team (if team is null, return UINT32_MAX)
 */
uint32_t smem_shm_team_get_rank(smem_shm_team_t team);

/**
 * @brief Get rank size of a shm team
 *
 * @param team              [in] query shm team
 * @return rank size in the input team (if team is null, return UINT32_MAX)
 */
uint32_t smem_shm_team_get_size(smem_shm_team_t team);

/**
 * @brief Do barrier on a shm team, using control network
 *
 * @param team              [in] barrier shm team
 * @return 0 if successful, other is error
 */
int32_t smem_shm_control_barrier(smem_shm_team_t team);

/**
 * @brief Do all gather on a shm tream, using control network
 *
 * @param team              [in] barrier shm team
 * @param sendBuf           [in] input data buf
 * @param sendSize          [in] input data buf size
 * @param recvBuf           [in] output data buf
 * @param recvSize          [in] output data buf size
 * @return 0 if successful
 */
int32_t smem_shm_control_allgather(smem_shm_team_t team, const char *sendBuf, uint32_t sendSize, char *recvBuf,
    uint32_t recvSize);

/**
 * @brief Query if remote rank can ranch
 *
 * @param handle            [in] shm object
 * @param remoteRank        [in] remote rank
 * @param reachInfo         [out] reach info, the set of the smem_shm_transport_capabilities_type
 * @return 0 if successful
 */
int32_t smem_shm_topology_can_reach(smem_shm_t handle, uint32_t remoteRank, uint32_t *reachInfo);

#ifdef __cplusplus
}
#endif

#endif // __MEMFABRIC_SMEM_SHM_H__
