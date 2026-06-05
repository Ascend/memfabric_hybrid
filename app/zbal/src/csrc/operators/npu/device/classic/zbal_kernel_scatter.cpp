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

template<typename T>
class ZBALScatterKernel : public ZBALBaseKernel {
public:
    ZBAL_KERNEL ZBALScatterKernel() {}

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
        this->myGroupRank = comm->myGroupRank;
        this->groupSize = comm->groupSize;
        this->elements = elements;
        this->addrOffset = groupSize * ZBAL_FLAG_SIZE;
        this->flagMagic = waitSymbol;
        this->memSize = comm->localDeviceMemSize;
        // |------input------|------flag------|
        this->exchangeAddr = reinterpret_cast<__gm__ uint64_t *>(comm->myAddressExchangeGva);
        this->exchangeFlag = this->exchangeAddr + this->addrOffset;
        this->worldRanks = reinterpret_cast<__gm__ uint16_t *>(comm->peerGroupRank2WorldRank);

        this->dataOpType = comm->dataOpType;
        ZBALBaseKernel::Init();
    }

    ZBAL_KERNEL void Process()
    {
#ifdef __DAV_C220_VEC__
        InitDataAddrAndFlag();
        ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_FLAG);
        ZBALWaitFlag(exchangeFlag, flagMagic, root);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_FLAG);
        uint64_t rootDataAddr = GetRootDataAddr(exchangeAddr, root);

        uint32_t elementsPerRank = elements;
        uint32_t rankOffset = rank * elementsPerRank;
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

        ZBAL_PROF_START(comm, ZBAL_PROF_SCATTER_KERNEL_ALL);
        CpGM2GM(inputGm[startInRank], outputGm[startInRank], numPerCore);
        BarrierAll();
        ZBAL_PROF_STOP(comm, ZBAL_PROF_SCATTER_KERNEL_ALL);
#endif
    }

private:
    ZBAL_KERNEL void InitDataAddrAndFlag()
    {
        ZBAL_PROF_START(comm, ZBAL_PROF_EXCHANGE_ADDR);
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

        if (startRank < groupSize && root == rank) {
            for (int64_t r = startRank; r < endRank; r++) {
                uint64_t dataAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(input));
                auto ptr = ZbalPtr(exchangeAddr, r);
                ZBALSetFlag(ptr, dataAddr, rank);
                AscendC::PipeBarrier<PIPE_ALL>();
                auto flagPtr = ZbalPtr(exchangeFlag, r);
                ZBALSetFlag(flagPtr, flagMagic, rank);
                AscendC::PipeBarrier<PIPE_ALL>();
            }
        }
        ZBAL_PROF_STOP(comm, ZBAL_PROF_EXCHANGE_ADDR);
    }

    ZBAL_KERNEL uint64_t GetRootDataAddr(__gm__ void *metaAddr, uint32_t coreTargetRank)
    {
        uint32_t dataAddrOffset = coreTargetRank * ZBAL_FLAG_SIZE;
        __gm__ uint64_t *dataGmAddr = (__gm__ uint64_t *)metaAddr + dataAddrOffset;
        dcciCacheline((__gm__ uint8_t *)dataGmAddr);
        __gm__ uint64_t *realInputAddr = (__gm__ uint64_t *)(*dataGmAddr);
        return realInputAddr[rank];
    }

private:
    AscendC::GlobalTensor<T> inputGm;
    AscendC::GlobalTensor<T> outputGm;
    uint32_t aivNum;
    uint32_t aivIndex;
    uint16_t root;
    uint32_t rank;
    uint32_t elements;
    uint32_t addrOffset;
    uint64_t flagMagic;
    __gm__ void *input;
    __gm__ void *output;
    __gm__ uint64_t *exchangeAddr;
    __gm__ uint64_t *exchangeFlag;
};

extern "C" __global__ __aicore__ void ZBALScatterInner(GM_ADDR input, GM_ADDR output, size_t elements,
                                                       uint32_t dataType, GM_ADDR metaAddr, uint16_t root,
                                                       uint64_t waitSymbol)
{
    zbal_datatype_t ZBAL_DATA_TYPE = static_cast<zbal_datatype_t>(dataType);
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIV_1_0);

    switch (ZBAL_DATA_TYPE) {
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT8: {
            ZBALScatterKernel<int8_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT16: {
            ZBALScatterKernel<int16_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT32: {
            ZBALScatterKernel<int32_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP16: {
            ZBALScatterKernel<float16_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP32: {
            ZBALScatterKernel<float> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT64: {
            ZBALScatterKernel<int64_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT64: {
            ZBALScatterKernel<uint64_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT8: {
            ZBALScatterKernel<uint8_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT16: {
            ZBALScatterKernel<uint16_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT32: {
            ZBALScatterKernel<uint32_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP64: {
            ZBALScatterKernel<float64_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_BFP16: {
            ZBALScatterKernel<bfloat16_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        default:
            break;
    }
}

int32_t ZBALOpScatter(const void *sendBuff, void *recvBuff, size_t sendCount, zbal_datatype_t dataType, uint16_t root,
                      aclrtStream stream, CommGroupInfo &groupInfo)
{
    uint32_t blockDim = ZBALOpGetAivBlockDim(groupInfo, sendCount, dataType);

    uint32_t dataTypeNum = static_cast<uint32_t>(dataType);

    uint64_t fftsAddr = groupInfo.fftsConfig;
    uint16_t rank = groupInfo.myGroupRank;
    uint16_t groupSize = groupInfo.groupSize;
    uint8_t *metaAddr = reinterpret_cast<uint8_t *>(groupInfo.myMetaGva);
    uint8_t *input = reinterpret_cast<uint8_t *>(const_cast<void *>(sendBuff));
    uint8_t *output = reinterpret_cast<uint8_t *>(recvBuff);
    uint64_t waitSymbol = ++groupInfo.waitSymbol;

    ZBALScatterInner<<<blockDim, nullptr, stream>>>(input, output, sendCount, dataTypeNum, metaAddr, root, waitSymbol);
    return 0;
}
