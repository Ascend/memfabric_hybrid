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
#ifndef __MEMFABRIC_SMEM_AI_CORE_BASE_COPY_H__
#define __MEMFABRIC_SMEM_AI_CORE_BASE_COPY_H__

#include "smem_shm_aicore_base_define.h"

template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_copy_ub2gm(__gm__ T *dstGva, __ubuf__ T *srcUb, uint32_t size,
                                                bool enableL2 = true)
{
    ASCENDC_ASSERT((dstGva != nullptr), "input gva is null");

    AscendC::LocalTensor<T> ubTensor;
    AscendC::GlobalTensor<T> gmTensor;
    AscendC::DataCopyExtParams dataCopyParams(1, size, 0, 0, 0);
    ubTensor.address_.logicPos = static_cast<uint8_t>(AscendC::TPosition::VECIN);
    ubTensor.address_.bufferAddr = reinterpret_cast<uint64_t>(srcUb);
    gmTensor.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(dstGva));
    if (!enableL2) {
        gmTensor.SetL2CacheHint(AscendC::CacheMode::CACHE_MODE_DISABLE);
    }
    AscendC::DataCopyPad(gmTensor, ubTensor, dataCopyParams);
}

template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_copy_ub2gm(const AscendC::GlobalTensor<T> &dstGva,
                                                const AscendC::LocalTensor<T> &srcUb, uint32_t size)
{
    AscendC::DataCopyExtParams dataCopyParams(1, size, 0, 0, 0);
    AscendC::DataCopyPad(dstGva, srcUb, dataCopyParams);
}

template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_copy_ub2gm(__gm__ T *dstGva, __ubuf__ T *srcUb,
                                                AscendC::DataCopyExtParams &copyParams)
{
    ASCENDC_ASSERT((dstGva != nullptr), "input gva is null");

    AscendC::LocalTensor<T> ubTensor;
    AscendC::GlobalTensor<T> gmTensor;
    ubTensor.address_.logicPos = static_cast<uint8_t>(AscendC::TPosition::VECIN);
    ubTensor.address_.bufferAddr = reinterpret_cast<uint64_t>(srcUb);
    gmTensor.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(dstGva));
    AscendC::DataCopyPad(gmTensor, ubTensor, copyParams);
}

template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_copy_ub2gm(const AscendC::GlobalTensor<T> &dstGva,
                                                const AscendC::LocalTensor<T> &srcUb,
                                                AscendC::DataCopyExtParams &copyParams)
{
    AscendC::DataCopyPad(dstGva, srcUb, copyParams);
}

template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_copy_gm2ub(__ubuf__ T *dstUb, __gm__ T *srcGva, uint32_t size,
                                                bool enableL2 = true)
{
    ASCENDC_ASSERT((srcGva != nullptr), "input gva is null");
    AscendC::LocalTensor<T> ubTensor;
    AscendC::GlobalTensor<T> gmTensor;
    AscendC::DataCopyExtParams dataCopyParams(1, size, 0, 0, 0);
    ubTensor.address_.logicPos = static_cast<uint8_t>(AscendC::TPosition::VECIN);
    ubTensor.address_.bufferAddr = reinterpret_cast<uint64_t>(dstUb);
    gmTensor.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(srcGva));
    if (!enableL2) {
        gmTensor.SetL2CacheHint(AscendC::CacheMode::CACHE_MODE_DISABLE);
    }
    AscendC::DataCopyPadExtParams<T> padParams;
    AscendC::DataCopyPad(ubTensor, gmTensor, dataCopyParams, padParams);
}

template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_copy_gm2ub(const AscendC::LocalTensor<T> &dstUb,
                                                const AscendC::GlobalTensor<T> &srcGva, uint32_t size)
{
    AscendC::DataCopyExtParams dataCopyParams(1, size, 0, 0, 0);
    AscendC::DataCopyPadExtParams<T> padParams;
    AscendC::DataCopyPad(dstUb, srcGva, dataCopyParams, padParams);
}

template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_copy_gm2ub(__ubuf__ T *dstUb, __gm__ T *srcGva,
                                                AscendC::DataCopyExtParams &copyParams)
{
    ASCENDC_ASSERT((srcGva != nullptr), "input gva is null");
    AscendC::LocalTensor<T> ubTensor;
    AscendC::GlobalTensor<T> gmTensor;
    ubTensor.address_.logicPos = static_cast<uint8_t>(AscendC::TPosition::VECIN);
    ubTensor.address_.bufferAddr = reinterpret_cast<uint64_t>(dstUb);
    gmTensor.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(srcGva));
    AscendC::DataCopyPadExtParams<T> padParams;
    AscendC::DataCopyPad(ubTensor, gmTensor, copyParams, padParams);
}

template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_copy_gm2ub(const AscendC::LocalTensor<T> &dstUb,
                                                const AscendC::GlobalTensor<T> &srcGva,
                                                AscendC::DataCopyExtParams &copyParams)
{
    AscendC::DataCopyPadExtParams<T> padParams;
    AscendC::DataCopyPad(dstUb, srcGva, copyParams, padParams);
}

#endif // __MEMFABRIC_SMEM_AI_CORE_BASE_COPY_H__
