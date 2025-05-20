/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __MEMFABRIC_SMEM_BM_H__
#define __MEMFABRIC_SMEM_BM_H__

#include <smem_bm_def.h>

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
 * @brief Initialize smem big memory library
 *
 * @param storeURL         [in] configure store url for control, e.g. tcp:://ip:port
 * @param worldSize        [in] number of guys participating
 * @param deviceId         [in] device id
 * @param config           [in] extract config
 * @return 0 if successful
 */
int32_t smem_bm_init(const char *storeURL, uint32_t worldSize, uint16_t deviceId, const smem_bm_config_t *config);

/**
 * @brief Un-initialize the smem big memory library
 *
 * @param flags            [in] optional flags, not used yet
 */
void smem_bm_uninit(uint32_t flags);

/**
 * @brief Get the rank id, assigned during initialize
 *
 * @return rank id if successful, UINT32_MAX is returned if failed
 */
uint32_t smem_bm_get_rank_id(void);

/**
 * @brief Create a big memory object locally after initialized
 *
 * @param id               [in] identity of the big memory object
 * @param memberSize       [in] number of guys participating, which should equal or less the world size
 * @param memType          [in] memory type, device HBM memory, host dram memory or both
 * @param dataOpType       [in] data operation type, SDMA or RoCE etc
 * @param localMemorySize  [in] the size of local memory contributes to big memory object
 * @param flags            [in] optional flags
 * @return Big memory object handle if successful, nullptr if failed
 */
smem_bm_t smem_bm_create(uint32_t id, uint32_t memberSize,
                         smem_bm_mem_type memType, smem_bm_data_op_type dataOpType,
                         uint64_t localMemorySize, uint32_t flags);

/**
 * @brief Destroy the Big Memory Object
 *
 * @param handle           [in] the big memory object to be destroyed
 */
void smem_bm_destroy(smem_bm_t handle);

/**
 * @brief Join to global Big Memory space actively, after this, we operate on the global space
 *
 * @param handle           [in] big memory object handle created by <i>smem_bm_create</i>
 * @param flags            [in] optional flags
 * @param localGvaAddress  [out] local part of the global virtual memory space
 * @return 0 if successful
 */
int32_t smem_bm_join(smem_bm_t handle, uint32_t flags, void **localGvaAddress);

/**
 * @brief Leave the global Big Memory space actively, after this, we cannot operate on the global space any more
 *
 * @param handle           [in] big memory object handle created by <i>smem_bm_create</i>
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t smem_bm_leave(smem_bm_t handle, uint32_t flags);

/**
 * @brief Get size of local memory that contributed to global space
 *
 * @param handle           [in] big memory object handle created by <i>smem_bm_create</i>
 * @return local memory size in bytes
 */
uint64_t smem_bm_get_local_mem_size(smem_bm_t handle);

/**
 * @brief Get peer gva by rank id
 *
 * @param handle           [in] big memory object handle created by <i>smem_bm_create</i>
 * @param peerRankId       [in] rank id of peer
 * @return ptr of peer gva
 */
void *smem_bm_ptr(smem_bm_t handle, uint16_t peerRankId);

/**
 * @brief Data operation on Big Memory object
 *
 * @param handle           [in] big memory object handle created by <i>smem_bm_create</i>
 * @param src              [in] source gva of data
 * @param dest             [in] target gva of data
 * @param size             [in] size of data to be copied
 * @param t                [in] copy type, L2G, G2L, G2H, H2G
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t smem_bm_copy(smem_bm_t handle, const void *src, void *dest, uint64_t size, smem_bm_copy_type t, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif  //__MEMFABRIC_SMEM_BM_H__