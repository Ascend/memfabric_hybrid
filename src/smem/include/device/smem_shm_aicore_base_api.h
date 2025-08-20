/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef __MEMFABRIC_SMEM_AI_CORE_BASE_API_H__
#define __MEMFABRIC_SMEM_AI_CORE_BASE_API_H__

#include "smem_shm_aicore_base_meta.h"
#include "smem_shm_aicore_base_copy.h"
#include "smem_shm_aicore_base_rdma.h"

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
 * @brief Get qp info address which is set by function smem_shm_create from host side
 * @param shmemId           [in] shm object id, default 0
 */
SMEM_SHM_INLINE_AICORE __gm__ void* smem_shm_get_qp_info_address(uint32_t shmemId = 0);

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

/**
 * @brief Asynchronous RDMA Write function.
 *
 * @param srcDmaAddr             [in] source address in local HBM
 * @param destDmaAddr            [in] destination address in remote HBM
 * @param destRankId             [in] destination rank ID
 * @param qpIdx                  [in] QP index in multi-QP scenario (default 0 for single QP)
 * @param messageLen             [in] message length in Bytes
 * @param ubLocal64              [in] temporary UB local tensor of uint64_t used as workspace
 * @param ubLocal32              [in] temporary UB local tensor of uint32_t used as workspace
 */

template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_roce_write(__gm__ T* srcDmaAddr, __gm__ T* destDmaAddr, uint32_t destRankId,
                                                uint32_t qpIdx, uint64_t messageLen,
                                                AscendC::LocalTensor<uint64_t> ubLocal64,
                                                AscendC::LocalTensor<uint32_t> ubLocal32);
/**
 * @brief Asynchronous RDMA READ function.
 *
 * @param srcDmaAddr             [in] source address in remote HBM
 * @param destDmaAddr            [in] destination address in local HBM
 * @param srcRankId              [in] destination rank ID
 * @param qpIdx                  [in] QP index in multi-QP scenario (default 0 for single QP)
 * @param messageLen             [in] message length in Bytes
 * @param ubLocal64              [in] temporary UB local tensor of uint64_t used as workspace
 * @param ubLocal32              [in] temporary UB local tensor of uint32_t used as workspace
 */

template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_roce_read(__gm__ T* srcDmaAddr, __gm__ T* destDmaAddr, uint32_t srcRankId,
                                                uint32_t qpIdx, uint64_t messageLen,
                                                AscendC::LocalTensor<uint64_t> ubLocal64,
                                                AscendC::LocalTensor<uint32_t> ubLocal32);

SMEM_SHM_INLINE_AICORE void smem_shm_roce_qpinfo_test(__gm__ uint8_t* gva, uint32_t destRankId, uint32_t qpIdx);

template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_roce_pollcq_test(__gm__ T* srcDmaAddr, __gm__ T* destDmaAddr, uint32_t destRankId,
                                                uint32_t qpIdx, uint64_t messageLen,
                                                AscendC::LocalTensor<uint64_t> ubLocal64,
                                                AscendC::LocalTensor<uint32_t> ubLocal32, __gm__ uint8_t* gva);

#endif // __MEMFABRIC_SMEM_AI_CORE_BASE_API_H__
