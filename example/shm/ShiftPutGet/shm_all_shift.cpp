/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "kernel_operator.h"
#include "smem_shm_aicore_base_api.h"

constexpr int32_t RANK_SIZE_MAX = 32;
constexpr int32_t BLOCK_LEN = SMEM_SHM_ALIGN_SIZE / sizeof(int64_t);
constexpr int64_t FLAG_MAGIC = 3285742LL;

__BLOCK_LOCAL__ __inline__ uint64_t gD2dUbuf;
__BLOCK_LOCAL__ __inline__ uint32_t gUbufSize;

template<typename T>
SMEM_SHM_INLINE_AICORE void smem_shm_set_copy_ubuf(__ubuf__ T* srcUb, uint32_t size)
{
    gD2dUbuf = reinterpret_cast<uint64_t>(srcUb);
    gUbufSize = size;
}

SMEM_SHM_INLINE_AICORE void smem_shm_unset_copy_ubuf()
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
        blockSize = SMEM_SHM_ALIGN_DOWN(ubSize, sizeof(T));
    }

    __aicore__ inline void Process()
    {
        int64_t i = 0;
        while (dataSizeRemain >= blockSize) {
            smem_shm_copy_gm2ub(transUb, inputGm + i * blockSize / sizeof(T), blockSize);
            AscendC::SetFlag<AscendC::HardEvent::MTE2_MTE3>(EVENT_ID0);  // 3等2
            AscendC::WaitFlag<AscendC::HardEvent::MTE2_MTE3>(EVENT_ID0);
            smem_shm_copy_ub2gm(outputGm + i * blockSize / sizeof(T), transUb, blockSize);
            AscendC::SetFlag<AscendC::HardEvent::MTE3_MTE2>(EVENT_ID1);
            AscendC::WaitFlag<AscendC::HardEvent::MTE3_MTE2>(EVENT_ID1);
            i += 1;
            dataSizeRemain -= blockSize;
        }
        if (dataSizeRemain > 0) {
            smem_shm_copy_gm2ub(transUb, inputGm + i * blockSize / sizeof(T), dataSizeRemain);
            AscendC::SetFlag<AscendC::HardEvent::MTE2_MTE3>(EVENT_ID0);  // 3等2
            AscendC::WaitFlag<AscendC::HardEvent::MTE2_MTE3>(EVENT_ID0);
            smem_shm_copy_ub2gm(outputGm + i * blockSize / sizeof(T), transUb, dataSizeRemain);
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
SMEM_SHM_INLINE_AICORE void smem_shm_copy_gm2gm(__gm__ T* dstGva, __gm__ T* srcGva, uint32_t size)
{
    SmemCopyGM2GM<T> cpKernel;
    cpKernel.Init(dstGva, srcGva, reinterpret_cast<__ubuf__ T*>(gD2dUbuf), size, gUbufSize);
    cpKernel.Process();
}

SMEM_SHM_INLINE_AICORE void smem_shm_put_int64(__gm__ int64_t *gva,
    __gm__ int64_t *src, uint32_t rank, uint32_t count)
{
    uint64_t offset = smem_shm_get_symmetric_size();
    uint64_t dst = reinterpret_cast<uint64_t>(gva) + offset * rank;
    smem_shm_copy_gm2gm<int64_t>(reinterpret_cast<__gm__ int64_t *>(dst), src, count * sizeof(int64_t));
}

SMEM_SHM_INLINE_AICORE void smem_shm_uput_int64(__gm__ int64_t *gva,
    __ubuf__ int64_t *src, uint32_t rank, uint32_t count)
{
    uint64_t offset = smem_shm_get_symmetric_size();
    uint64_t dst = reinterpret_cast<uint64_t>(gva) + offset * rank;
    smem_shm_copy_ub2gm(reinterpret_cast<__gm__ int64_t *>(dst), src, count * sizeof(int64_t));
}

class KernelAllShift {
public:
    __aicore__ inline KernelAllShift() {}
    __aicore__ inline void Init(GM_ADDR gva, GM_ADDR local)
    {
        gvaSt = (__gm__ int64_t *)gva;
        inputGm = (__gm__ int64_t *)local;
        pipe.InitBuffer(bufQueue, BUFFER_NUM, SMEM_SHM_ALIGN_SIZE);
    }
    __aicore__ inline void Process()
    {
        AscendC::LocalTensor<int64_t> bufTensor = bufQueue.AllocTensor<int64_t>();
        __ubuf__ int64_t *buf = (__ubuf__ int64_t *)bufTensor.address_.bufferAddr;
        smem_shm_set_copy_ubuf(buf, SMEM_SHM_ALIGN_SIZE);

        uint32_t rank = smem_shm_get_global_rank();
        uint32_t rankSize = smem_shm_get_global_rank_size();
	    uint64_t ss = smem_shm_get_symmetric_size();
        smem_shm_put_int64(gvaSt, inputGm, (rank + 1) % rankSize, BLOCK_LEN);
        buf[0] = rank;
        buf[1] = rankSize;
        smem_shm_uput_int64(gvaSt + BLOCK_LEN, buf, rank, 2);
        bufQueue.FreeTensor(bufTensor);
    }

private:
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, BUFFER_NUM> bufQueue;
    __gm__ int64_t *gvaSt, *inputGm;
};

extern "C" __global__ __aicore__ void shm_all_shift(GM_ADDR gva, GM_ADDR localInput)
{
    KernelAllShift op;
    op.Init(gva, localInput);
    op.Process();
}

void shm_all_shift_do(void* stream, uint8_t* gva, int64_t *localInput)
{
    shm_all_shift<<<1, nullptr, stream>>>(gva, localInput);
}
