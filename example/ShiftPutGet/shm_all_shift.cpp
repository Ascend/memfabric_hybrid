/**
 * @file add_custom.cpp
 *
 * Copyright (C) 2024. Huawei Technologies Co., Ltd. All rights reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */
#include "kernel_operator.h"
#include "smem_shm_aicore_api.h"

constexpr int32_t RANK_SIZE_MAX = 32;
constexpr int32_t BLOCK_LEN = SHMEM_ALIGN_SIZE / sizeof(int64_t);
constexpr int64_t FLAG_MAGIC = 3285742LL;


class KernelAllShift {
public:
    __aicore__ inline KernelAllShift() {}
    __aicore__ inline void Init(GM_ADDR gva, GM_ADDR local)
    {
        gvaSt = (__gm__ int64_t *)gva;
        inputGm = (__gm__ int64_t *)local;
        pipe.InitBuffer(bufQueue, BUFFER_NUM, SHMEM_ALIGN_SIZE);
    }
    __aicore__ inline void Process()
    {
        AscendC::LocalTensor<int64_t> bufTensor = bufQueue.AllocTensor<int64_t>();
        __ubuf__ int64_t *buf = (__ubuf__ int64_t *)bufTensor.address_.bufferAddr;
        smem_set_copy_ubuf(buf, SHMEM_ALIGN_SIZE);

        uint32_t rank = smem_shm_get_global_rank();
        uint32_t rankSize = smem_shm_get_global_rank_size();
	    uint64_t ss = smem_shm_get_symmetric_size();

        AscendC::printf("rank: %u  size: %u len:%llu\n", rank, rankSize, ss);

        smem_put_int64_t(gvaSt, inputGm, (rank + 1) % rankSize, BLOCK_LEN);
        buf[0] = rank;
        buf[1] = rankSize;
        smem_uput_int64_t(gvaSt + BLOCK_LEN, buf, rank, 2);
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
