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

#include <random>
#include <limits>
#include <iostream>
#include "kernel_operator.h"
#include "zbal_def.h"
#include "zbal_kernel_trace.h"
#include "zbal_kernel_utils.h"
#include "zbal_comm_host_device_struct.h"
#include "zbal_kernel_sdma_data_op.h"

template<typename T>
class ScatterKernel {
public:
    ZBAL_KERNEL ScatterKernel() {}

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
        this->addrOffset = groupSize * ZBAL_FLAG_SIZE;
        this->flagMagic = waitSymbol;
        this->localDeviceMemSize = comm->localDeviceMemSize;
        // |------input------|------flag------|
        this->exchangeAddr = reinterpret_cast<__gm__ uint64_t *>(comm->myAddressExchangeGva);
        this->exchangeFlag = this->exchangeAddr + this->addrOffset;
        this->peerGroupRank2WorldRank = reinterpret_cast<__gm__ uint16_t *>(comm->peerGroupRank2WorldRank);
        pipe.InitBuffer(bindQueue, 1, UB_ALIGN_SIZE_64);
    }

    ZBAL_KERNEL void Process()
    {
#ifdef __DAV_C220_VEC__
        InitDataAddrAndFlag();
        WaitFlag(root);
        uint64_t rootDataAddr = GetDataAddr(exchangeAddr, root);

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
        process_sdma_scatter(outputGm[startInRank], inputGm[startInRank], numPerCore);
        BarrierAll(comm);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_SCATTER_KERNEL_ALL);
#endif
    }

private:
    ZBAL_KERNEL void InitDataAddrAndFlag()
    {
        ZBAL_PROF_START(comm, ZBAL_PROF_EXCHANGE_ADDR);
        if (aivNum < groupSize) {
            uint32_t ranksPerCore = (groupSize + aivNum - 1) / aivNum;
            const int64_t startRank = aivIndex * ranksPerCore;
            int64_t endRank = startRank + ranksPerCore;
            if (endRank > groupSize) {
                endRank = groupSize;
            }
            for (auto dsrRank = startRank; dsrRank < endRank; dsrRank++) {
                uint64_t dataAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(input));
                auto ptr = zbal_ptr(exchangeAddr, rank, dsrRank, localDeviceMemSize, peerGroupRank2WorldRank);
                SetDataAddr(ptr, dataAddr, rank);
                AscendC::PipeBarrier<PIPE_ALL>();
                auto flagPtr = zbal_ptr(exchangeFlag, rank, dsrRank, localDeviceMemSize, peerGroupRank2WorldRank);
                SetFlag(flagPtr, flagMagic, rank);
                AscendC::PipeBarrier<PIPE_ALL>();
            }
        } else if (aivIndex < groupSize) {
            auto ptr = zbal_ptr(exchangeAddr, rank, aivIndex, localDeviceMemSize, peerGroupRank2WorldRank);
            if (rank == root) {
                uint64_t dataAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(input));
                SetDataAddr(ptr, dataAddr, rank);
            }
            AscendC::PipeBarrier<PIPE_ALL>();
            auto flagPtr = zbal_ptr(exchangeFlag, rank, aivIndex, localDeviceMemSize, peerGroupRank2WorldRank);
            SetFlag(flagPtr, flagMagic, rank);
        }
        ZBAL_PROF_STOP(comm, ZBAL_PROF_EXCHANGE_ADDR);
    }

    ZBAL_KERNEL uint64_t GetDataAddr(__gm__ void *metaAddr, uint32_t coreTargetRank)
    {
        uint32_t dataAddrOffset = coreTargetRank * ZBAL_FLAG_SIZE;
        __gm__ uint64_t *dataGmAddr = (__gm__ uint64_t *)metaAddr + dataAddrOffset;
        dcciCacheline((__gm__ uint8_t *)dataGmAddr);
        __gm__ uint64_t *realInputAddr = (__gm__ uint64_t *)(*dataGmAddr);
        return realInputAddr[rank];
    }

    ZBAL_KERNEL void SetDataAddr(__gm__ void *metaAddr, uint64_t val, uint32_t coreTargetRank)
    {
        uint32_t dataAddrOffset = coreTargetRank * ZBAL_FLAG_SIZE;
        __gm__ uint64_t *dataGmAddr = (__gm__ uint64_t *)metaAddr + dataAddrOffset;
        *dataGmAddr = val;
        dcciCacheline((__gm__ uint8_t *)dataGmAddr);
    }

    ZBAL_KERNEL void WaitFlag(uint32_t coreTargetRank)
    {
        ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_FLAG);
        uint64_t readyFlag;
        do {
            readyFlag = GetFlag(exchangeFlag, coreTargetRank);
        } while (readyFlag != flagMagic);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_FLAG);
    }

    ZBAL_KERNEL void process_sdma_scatter(AscendC::GlobalTensor<T> outputGT, AscendC::GlobalTensor<T> inputGT,
                                          uint64_t count)
    {
        // Define temporary UB buffer as LocalTensor for SDMA operations
        AscendC::LocalTensor<T> tmp_local = bindQueue.AllocTensor<T>();
        zbal_sdma_get_nbi(outputGT, inputGT, tmp_local, count, EVENT_ID0);
        zbal_sdma_quiet(tmp_local, EVENT_ID0);
    }

private:
    AscendC::TPipe pipe;
    AscendC::TQueBind<AscendC::TPosition::VECIN, AscendC::TPosition::VECOUT, 1> bindQueue;
    AscendC::GlobalTensor<T> inputGm;
    AscendC::GlobalTensor<T> outputGm;
    uint32_t aivNum;
    uint32_t aivIndex;
    uint16_t root;
    uint32_t rank;
    uint32_t groupSize;
    uint32_t elements;
    uint32_t addrOffset;
    uint64_t flagMagic;
    uint64_t localDeviceMemSize;
    __gm__ void *input;
    __gm__ void *output;
    __gm__ CommGroupInfo *comm;
    __gm__ uint64_t *exchangeAddr;
    __gm__ uint64_t *exchangeFlag;
    __gm__ uint16_t *peerGroupRank2WorldRank;
};

extern "C" __global__ __aicore__ void ZBALScatterSDMAInner(GM_ADDR input, GM_ADDR output, size_t elements,
                                                           uint32_t dataType, GM_ADDR metaAddr, uint16_t root,
                                                           uint64_t waitSymbol)
{
    zbal_datatype_t ZBAL_DATA_TYPE = static_cast<zbal_datatype_t>(dataType);
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIV_1_0);

    switch (ZBAL_DATA_TYPE) {
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT8: {
            ScatterKernel<int8_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT16: {
            ScatterKernel<int16_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT32: {
            ScatterKernel<int32_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP16: {
            ScatterKernel<float16_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP32: {
            ScatterKernel<float> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT64: {
            ScatterKernel<int64_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT64: {
            ScatterKernel<uint64_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT8: {
            ScatterKernel<uint8_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT16: {
            ScatterKernel<uint16_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT32: {
            ScatterKernel<uint32_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP64: {
            ScatterKernel<float64_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        case zbal_datatype_t::ZBAL_DATA_TYPE_BFP16: {
            ScatterKernel<bfloat16_t> op;
            op.Init(input, output, metaAddr, elements, root, waitSymbol);
            op.Process();
            break;
        }
        default:
            break;
    }
}

int32_t ZBALOpScatterSDMA(const void *sendBuff, void *recvBuff, size_t sendCount, zbal_datatype_t dataType,
                          uint16_t root, aclrtStream stream, CommGroupInfo &groupInfo)
{
    uint32_t minNum = 16;
    uint32_t blockDim = ZBALOpGetAivBlockDim(groupInfo, sendCount, dataType);
    if (blockDim > minNum) {
        blockDim = minNum;
    }

    uint32_t dataTypeNum = static_cast<uint32_t>(dataType);

    uint64_t fftsAddr = groupInfo.fftsConfig;
    uint16_t rank = groupInfo.myGroupRank;
    uint16_t groupSize = groupInfo.groupSize;
    uint8_t *metaAddr = reinterpret_cast<uint8_t *>(groupInfo.myMetaGva);
    uint8_t *input = reinterpret_cast<uint8_t *>(const_cast<void *>(sendBuff));
    uint8_t *output = reinterpret_cast<uint8_t *>(recvBuff);
    uint64_t waitSymbol = ++groupInfo.waitSymbol;

    ZBALScatterSDMAInner<<<blockDim, nullptr, stream>>>(input, output, sendCount, dataTypeNum, metaAddr, root,
                                                        waitSymbol);
    return 0;
}
