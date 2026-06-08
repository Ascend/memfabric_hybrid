/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "zbal_kernel_base.h"
#include "zbal_kernel_allgather.h"

constexpr uint32_t SMALL_DATA_SIZE = 0 * 7168;
constexpr uint32_t INPUT_BUFFER_BOTH = 0;
constexpr uint32_t INPUT_ONLY = 1;
constexpr uint32_t BUFFER_ONLY = 2;

struct ArParallelStrategy {
    bool aivNumLtGroupSize{false};
    uint32_t startRank{0};
    uint32_t endRank{0};
    uint32_t startNotifyRank{0};
    uint32_t endNotifyRank{0};
    uint32_t corePerRank{0};
    uint32_t coreRankIdx{0};
};

template<typename T>
class ZBALAllReduceKernel : public ZBALBaseKernel {
public:
    ZBAL_KERNEL ZBALAllReduceKernel() {}

    ZBAL_KERNEL void Init(GM_ADDR x, GM_ADDR y, GM_ADDR metaAddr, GM_ADDR buf, uint32_t totalElems,
                          uint32_t atomicOp, uint64_t waitSymbol)
    {
        this->atomicOp = atomicOp;
        this->totalElems = totalElems;
        this->comm = reinterpret_cast<__gm__ CommGroupInfo *>(metaAddr);
        this->rank = comm->myGroupRank;
        this->myGroupRank = comm->myGroupRank;
        this->groupSize = comm->groupSize;
        this->waitSymbol = waitSymbol;
        this->input = x;
        this->output = y;
        this->buffer = buf;
        this->memSize = comm->localDeviceMemSize;
        this->worldRanks = reinterpret_cast<__gm__ uint16_t *>(comm->peerGroupRank2WorldRank);

        this->aivNum = AscendC::GetBlockNum();
        this->aivIndex = AscendC::GetBlockIdx();

        // |------input------|------buffer------|------inputflag------|------bufferflag------|
        this->addrOffset = groupSize * ZBAL_FLAG_SIZE;
        this->exchangeInputStart = reinterpret_cast<__gm__ uint64_t *>(comm->myAddressExchangeGva);
        this->exchangeBufferStart = exchangeInputStart + addrOffset;
        this->exchangeInputFlagStart = exchangeBufferStart + addrOffset;
        this->exchangeBufferFlagStart = exchangeInputFlagStart + addrOffset;
        this->exchangeMetaSize = 4 * addrOffset;

        InitParallelStrategy();

        this->dataOpType = comm->dataOpType;
        ZBALBaseKernel::Init();
    }

    ZBAL_KERNEL void InitParallelStrategy()
    {
        meta.aivNumLtGroupSize = aivNum < groupSize;
        // 核数小于集群数场景，前remain个核负责avg+1张卡的数据搬运，其他核负责avg张卡的数据搬运
        if (meta.aivNumLtGroupSize) {
            uint16_t avg = groupSize / aivNum;
            uint16_t remain = groupSize % aivNum;
            uint16_t extra = aivIndex < remain;
            meta.startRank = aivIndex * avg + (extra ? aivIndex : remain);
            meta.endRank = meta.startRank + avg + extra;
            meta.startNotifyRank = meta.startRank;
            meta.endNotifyRank = meta.endRank;
        } else {
            // 核数大于集群数场景，前remain个组负责一张卡的数据搬运，每个组avg+1个核并行搬运，其他组avg个核并行搬运一张卡
            uint16_t avg = aivNum / groupSize;
            uint16_t remain = aivNum % groupSize;
            uint16_t extra = avg + 1;

            bool extraPart = aivIndex < remain * extra;
            if (extraPart) {
                meta.corePerRank = extra;
                meta.coreRankIdx = aivIndex % extra;
                meta.startRank = aivIndex / extra;
            } else {
                uint32_t offset = aivIndex - remain * extra;
                meta.corePerRank = avg;
                meta.coreRankIdx = offset % avg;
                meta.startRank = remain + offset / avg;
            }
            meta.endRank = meta.startRank + 1;
            if (aivIndex < groupSize) {
                meta.startNotifyRank = aivIndex;
                meta.endNotifyRank = aivIndex + 1;
            }
        }
    }

    ZBAL_KERNEL void Process()
    {
#ifdef __DAV_C220_VEC__
        ClearExchange(exchangeInputStart, exchangeMetaSize);
        BarrierAll();

        if (totalElems > groupSize) {
            ProcessElemsGtGroupSize();
        } else {
            ProcessElemsLtGroupSize();
        }
#endif
    }

private:
    ZBAL_KERNEL void ProcessElemsLtGroupSize()
    {
#ifdef __DAV_C220_VEC__
        ZBAL_PROF_START(comm, ZBAL_PROF_ALLREDUCE_KERNEL_ALL);
        if (meta.aivNumLtGroupSize) {
            ZBAL_PROF_START(comm, ZBAL_PROF_ALLREDUCE_SCATTER_REDUCE);
            if (aivIndex == 0) {
                xGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(input), totalElems);
                buffGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(buffer), totalElems);
                CpGM2GM(xGm, buffGm, totalElems);
            }
            AscendC::SyncAll<true>();

            InitDataAddrAndFlag(INPUT_ONLY);
            for (auto srcRank = meta.startRank; srcRank < meta.endRank; srcRank++) {
                WaitFlagAndPtr(srcRank, INPUT_ONLY);
                xGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(inputPtr), totalElems);
                buffGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(buffer), totalElems);
                if (srcRank != rank) {
                    CpGM2GM(xGm, buffGm, totalElems, true, atomicOp);
                }
            }
            ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLREDUCE_SCATTER_REDUCE);
        } else {
            InitDataAddrAndFlag(INPUT_BUFFER_BOTH);
            WaitFlagAndPtr(aivIndex, INPUT_BUFFER_BOTH);

            ZBAL_PROF_START(comm, ZBAL_PROF_ALLREDUCE_SCATTER_REDUCE);
            xGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(inputPtr), totalElems);
            buffGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(buffer), totalElems);
            if (aivIndex == rank) {
                CpGM2GM(xGm, buffGm, totalElems);
                AscendC::SyncAll<true>();
            } else {
                AscendC::SyncAll<true>();
                CpGM2GM(xGm, buffGm, totalElems, true, atomicOp);
            }
            ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLREDUCE_SCATTER_REDUCE);
        }

        BarrierAll();

        ZBAL_PROF_START(comm, ZBAL_PROF_ALLREDUCE_ALLGATHER);
        buffGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(buffer), totalElems);
        yGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(output), totalElems);
        CpGM2GM(buffGm, yGm, totalElems);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLREDUCE_ALLGATHER);

        BarrierAll();
        ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLREDUCE_KERNEL_ALL);
#endif
    }

    ZBAL_KERNEL void ProcessElemsGtGroupSize()
    {
#ifdef __DAV_C220_VEC__
        ZBAL_PROF_START(comm, ZBAL_PROF_ALLREDUCE_KERNEL_ALL);

        // 先做reducescatter，将reducescatter结果存入buffer区
        ZBAL_PROF_START(comm, ZBAL_PROF_ALLREDUCE_SCATTER_REDUCE);
        ProcessRs();
        ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLREDUCE_SCATTER_REDUCE);

        // 恢复tensor到完整范围, 搬运数据buffer -> output
        ZBAL_PROF_START(comm, ZBAL_PROF_ALLREDUCE_ALLGATHER);
        pipe.Reset();
        pipe.InitBuffer(bindQueue, 1, UB_DMA_MAX_SIZE);

        uint32_t slice = totalElems / groupSize;
        bool regular = totalElems % groupSize == 0;

        // 均分且数据量较大，走双ring实现，天然支持集群数大于核数
        if (regular && slice >= SMALL_DATA_SIZE) {
            ZBALAllGatherBigKernel op;
            op.Init<T>(buffer, output, reinterpret_cast<GM_ADDR>(comm), slice, --waitSymbol);
            op.Process<T>();
        } else {
            ProcessAg();
        }
        ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLREDUCE_ALLGATHER);

        ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLREDUCE_KERNEL_ALL);
#endif
    }

    ZBAL_KERNEL void ProcessRs()
    {
        // 目前选择使用ring allreduce算法进行实现，要考虑totalElems不整除groupSize场景时reducescatter和allgather的例外场景，前N-1张卡分配slice，最后一张卡分配剩余部分
        // slice代表前N-1张卡切分的数据量, N-1号卡需要修正, 因此使用elements代表拷贝数据量, 使用slice计算偏移
        uint32_t slice = totalElems / groupSize;
        uint32_t elements = rank != groupSize - 1 ? slice : totalElems - (groupSize - 1) * slice;

        // step1. 所有核先参与本地搬运
        uint32_t numPerCoreLocal = elements / aivNum;
        uint32_t xOffsetLocal = rank * slice + aivIndex * numPerCoreLocal;
        uint32_t yOffsetLocal = aivIndex * numPerCoreLocal;
        if (aivIndex == aivNum - 1) {
            numPerCoreLocal = elements - (aivNum - 1) * numPerCoreLocal;
        }

        ZBAL_PROF_START(comm, ZBAL_PROF_REDUCESCATTER_LOCAL_COPY);
        xGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(input) + xOffsetLocal, numPerCoreLocal);
        buffGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(buffer) + yOffsetLocal, numPerCoreLocal);
        CpGM2GM(xGm, buffGm, numPerCoreLocal);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_REDUCESCATTER_LOCAL_COPY);
        AscendC::SyncAll<true>();

        // step2. 分核与交换地址
        InitDataAddrAndFlag(INPUT_ONLY);

        // step3. 分核与并行拷贝
        uint32_t xOffset = 0;
        uint32_t yOffset = 0;
        uint32_t numPerCore = 0;
        if (meta.aivNumLtGroupSize) {
            numPerCore = elements;
            xOffset = rank * slice;
            yOffset = 0;
        } else {
            numPerCore = elements / meta.corePerRank;
            xOffset = rank * slice + meta.coreRankIdx * numPerCore;
            yOffset = meta.coreRankIdx * numPerCore;
            if (meta.coreRankIdx == meta.corePerRank - 1) {
                numPerCore = elements - (meta.corePerRank - 1) * numPerCore;
            }
        }

        for (auto srcRank = meta.startRank; srcRank < meta.endRank; srcRank++) {
            WaitFlagAndPtr(srcRank, INPUT_ONLY);
            xGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(inputPtr) + xOffset, numPerCore);
            buffGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(buffer) + yOffset, numPerCore);
            if (srcRank != rank) {
                ZBAL_PROF_START(comm, ZBAL_PROF_REDUCESCATTER_COPY);
                CpGM2GM(xGm, buffGm, numPerCore, true, atomicOp);
                ZBAL_PROF_STOP(comm, ZBAL_PROF_REDUCESCATTER_COPY);
            }
        }

        BarrierAll();
    }

    ZBAL_KERNEL void ProcessAg()
    {
        uint32_t slice = totalElems / groupSize;
        uint32_t lastRank = groupSize - 1;
        uint32_t lastSlice = totalElems - lastRank * slice;

        // step1. 分核与交换地址
        InitDataAddrAndFlag(BUFFER_ONLY);

        // step2. 分核与并行拷贝
        if (meta.aivNumLtGroupSize) {
            for (auto srcRank = meta.startRank; srcRank < meta.endRank; srcRank++) {
                uint32_t yOffset = srcRank * slice;
                uint32_t elements = srcRank != lastRank ? slice : lastSlice;
                WaitFlagAndPtr(srcRank, BUFFER_ONLY);
                buffGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(exchangeBufPtr), elements);
                yGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(output) + yOffset, elements);
                CpGM2GM(buffGm, yGm, elements);
            }
        } else {
            uint32_t elements = meta.startRank != lastRank ? slice : lastSlice;
            uint32_t numPerCore = elements / meta.corePerRank;
            uint32_t xOffset = meta.coreRankIdx * numPerCore;
            uint32_t yOffset = meta.startRank * slice + xOffset;
            if (meta.coreRankIdx == meta.corePerRank - 1) {
                numPerCore = elements - (meta.corePerRank - 1) * numPerCore;
            }

            WaitFlagAndPtr(meta.startRank, BUFFER_ONLY);
            buffGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(exchangeBufPtr) + xOffset, numPerCore);
            yGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(output) + yOffset, numPerCore);
            CpGM2GM(buffGm, yGm, numPerCore);
        }

        BarrierAll();
    }

    ZBAL_KERNEL void InitDataAddrAndFlag(uint32_t op)
    {
        ZBAL_PROF_START(comm, ZBAL_PROF_EXCHANGE_ADDR);
        for (auto dsrRank = meta.startNotifyRank; dsrRank < meta.endNotifyRank; dsrRank++) {
            if (op != BUFFER_ONLY) {
                uint64_t inputddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(input));
                auto dstInput = ZbalPtr(exchangeInputStart, dsrRank);
                ZBALSetFlag(dstInput, inputddr, rank);
                AscendC::PipeBarrier<PIPE_ALL>();

                auto inputFlag = ZbalPtr(exchangeInputFlagStart, dsrRank);
                ZBALSetFlag(inputFlag, waitSymbol, rank);
                AscendC::PipeBarrier<PIPE_ALL>();
            }

            if (op != INPUT_ONLY) {
                uint64_t bufferAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(buffer));
                auto dstBuffer = ZbalPtr(exchangeBufferStart, dsrRank);
                ZBALSetFlag(dstBuffer, bufferAddr, rank);
                AscendC::PipeBarrier<PIPE_ALL>();

                auto bufferFlag = ZbalPtr(exchangeBufferFlagStart, dsrRank);
                ZBALSetFlag(bufferFlag, waitSymbol, rank);
                AscendC::PipeBarrier<PIPE_ALL>();
            }
        }
        ZBAL_PROF_STOP(comm, ZBAL_PROF_EXCHANGE_ADDR);
    }

    ZBAL_KERNEL void WaitFlagAndPtr(uint32_t srcRank, uint32_t op)
    {
        ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_FLAG);
        if (op != BUFFER_ONLY) {
            ZBALWaitFlag(exchangeInputFlagStart, waitSymbol, srcRank);

            uint64_t inputAddr = ZBALGetFlag(exchangeInputStart, srcRank);
            this->inputPtr = reinterpret_cast<GM_ADDR>(static_cast<uintptr_t>(inputAddr));
        }

        if (op != INPUT_ONLY) {
            ZBALWaitFlag(exchangeBufferFlagStart, waitSymbol, srcRank);

            uint64_t exchangeBufAddr = ZBALGetFlag(exchangeBufferStart, srcRank);
            this->exchangeBufPtr = reinterpret_cast<GM_ADDR>(static_cast<uintptr_t>(exchangeBufAddr));
        }
        ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_FLAG);
    }

private:
    AscendC::GlobalTensor<T> xGm;
    AscendC::GlobalTensor<T> yGm;
    AscendC::GlobalTensor<T> buffGm;
    uint32_t rank;
    uint32_t atomicOp;
    uint32_t aivNum;
    uint32_t aivIndex;
    uint32_t totalElems;
    uint32_t addrOffset;
    uint32_t exchangeMetaSize;
    uint64_t waitSymbol;
    ArParallelStrategy meta;
    __gm__ uint64_t *exchangeInputStart;
    __gm__ uint64_t *exchangeBufferStart;
    __gm__ uint64_t *exchangeInputFlagStart;
    __gm__ uint64_t *exchangeBufferFlagStart;
    GM_ADDR inputPtr;
    GM_ADDR exchangeBufPtr;
    GM_ADDR input;
    GM_ADDR output;
    GM_ADDR buffer;
};

extern "C" __global__ __aicore__ void ZBALAllReduceInner(GM_ADDR input, GM_ADDR output, GM_ADDR buffer, GM_ADDR gva,
                                                         uint64_t fftsAddr, uint32_t dataType, uint32_t totalElems,
                                                         uint32_t reduceOp, uint64_t waitSymbol)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIV_1_0);
    AscendC::SetSyncBaseAddr(fftsAddr);
    zbal_datatype_t zbalDataType = static_cast<zbal_datatype_t>(dataType);
    switch (zbalDataType) {
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT8: {
            ZBALAllReduceKernel<int8_t> op;
            op.Init(input, output, gva, buffer, totalElems, reduceOp, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT16: {
            ZBALAllReduceKernel<int16_t> op;
            op.Init(input, output, gva, buffer, totalElems, reduceOp, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT32: {
            ZBALAllReduceKernel<int32_t> op;
            op.Init(input, output, gva, buffer, totalElems, reduceOp, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP32: {
            ZBALAllReduceKernel<float> op;
            op.Init(input, output, gva, buffer, totalElems, reduceOp, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP16: {
            ZBALAllReduceKernel<float16_t> op;
            op.Init(input, output, gva, buffer, totalElems, reduceOp, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_BFP16: {
            ZBALAllReduceKernel<bfloat16_t> op;
            op.Init(input, output, gva, buffer, totalElems, reduceOp, waitSymbol);
            op.Process();
            break;
        }
        default:
            return;
    }
}

int32_t ZBALOpAllReduce(const void *inp, void *out, void *buf, size_t numel, zbal_datatype_t dataType,
                        aclrtStream stream, zbal_reduce_op_t reduceOp, CommGroupInfo &groupInfo)
{
    uint32_t blockDim = ZBALOpGetAivBlockDim(groupInfo, numel, dataType);
    if (numel <= groupInfo.groupSize && blockDim > groupInfo.groupSize) {
        blockDim = groupInfo.groupSize;
    }

    uint32_t dataTypeNum = static_cast<uint32_t>(dataType);
    uint32_t reduceOpNum = static_cast<uint32_t>(reduceOp);

    uint64_t fftsAddr = groupInfo.fftsConfig;
    uint8_t *metaAddr = reinterpret_cast<uint8_t *>(groupInfo.myMetaGva);
    uint8_t *input = reinterpret_cast<uint8_t *>(const_cast<void *>(inp));
    uint8_t *output = reinterpret_cast<uint8_t *>(out);
    uint8_t *buffer = reinterpret_cast<uint8_t *>(buf);
    uint64_t waitSymbol = ++groupInfo.waitSymbol;
    ZBALAllReduceInner<<<blockDim, nullptr, stream>>>(input, output, buffer, metaAddr, fftsAddr, dataTypeNum, numel,
                                                      reduceOpNum, waitSymbol);

    return 0;
}
