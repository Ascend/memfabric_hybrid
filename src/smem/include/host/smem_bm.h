/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * @param storeURL         [in] configure store url for control, e.g. tcp:://ip:port
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
 *
 * @param handle        [in] Big Memory object handle created by smem_bm_create
 * @param memType       [in] memory type, device or host
 * @return local memory size in bytes
 */
uint64_t smem_bm_get_local_mem_size_by_mem_type(smem_bm_t handle, smem_bm_mem_type memType);

/**
 *
 * @param handle        [in] Big Memory object handle created by smem_bm_create
 * @param memType       [in] memory type, device or host
 * @param peerRankId    [in] rank id of peer
 * @return memory ptr of peer gva
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
 * @param src              [in] source gva of data
 * @param dest             [in] target gva of data
 * @param size             [in] size of data to be copied
 * @param t                [in] copy type, L2G, G2L, G2H, H2G
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t smem_bm_copy(smem_bm_t handle, const void *src, void *dest, uint64_t size, smem_bm_copy_type t, uint32_t flags);

/**
 * @brief Data copy on Big Memory object, several copy types supported:
 * The size of the dest address must be continuous heigth * width
 * L2G: local memory to global space
 * G2L: global space to local memory
 * G2H: global space to host memory
 * H2G: host memory to global space
 *
 * @param handle           [in] Big Memory object handle created by <i>smem_bm_create</i>
 * @param params.src       [in] source gva of data
 * @param params.spitch    [in] pitch of source memory
 * @param params.dest      [in] target gva of data
 * @param params.dpitch    [in] pitch of destination memory
 * @param params.width     [in] width of matrix transfer
 * @param params.heigth    [in] height of matrix transfer
 * @param t                [in] copy type, L2G, G2L, G2H, H2G
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t smem_bm_copy_2d(smem_bm_t handle, smem_copy_2d_params *params, smem_bm_copy_type t, uint32_t flags);


int32_t smem_bm_copy_batch(smem_bm_t handle, smem_batch_copy_params *params, smem_bm_copy_type t, uint32_t flags);
/**
 * @brief wait async copy finish
 * @return 0 if successful
 */
int32_t smem_bm_wait(smem_bm_t handle);

/**
 * @brief register hbm mem, support sdma or drma
 *
 * @param addr              [in] register addr
 * @param size              [in] register size
 */
int32_t smem_bm_register_user_mem(smem_bm_t handle, uint64_t addr, uint64_t size);

int32_t smem_bm_register_layer_mem(const uint64_t *addrs, const uint64_t *sizes, uint64_t layer, uint64_t num);

/**
 * @brief This command is used to register host memory to device.
 * @param addr             [in] requested the src share memory pointer, srcPtr must be page aligned.
 * @param size             [in] requested byte size.
 * @param dest             [out] pointer that stores the address of the allocated dst memory pointer.
 * @return 0 if successful
 */
int32_t smem_bm_register_host_mem(uint64_t addr, uint64_t size, uint64_t *dest);

/**
 * @brief This command is used to unregister host memory to device.
 * @param addr             [in] Requested the src share memory pointer.
 * @return 0 if successful
 */
int32_t smem_bm_unregister_host_mem(uint64_t addr);

/**
 * @brief alloc mem
 * @param size              [in] alloc size
 * @param memType           [in] mem type, only support SMEM_MEM_TYPE_HOST now
 * @param flags             [in] optional flags
 * @return addr if successful
 */
void *smem_bm_mem_malloc(uint64_t size, smem_bm_mem_type memType, uint64_t flags);

/**
 * @brief free mem
 * @param addr             [in] address
 */
void smem_bm_mem_free(void *addr);

#ifdef __cplusplus
}
#endif

#endif  //__MEMFABRIC_SMEM_BM_H__