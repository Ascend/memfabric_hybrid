/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __MEMFABRIC_SMEM_AI_CORE_COPY_H__
#define __MEMFABRIC_SMEM_AI_CORE_COPY_H__

#include "smem_shm_aicore_base_api.h"

__BLOCK_LOCAL__ __inline__ uint64_t gD2dUbuf;
__BLOCK_LOCAL__ __inline__ uint32_t gUbufSize;

template<typename T>
SMEM_INLINE_AICORE void smem_set_copy_ubuf(__ubuf__ T* srcUb, uint32_t size)
{
    gD2dUbuf = reinterpret_cast<uint64_t>(srcUb);
    gUbufSize = size;
}

SMEM_INLINE_AICORE void smem_unset_copy_ubuf()
{
    gD2dUbuf = 0;
    gUbufSize = 0;
}

template<typename T>
class SmemCopyGM2GM {
public:
    __aicore__ inline SmemCopyGM2GM() {}
    __aicore__ inline void Init(__gm__ T* dstGva, __gm__ T* srcGva, __ubuf__ T* tmpUb, uint32_t size, uint32_t ubSize)
    {
        inputGm = srcGva;
        outputGm = dstGva;
        transUb = tmpUb;
        dataSizeRemain = size;
        blockSize = SMEM_ALIGN_DOWN(ubSize, sizeof(T));
    }

    __aicore__ inline void Process()
    {
        int64_t i = 0;
        while (dataSizeRemain >= blockSize) {
            smem_copy_gm2ub(transUb, inputGm + i * blockSize / sizeof(T), blockSize);
            AscendC::SetFlag<AscendC::HardEvent::MTE2_MTE3>(EVENT_ID0);  // 3等2
            AscendC::WaitFlag<AscendC::HardEvent::MTE2_MTE3>(EVENT_ID0);
            smem_copy_ub2gm(outputGm + i * blockSize / sizeof(T), transUb, blockSize);
            AscendC::SetFlag<AscendC::HardEvent::MTE3_MTE2>(EVENT_ID1);
            AscendC::WaitFlag<AscendC::HardEvent::MTE3_MTE2>(EVENT_ID1);
            i += 1;
            dataSizeRemain -= blockSize;
        }
        if (dataSizeRemain > 0) {
            smem_copy_gm2ub(transUb, inputGm + i * blockSize / sizeof(T), dataSizeRemain);
            AscendC::SetFlag<AscendC::HardEvent::MTE2_MTE3>(EVENT_ID0);  // 3等2
            AscendC::WaitFlag<AscendC::HardEvent::MTE2_MTE3>(EVENT_ID0);
            smem_copy_ub2gm(outputGm + i * blockSize / sizeof(T), transUb, dataSizeRemain);
        }
    }

private:
    int64_t dataSizeRemain = 0;
    uint32_t blockSize = 0;
    __ubuf__ T* transUb = nullptr;
    __gm__ T* inputGm = nullptr;
    __gm__ T* outputGm = nullptr;
};

template <typename T>
SMEM_INLINE_AICORE void smem_copy_gm2gm(__gm__ T* dstGva, __gm__ T* srcGva, uint32_t size)
{
    SmemCopyGM2GM<T> cpKernel;
    cpKernel.Init(dstGva, srcGva, reinterpret_cast<__ubuf__ T*>(gD2dUbuf), size, gUbufSize);
    cpKernel.Process();
}

#endif // __MEMFABRIC_SMEM_AI_CORE_COPY_H__
