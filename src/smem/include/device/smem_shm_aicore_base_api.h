/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __MEMFABRIC_SMEM_AI_CORE_BASE_API_H__
#define __MEMFABRIC_SMEM_AI_CORE_BASE_API_H__

#include "smem_shm_aicore_base_meta.h"
#include "smem_shm_aicore_base_copy.h"

/**
 * @brief Get rank which is set by function smem_shm_create from host side
 * @param shmemId           [in] shm object id, default 0
 */
SMEM_SHM_INLINE_AICORE uint32_t smem_shm_get_global_rank(uint32_t shmemId = 0);

/**
 * @brief Get rank size which is set by function smem_shm_create from host side
 * @param shmemId           [in] shm object id, default 0
 */
SMEM_SHM_INLINE_AICORE uint32_t smem_shm_get_global_rank_size(uint32_t shmemId = 0);

/**
 * @brief Get symmetric size which is set by function smem_shm_create from host side
 * @param shmemId           [in] shm object id, default 0
 */
SMEM_SHM_INLINE_AICORE uint64_t smem_shm_get_symmetric_size(uint32_t shmemId = 0);

/**
 * @brief Get user extra context addr (context is set by function smem_shm_set_extra_context from host side)
 * @param shmemId           [in] shm object id, default 0
 */
SMEM_SHM_INLINE_AICORE __gm__ void* smem_shm_get_extra_context_addr(uint32_t shmemId = 0);

/**
 * @brief Get user extra context size (context is set by function smem_shm_set_extra_context from host side)
 * @param shmemId           [in] shm object id, default 0
 */
SMEM_SHM_INLINE_AICORE uint32_t smem_shm_get_extra_context_size(uint32_t shmemId = 0);

/**
 * @brief Copy data from ub to gva(global virtual address), executed by MTE engine
 *
 * @param dstGva            [in] global virtual address of destination data in hbm
 * @param srcUb             [in] address of source data in ub
 * @param size              [in] copy size
 */
template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_copy_ub2gm(__gm__ T* dstGva, __ubuf__ T* srcUb, uint32_t size);

/**
 * @brief Copy data from ub to gva(global virtual address) in Tensor, executed by MTE engine
 *
 * @param dstGva            [in] global virtual address of destination data in hbm
 * @param srcUb             [in] tensor of source data in ub
 * @param size              [in] copy size
 */
template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_copy_ub2gm(const AscendC::GlobalTensor<T> &dstGva,
    const AscendC::LocalTensor<T> &srcUb, uint32_t size);

/**
 * @brief Copy data from gva(global virtual address) to ub, executed by MTE engine
 *
 * @param dstUb             [in] address of destination data in ub
 * @param srcGva            [in] global virtual address of source data in hbm
 * @param size              [in] copy size
 */
template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_copy_gm2ub(__ubuf__ T* dstUb, __gm__ T* srcGva, uint32_t size);

/**
 * @brief Copy data from gva(global virtual address) to ub in Tensor, executed by MTE engine
 *
 * @param dstUb             [in] destination tensor in ub
 * @param srcGva            [in] source tensor in hbm with global virtual address
 * @param size              [in] copy size
 */
template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_copy_gm2ub(const AscendC::LocalTensor<T> &dstUb,
    const AscendC::GlobalTensor<T> &srcGva, uint32_t size);

#endif // __MEMFABRIC_SMEM_AI_CORE_BASE_API_H__
