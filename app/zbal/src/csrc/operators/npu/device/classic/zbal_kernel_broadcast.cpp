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

const uint32_t SMALL_AG_THRESHOLD = 1024 * 7168;
const uint32_t FULL_COPY_GROUP_SIZE = 4;

template<typename T>
class ZBALBroadcastFullCopyKernel : public ZBALBaseKernel {
public:
    ZBAL_KERNEL ZBALBroadcastFullCopyKernel() {}

    ZBAL_KERNEL void Init(GM_ADDR input, GM_ADDR output, GM_ADDR metaGM, uint64_t elements, uint16_t root,
                          uint64_t waitSymbol)
    {
        this->aivNum = AscendC::GetBlockNum();
        this->aivIndex = AscendC::GetBlockIdx();
        this->root = root;
        this->input = input;
        this->output = output;
        this->comm = reinterpret_cast<__gm__ CommGroupInfo *>(metaGM);
        this->rank = comm->myGroupRank;
        this->groupSize = comm->groupSize;
        this->elements = elements;
        this->flagMagic = waitSymbol;
        this->localDeviceMemSize = comm->localDeviceMemSize;
        this->inputAddrSize = groupSize * ZBAL_FLAG_SIZE;
        // |------input------|------flag------|------stat------|
        this->exchangeAddr = comm->myAddressExchangeGva;
        this->inputAddr = reinterpret_cast<__gm__ uint64_t *>(exchangeAddr);
        this->flagAddr = this->inputAddr + inputAddrSize;
        this->peerGroupRank2WorldRank = reinterpret_cast<__gm__ uint16_t *>(comm->peerGroupRank2WorldRank);

        ZBALBaseKernel::Init(comm->dataOpType);
    }

    ZBAL_KERNEL void Process()
    {
#ifdef __DAV_C220_VEC__
        ZBAL_PROF_START(comm, ZBAL_PROF_BROADCAST_KERNEL_ALL);

        InitDataAddrAndFlag();
        ZBALWaitFlag(flagAddr, flagMagic, root);
        uint64_t rootDataAddr = ZBALGetFlag(inputAddr, root);

        int64_t startRank;
        int64_t endRank;
        if (aivNum < groupSize) {
            uint32_t base = groupSize / aivNum;
            uint32_t rem = groupSize % aivNum;
            if (aivIndex < rem) {
                startRank = aivIndex * (base + 1);
                endRank = startRank + base + 1;
            } else {
                startRank = rem * (base + 1) + (aivIndex - rem) * base;
                endRank = startRank + base;
            }
        } else {
            startRank = aivIndex;
            endRank = aivIndex + 1;
        }
        for (int64_t dsrRank = startRank; dsrRank < endRank; dsrRank++) {
            uint32_t elementsPerRank = elements;
            uint32_t baseElementsPerCore = elementsPerRank / aivNum;
            uint32_t startInRank = 0;
            uint32_t numPerCore = 0;

            startInRank = aivIndex * baseElementsPerCore;
            if (aivIndex == aivNum - 1) {
                numPerCore = elementsPerRank - startInRank;
            } else {
                numPerCore = baseElementsPerCore;
            }

            inputGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(rootDataAddr), numPerCore);
            outputGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(output), numPerCore);

            ZBAL_PROF_START(comm, ZBAL_PROF_BROADCAST_SCATTER);
            if (rank != root) {
                CpGM2GM(inputGm[startInRank], outputGm[startInRank], numPerCore);
            }
            ZBAL_PROF_STOP(comm, ZBAL_PROF_BROADCAST_SCATTER);
        }
        BarrierAll(comm);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_BROADCAST_KERNEL_ALL);
#endif
    }

private:
    ZBAL_KERNEL void InitDataAddrAndFlag()
    {
        ZBAL_PROF_START(comm, ZBAL_PROF_EXCHANGE_ADDR);
        if (rank != root) {
            ZBAL_PROF_STOP(comm, ZBAL_PROF_EXCHANGE_ADDR);
            return;
        }
        if (aivNum < groupSize) {
            uint32_t ranksPerCore = (groupSize + aivNum - 1) / aivNum;
            const int64_t startRank = aivIndex * ranksPerCore;
            int64_t endRank = startRank + ranksPerCore;
            if (endRank > groupSize) {
                endRank = groupSize;
            }

            for (int dstRank = startRank; dstRank < endRank; dstRank++) {
                uint64_t dataAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(input));
                auto ptr = zbal_ptr(inputAddr, rank, dstRank, localDeviceMemSize, peerGroupRank2WorldRank);
                auto flagPtr = zbal_ptr(flagAddr, rank, dstRank, localDeviceMemSize, peerGroupRank2WorldRank);

                ZBALSetFlag(ptr, dataAddr, rank);
                AscendC::PipeBarrier<PIPE_ALL>();
                ZBALSetFlag(flagPtr, flagMagic, rank);
                AscendC::PipeBarrier<PIPE_ALL>();
            }
        } else if (aivIndex < groupSize) {
            uint64_t dataAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(input));
            auto ptr = zbal_ptr(inputAddr, rank, aivIndex, localDeviceMemSize, peerGroupRank2WorldRank);
            auto flagPtr = zbal_ptr(flagAddr, rank, aivIndex, localDeviceMemSize, peerGroupRank2WorldRank);

            ZBALSetFlag(ptr, dataAddr, rank);
            AscendC::PipeBarrier<PIPE_ALL>();
            ZBALSetFlag(flagPtr, flagMagic, rank);
            AscendC::PipeBarrier<PIPE_ALL>();
        }
        ZBAL_PROF_STOP(comm, ZBAL_PROF_EXCHANGE_ADDR);
    }

private:
    AscendC::GlobalTensor<T> inputGm;
    AscendC::GlobalTensor<T> outputGm;
    uint32_t aivNum;
    uint32_t aivIndex;
    uint16_t root;
    uint32_t rank;
    uint32_t groupSize;
    uint32_t elements;
    uint32_t inputAddrSize;
    uint64_t flagMagic;
    uint64_t localDeviceMemSize;
    __gm__ void *input;
    __gm__ void *output;
    __gm__ CommGroupInfo *comm;
    uintptr_t exchangeAddr;
    __gm__ uint64_t *inputAddr;
    __gm__ uint64_t *flagAddr;
    __gm__ uint16_t *peerGroupRank2WorldRank;
};

template<typename T>
class ZBALBroadcastRingKernel : public ZBALBaseKernel {
public:
    ZBAL_KERNEL ZBALBroadcastRingKernel() {}

    ZBAL_KERNEL void Init(GM_ADDR input, GM_ADDR output, GM_ADDR metaGM, uint64_t elements, uint16_t root,
                          uint64_t waitSymbol)
    {
        this->aivNum = AscendC::GetBlockNum();
        this->aivIndex = AscendC::GetBlockIdx();
        this->root = root;
        this->input = input;
        this->output = output;
        this->comm = reinterpret_cast<__gm__ CommGroupInfo *>(metaGM);
        this->rank = comm->myGroupRank;
        this->groupSize = comm->groupSize;
        this->elements = elements;
        this->flagMagic = waitSymbol;
        this->localDeviceMemSize = comm->localDeviceMemSize;
        this->inputAddrSize = groupSize * ZBAL_FLAG_SIZE;
        // |------input------|------flag------|------stat------|
        this->exchangeAddr = comm->myAddressExchangeGva;
        this->inputAddr = reinterpret_cast<__gm__ uint64_t *>(exchangeAddr);
        this->flagAddr = this->inputAddr + inputAddrSize;
        this->peerGroupRank2WorldRank = reinterpret_cast<__gm__ uint16_t *>(comm->peerGroupRank2WorldRank);

        ZBALBaseKernel::Init(comm->dataOpType);
    }

    ZBAL_KERNEL void Process()
    {
#ifdef __DAV_C220_VEC__
        ZBAL_PROF_START(comm, ZBAL_PROF_BROADCAST_KERNEL_ALL);
        InitDataAddrAndFlag();
        ZBALWaitFlag(flagAddr, flagMagic, root);
        uint64_t rootDataAddr = ZBALGetFlag(inputAddr, root);

        elementsPerRank = elements / groupSize;
        rankOffset = rank * elementsPerRank;
        uint32_t baseElementsPerCore = elementsPerRank / aivNum;
        uint32_t startInRank = 0;
        uint32_t numPerCore = 0;
        startInRank = aivIndex * baseElementsPerCore;

        if (aivIndex == aivNum - 1) {
            numPerCore = elementsPerRank - startInRank;
        } else {
            numPerCore = baseElementsPerCore;
        }
        uint32_t globalOffset = rankOffset + startInRank;

        inputGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(rootDataAddr), numPerCore);
        outputGm.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(output), numPerCore);

        ZBAL_PROF_START(comm, ZBAL_PROF_BROADCAST_SCATTER);
        if (rank != root) {
            CpGM2GM(inputGm[globalOffset], outputGm[globalOffset], numPerCore);
        }
        ZBAL_PROF_STOP(comm, ZBAL_PROF_BROADCAST_SCATTER);

        ZBAL_PROF_START(comm, ZBAL_PROF_BROADCAST_ALLGATHER);
        BarrierAll(comm);
        if (elements * sizeof(T) <= SMALL_AG_THRESHOLD) {
            ZBALAllGatherSmallKernel op;
            op.Init<T>((GM_ADDR)(input) + rankOffset * sizeof(T), (GM_ADDR)output, (GM_ADDR)comm, elementsPerRank,
                       flagMagic + 16);
            op.Process<T>();
        } else {
            ZBALAllGatherBigKernel op;
            op.Init<T>((GM_ADDR)(input) + rankOffset * sizeof(T), (GM_ADDR)output, (GM_ADDR)comm, elementsPerRank,
                       flagMagic + 16);
            op.Process<T>();
        }
        ZBAL_PROF_STOP(comm, ZBAL_PROF_BROADCAST_ALLGATHER);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_BROADCAST_KERNEL_ALL);
#endif
    }

private:
    ZBAL_KERNEL void InitDataAddrAndFlag()
    {
        ZBAL_PROF_START(comm, ZBAL_PROF_EXCHANGE_ADDR);
        if (rank != root) {
            ZBAL_PROF_STOP(comm, ZBAL_PROF_EXCHANGE_ADDR);
            return;
        }
        if (aivNum < groupSize) {
            uint32_t ranksPerCore = (groupSize + aivNum - 1) / aivNum;
            const int64_t startRank = aivIndex * ranksPerCore;
            int64_t endRank = startRank + ranksPerCore;
            if (endRank > groupSize) {
                endRank = groupSize;
            }

            for (int dstRank = startRank; dstRank < endRank; dstRank++) {
                uint64_t dataAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(input));
                auto ptr = zbal_ptr(inputAddr, rank, dstRank, localDeviceMemSize, peerGroupRank2WorldRank);
                auto flagPtr = zbal_ptr(flagAddr, rank, dstRank, localDeviceMemSize, peerGroupRank2WorldRank);

                ZBALSetFlag(ptr, dataAddr, rank);
                AscendC::PipeBarrier<PIPE_ALL>();
                ZBALSetFlag(flagPtr, flagMagic, rank);
                AscendC::PipeBarrier<PIPE_ALL>();
            }
        } else if (aivIndex < groupSize) {
            uint64_t dataAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(input));
            auto ptr = zbal_ptr(inputAddr, rank, aivIndex, localDeviceMemSize, peerGroupRank2WorldRank);
            auto flagPtr = zbal_ptr(flagAddr, rank, aivIndex, localDeviceMemSize, peerGroupRank2WorldRank);

            ZBALSetFlag(ptr, dataAddr, rank);
            AscendC::PipeBarrier<PIPE_ALL>();
            ZBALSetFlag(flagPtr, flagMagic, rank);
            AscendC::PipeBarrier<PIPE_ALL>();
        }
        ZBAL_PROF_STOP(comm, ZBAL_PROF_EXCHANGE_ADDR);
    }

private:
    AscendC::GlobalTensor<T> inputGm;
    AscendC::GlobalTensor<T> outputGm;
    uint32_t aivNum;
    uint32_t aivIndex;
    uint16_t root;
    uint32_t rank;
    uint32_t groupSize;
    uint32_t elements;
    uint32_t inputAddrSize;
    uint64_t flagMagic;
    uint64_t localDeviceMemSize;
    uint32_t elementsPerRank;
    uint32_t rankOffset;
    __gm__ void *input;
    __gm__ void *output;
    __gm__ CommGroupInfo *comm;
    uintptr_t exchangeAddr;
    __gm__ uint64_t *inputAddr;
    __gm__ uint64_t *flagAddr;
    __gm__ uint16_t *peerGroupRank2WorldRank;
};

extern "C" __global__ __aicore__ void ZBALBroadcastInner(GM_ADDR input, GM_ADDR output, size_t elements,
                                                         uint32_t dataType, GM_ADDR metaAddr, uint16_t root,
                                                         uint64_t waitSymbol, uint32_t groupSize)
{
    zbal_datatype_t ZBAL_DATA_TYPE = static_cast<zbal_datatype_t>(dataType);
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIV_1_0);
    if (groupSize <= FULL_COPY_GROUP_SIZE || elements % groupSize != 0) {
        switch (ZBAL_DATA_TYPE) {
            case zbal_datatype_t::ZBAL_DATA_TYPE_INT8: {
                ZBALBroadcastFullCopyKernel<int8_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_INT16: {
                ZBALBroadcastFullCopyKernel<int16_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_INT32: {
                ZBALBroadcastFullCopyKernel<int32_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_FP16: {
                ZBALBroadcastFullCopyKernel<float16_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_FP32: {
                ZBALBroadcastFullCopyKernel<float> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_INT64: {
                ZBALBroadcastFullCopyKernel<int64_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_UINT64: {
                ZBALBroadcastFullCopyKernel<uint64_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_UINT8: {
                ZBALBroadcastFullCopyKernel<uint8_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_UINT16: {
                ZBALBroadcastFullCopyKernel<uint16_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_UINT32: {
                ZBALBroadcastFullCopyKernel<uint32_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_FP64: {
                ZBALBroadcastFullCopyKernel<float64_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_BFP16: {
                ZBALBroadcastFullCopyKernel<bfloat16_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            default:
                break;
        }
    } else {
        switch (ZBAL_DATA_TYPE) {
            case zbal_datatype_t::ZBAL_DATA_TYPE_INT8: {
                ZBALBroadcastRingKernel<int8_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_INT16: {
                ZBALBroadcastRingKernel<int16_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_INT32: {
                ZBALBroadcastRingKernel<int32_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_FP16: {
                ZBALBroadcastRingKernel<float16_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_FP32: {
                ZBALBroadcastRingKernel<float> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_INT64: {
                ZBALBroadcastRingKernel<int64_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_UINT64: {
                ZBALBroadcastRingKernel<uint64_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_UINT8: {
                ZBALBroadcastRingKernel<uint8_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_UINT16: {
                ZBALBroadcastRingKernel<uint16_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_UINT32: {
                ZBALBroadcastRingKernel<uint32_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_FP64: {
                ZBALBroadcastRingKernel<float64_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            case zbal_datatype_t::ZBAL_DATA_TYPE_BFP16: {
                ZBALBroadcastRingKernel<bfloat16_t> op;
                op.Init(input, output, metaAddr, elements, root, waitSymbol);
                op.Process();
                break;
            }
            default:
                break;
        }
    }
}

int32_t ZBALOpBroadcast(const void *sendBuff, size_t sendCount, zbal_datatype_t dataType, uint16_t root,
                        aclrtStream stream, CommGroupInfo &groupInfo)
{
    uint32_t blockDim = ZBALOpGetAivBlockDim(groupInfo, sendCount, dataType);

    uint32_t dataTypeNum = static_cast<uint32_t>(dataType);

    // Prepare FFTS address
    uint64_t fftsAddr = groupInfo.fftsConfig;
    uint16_t rank = groupInfo.myGroupRank;
    uint16_t groupSize = groupInfo.groupSize;
    uint8_t *metaAddr = reinterpret_cast<uint8_t *>(groupInfo.myMetaGva);
    uint8_t *input = reinterpret_cast<uint8_t *>(const_cast<void *>(sendBuff));
    uint8_t *output = reinterpret_cast<uint8_t *>(const_cast<void *>(sendBuff));
    uint64_t waitSymbol = ++groupInfo.waitSymbol;

    ZBALBroadcastInner<<<blockDim, nullptr, stream>>>(input, output, sendCount, dataTypeNum, metaAddr, root, waitSymbol,
                                                      groupSize);
    return 0;
}
