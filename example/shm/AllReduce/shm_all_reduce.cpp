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
#include "smem_shm_aicore_base_api.h"

constexpr uint64_t TOTAL_LENGTH = 16 * 2048;                            // total length of data
constexpr int32_t USE_CORE_NUM = 8;                                   // num of core used
constexpr int32_t RANK_SIZE_MAX = 32;
constexpr int32_t FLAG_OFFSET = SMEM_SHM_ALIGN_SIZE / sizeof(int64_t);
constexpr int64_t FLAG_MAGIC = 3285742LL;

SMEM_SHM_INLINE_AICORE void smem_shm_set_flag(__ubuf__ int64_t *ubAddr,
    __gm__ int64_t *gvaAddr, int64_t flagValue)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    *ubAddr = flagValue;
    AscendC::SetFlag<AscendC::HardEvent::S_MTE3>(EVENT_ID1);
    AscendC::WaitFlag<AscendC::HardEvent::S_MTE3>(EVENT_ID1);
    smem_shm_copy_ub2gm(gvaAddr, ubAddr, sizeof(int64_t));
    AscendC::PipeBarrier<PIPE_ALL>();
}

SMEM_SHM_INLINE_AICORE void smem_shm_wait_flag(__ubuf__ int64_t *ubAddr,
    __gm__ int64_t *gvaAddr, int64_t expectValue)
{
    while (true) {
        AscendC::PipeBarrier<PIPE_ALL>();
        smem_shm_copy_gm2ub(ubAddr, gvaAddr, sizeof(int64_t));
        AscendC::SetFlag<AscendC::HardEvent::MTE2_S>(EVENT_ID0);
        AscendC::WaitFlag<AscendC::HardEvent::MTE2_S>(EVENT_ID0);
        if (*ubAddr == expectValue) {
            break;
        }
    }
}

class KernelAllReduce {
public:
    __aicore__ inline KernelAllReduce() {}
    __aicore__ inline void Init(GM_ADDR gva, uint64_t spaceOffset,
        uint64_t flagOffset, uint32_t rankId, uint32_t rankSize)
    {
        uint32_t coreOffset = USE_CORE_NUM * rankId + AscendC::GetBlockIdx(); // flag offset of each core
        
        blockLen = TOTAL_LENGTH / rankSize / USE_CORE_NUM; // length computed of each core
        rankNum = rankSize;
        rank = rankId;

        for (uint32_t i = 0; i < rankSize; i++) {
            uint64_t shmVa = (uint64_t)gva + spaceOffset * i;
            uint64_t startAddr = shmVa + flagOffset + coreOffset * blockLen * sizeof(half);

            dataGm[i].SetGlobalBuffer((__gm__ half *)startAddr, blockLen);
            flagAddr[i] = (__gm__ int64_t *)(shmVa + AscendC::GetBlockIdx() * SMEM_SHM_ALIGN_SIZE * 2);
        }

        pipe.InitBuffer(inQueue, BUFFER_NUM, blockLen * sizeof(half));
        pipe.InitBuffer(calQueue, BUFFER_NUM, blockLen * sizeof(half));
        pipe.InitBuffer(outQueue, BUFFER_NUM, blockLen * sizeof(half));
        pipe.InitBuffer(flagQueue, BUFFER_NUM, sizeof(int64_t) * SMEM_SHM_ALIGN_SIZE);
    }
    __aicore__ inline void Process()
    {
        AscendC::LocalTensor<int64_t> flagTensor = flagQueue.AllocTensor<int64_t>();
        __ubuf__ int64_t *ubFlag = (__ubuf__ int64_t *)flagTensor.address_.bufferAddr;
        smem_shm_set_flag(ubFlag, flagAddr[rank], FLAG_MAGIC); // local set flag, default is 0
        for (uint32_t rk = 0; rk < rankNum; rk++) {
            smem_shm_wait_flag(ubFlag, flagAddr[rk], FLAG_MAGIC);
        }

        // data ready, start to calc
        AscendC::LocalTensor<half> inTensor = inQueue.AllocTensor<half>();
        AscendC::LocalTensor<half> calTensor = calQueue.AllocTensor<half>();
        AscendC::LocalTensor<half> outTensor = outQueue.AllocTensor<half>();

        AscendC::DataCopy(calTensor, dataGm[0], blockLen);
        AscendC::PipeBarrier<PIPE_ALL>();
        
        uint64_t copyMask = 128; // = 256 / sizeof(half)
        for (uint32_t rk = 1; rk < rankNum; rk++) {
            AscendC::DataCopy(inTensor, dataGm[rk], blockLen);
            AscendC::PipeBarrier<PIPE_ALL>();
            AscendC::Add(outTensor, inTensor, calTensor, blockLen);
            AscendC::PipeBarrier<PIPE_ALL>();
            if (rk + 1 < rankNum) {
                AscendC::Copy(calTensor, outTensor, copyMask, blockLen / copyMask, { 1, 1, 8, 8 });
                AscendC::PipeBarrier<PIPE_ALL>();
            }
        }

        inQueue.FreeTensor(inTensor);
        calQueue.FreeTensor(calTensor);

        for (uint32_t rk = 0; rk < rankNum; rk++) {
            AscendC::DataCopy(dataGm[rk], outTensor, blockLen);
        }

        AscendC::PipeBarrier<PIPE_ALL>();
        smem_shm_set_flag(ubFlag, flagAddr[rank] + FLAG_OFFSET, FLAG_MAGIC); // local set flag, default is 0
        for (uint32_t rk = 0; rk < rankNum; rk++) {
            smem_shm_wait_flag(ubFlag, flagAddr[rk] + FLAG_OFFSET, FLAG_MAGIC);
        }

        outQueue.FreeTensor(outTensor);
        flagQueue.FreeTensor(flagTensor);
    }

private:
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, BUFFER_NUM> inQueue;
    AscendC::TQue<AscendC::TPosition::VECCALC, BUFFER_NUM> calQueue;
    AscendC::TQue<AscendC::TPosition::VECOUT, BUFFER_NUM> outQueue;
    AscendC::TQue<AscendC::TPosition::VECIN, BUFFER_NUM> flagQueue;
    AscendC::GlobalTensor<half> dataGm[RANK_SIZE_MAX];
    __gm__ int64_t *flagAddr[RANK_SIZE_MAX];
    uint32_t blockLen = 0;
    uint32_t rankNum = 0;
    uint32_t rank = 0;
};

extern "C" __global__ __aicore__ void shm_all_reduce(GM_ADDR gva, uint64_t spaceOffset,
    uint64_t flagOffset, uint32_t rankId, uint32_t rankSize)
{
    KernelAllReduce op;
    op.Init(gva, spaceOffset, flagOffset, rankId, rankSize);
    op.Process();
}

void shm_all_reduce_do(uint32_t coreDim, void* stream, uint8_t* gva, uint64_t spaceOffset,
    uint64_t flagOffset, uint32_t rankId, uint32_t rankSize)
{
    shm_all_reduce<<<coreDim, nullptr, stream>>>(gva, spaceOffset, flagOffset, rankId, rankSize);
}
