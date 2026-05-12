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
#include <acl/acl_rt.h>
#include "kernel_operator.h"
#include "zbal_def.h"
#include "zbal_defines.h"
#include "zbal_kernel_utils.h"
#include "zbal_kernel_trace.h"

using namespace zbal;
constexpr uint16_t ZBAL_INOUT_LEN = 2;

class AlltoAllVKernel {
public:
    ZBAL_KERNEL AlltoAllVKernel() {}

    template<typename T>
    ZBAL_KERNEL void Init(GM_ADDR input, GM_ADDR output, GM_ADDR metaGM, GM_ADDR inputCumSum, GM_ADDR outputCounts,
                          GM_ADDR elements, uint64_t waitSymbol)
    {
#ifdef __DAV_C220_VEC__
        this->aivNum = AscendC::GetBlockNum();
        this->comm = reinterpret_cast<__gm__ CommGroupInfo *>(metaGM);
        this->groupSize = comm->groupSize;
        this->myGroupRank = comm->myGroupRank;
        this->memSize = comm->localDeviceMemSize;
        this->peerRank = reinterpret_cast<__gm__ uint16_t *>(comm->peerGroupRank2WorldRank);
        this->exchangeAddr = comm->myAddressExchangeGva;
        this->paramAddr = comm->myParamDataGva;

        uint64_t inputAddrSize = groupSize * ZBAL_FLAG_SIZE;
        this->inputAddr = reinterpret_cast<__gm__ uint64_t *>(exchangeAddr);
        this->inputCumSumAddr = this->inputAddr + inputAddrSize;
        this->inputElementAddr = this->inputCumSumAddr + inputAddrSize;
        this->flagAddr = this->inputElementAddr + inputAddrSize;
        this->statAddr = this->flagAddr + inputAddrSize;
        this->localStatSendAddr = this->statAddr + inputAddrSize;
        this->localStatBaseAddr = this->localStatSendAddr + inputAddrSize;
        this->localStatReadyAddr = this->localStatBaseAddr + inputAddrSize;
        this->input = input;
        this->output = output;
        this->inputCumSum = inputCumSum;
        this->outputCounts = outputCounts;
        this->elements = elements;
        this->waitSymbol = waitSymbol;
#endif
    }

    ZBAL_KERNEL void Prepare()
    {
        AscendC::LocalTensor<uint64_t> buf1(AscendC::TPosition::VECIN, UB_BUFF_INTERVAL, UB_PAD_COUNT);
        auto elementsPtr = reinterpret_cast<__gm__ uint64_t *>(elements);

        GetMetaValue(elementsPtr, 0, ZBAL_INOUT_LEN, buf1);
        this->inputElement = buf1.GetValue(0);

        GetMetaValue(elementsPtr, 1, ZBAL_INOUT_LEN, buf1);
        this->outputElement = buf1.GetValue(0);

        for (uint16_t i = 0; i < groupSize; i++) {
            GetMetaValue(reinterpret_cast<__gm__ uint64_t *>(outputCounts), i, groupSize, buf1);
            outputCountsArray[i] = buf1.GetValue(0);

            SetMetaValue(this->localStatSendAddr, i, 0, groupSize, buf1);
            AscendC::PipeBarrier<PIPE_ALL>();
            SetMetaValue(this->localStatBaseAddr, i, 0, groupSize, buf1);
            AscendC::PipeBarrier<PIPE_ALL>();
        }
    }

    ZBAL_KERNEL void ReAssignmentCoreNum()
    {
        if (groupSize <= aivNum && outputElement < groupSize) {
            aivNum = groupSize;
        }
    }

    ZBAL_KERNEL void Exchange(uint16_t offset)
    {
        AscendC::LocalTensor<uint64_t> buf1(AscendC::TPosition::VECIN, UB_BUFF_INTERVAL, UB_PAD_COUNT);

        // exchange input addr
        AscendC::PipeBarrier<PIPE_ALL>();
        auto ptr = zbal_ptr(this->inputAddr, myGroupRank, offset, memSize, peerRank);
        SetMetaValue((__gm__ uint64_t *)ptr, myGroupRank, reinterpret_cast<uint64_t>(input), groupSize, buf1);

        // exchange inputCount
        AscendC::PipeBarrier<PIPE_ALL>();
        ptr = zbal_ptr(this->inputCumSumAddr, myGroupRank, offset, memSize, peerRank);
        SetMetaValue((__gm__ uint64_t *)ptr, myGroupRank, reinterpret_cast<uint64_t>(inputCumSum), groupSize, buf1);

        // exchange input elements
        AscendC::PipeBarrier<PIPE_ALL>();
        ptr = zbal_ptr(this->inputElementAddr, myGroupRank, offset, memSize, peerRank);
        SetMetaValue((__gm__ uint64_t *)ptr, myGroupRank, inputElement, groupSize, buf1);

        // write exchangeFlag
        AscendC::PipeBarrier<PIPE_ALL>();
        ptr = zbal_ptr(this->flagAddr, myGroupRank, offset, memSize, peerRank);
        SetMetaValue((__gm__ uint64_t *)ptr, myGroupRank, waitSymbol, groupSize, buf1);
    }

    ZBAL_KERNEL void WaitFlag(const uint16_t offset)
    {
        ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_FLAG);
        AscendC::LocalTensor<uint64_t> buf(AscendC::TPosition::VECIN, ZBAL_CONST_3 * UB_BUFF_INTERVAL, UB_PAD_COUNT);
        WaitMetaValue(this->flagAddr, offset, waitSymbol, groupSize, buf);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_FLAG);
    }

    ZBAL_KERNEL void GetInputInfo(uint16_t offset, uint64_t &inputOff, uint64_t &srcElement, __gm__ void **srcAddress)
    {
        AscendC::LocalTensor<uint64_t> buf1(AscendC::TPosition::VECIN, ZBAL_CONST_3 * UB_BUFF_INTERVAL, UB_PAD_COUNT);

        GetMetaValue(inputCumSumAddr, offset, groupSize, buf1);
        uint64_t srcCumSumAddr = buf1.GetValue(0);

        auto ptr = reinterpret_cast<__gm__ uint64_t *>(srcCumSumAddr);
        GetMetaValue(ptr, myGroupRank, groupSize, buf1);
        inputOff = buf1.GetValue(0);

        GetMetaValue(inputElementAddr, offset, groupSize, buf1);
        srcElement = buf1.GetValue(0);

        GetMetaValue(this->inputAddr, offset, groupSize, buf1);
        *srcAddress = reinterpret_cast<__gm__ void *>(buf1.GetValue(0));
    }

    ZBAL_KERNEL void GetCoreCommonRangeInfo(uint16_t &commStartRank, uint16_t &commEndRank)
    {
        if (groupSize <= aivNum) {
            if (AscendC::GetBlockIdx() < groupSize) {
                commStartRank = AscendC::GetBlockIdx();
                commEndRank = commStartRank + 1; // end not include
            } else {
                commEndRank = groupSize + 1;
                commStartRank = commEndRank + 1;
            }
        } else {
            uint16_t rankPerCore = groupSize / aivNum;
            commStartRank = AscendC::GetBlockIdx() * rankPerCore;
            commEndRank = commStartRank + rankPerCore; // end not include
            if (AscendC::GetBlockIdx() == aivNum - 1) {
                commEndRank = groupSize;
            }
        }
    }

    ZBAL_KERNEL void P2MPExchange(uint16_t commonStartRank, uint16_t commonEndRank)
    {
        ZBAL_PROF_START(comm, ZBAL_PROF_EXCHANGE_ADDR);
        for (uint16_t r = commonStartRank; r < commonEndRank; r++) {
            Exchange(r);
        }
        ZBAL_PROF_STOP(comm, ZBAL_PROF_EXCHANGE_ADDR);
    }

    ZBAL_KERNEL void GetCoreCopyRangeInfo(uint16_t &copyStartRank, uint64_t &startOffset, uint16_t &copyEndRank)
    {
        ZBAL_PROF_START(comm, ZBAL_PROF_ALLTOALL_CORE_RANGE);
        int64_t coreIndex = AscendC::GetBlockIdx();
        uint64_t curElement = outputElement / aivNum;
        uint64_t curCoreLeft = coreIndex * curElement;
        if (coreIndex == aivNum - 1) {
            curElement = outputElement - (aivNum - 1) * curElement;
        }

        copyStartRank = groupSize + 1;
        copyEndRank = groupSize;
        uint64_t left = 0;
        uint64_t curCoreRight = curCoreLeft + curElement;
        bool findStart = false;
        bool findEnd = false;
        for (uint16_t i = 0; i < groupSize && i < copyEndRank; i++) {
            copyElements[i] = 0;

            uint64_t counts = outputCountsArray[i];
            uint64_t right = left + counts;
            if (!findStart && right > curCoreLeft) {
                copyStartRank = i;
                startOffset = curCoreLeft - left;
                copyElements[i] = static_cast<uint32_t>(counts - startOffset);
                findStart = true;
            }
            if (findStart && !findEnd && right >= curCoreRight) {
                copyEndRank = i;
                if (copyStartRank == copyEndRank) {
                    copyElements[i] -= (right - curCoreRight);
                } else {
                    uint64_t copyCnt = curCoreRight - left;
                    copyElements[i] = static_cast<uint32_t>(copyCnt);
                }
                findEnd = true;
            }
            if (findStart && i > copyStartRank && i < copyEndRank) {
                copyElements[i] = static_cast<uint32_t>(counts);
            }
            left = right;
        }
        ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLTOALL_CORE_RANGE);
    }

    ZBAL_KERNEL void AtomicIncStat(__gm__ uint64_t *stat, uint16_t offset)
    {
        ZBAL_PROF_START(comm, ZBAL_PROF_ALLTOALL_ATOMIC_INC);
        __gm__ uint64_t *target = stat + offset * ZBAL_FLAG_SIZE;

        AscendC::SetAtomicAdd<int32_t>();

        AscendC::GlobalTensor<int32_t> outputGT;
        outputGT.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(target), ZBAL_FLAG_SIZE * sizeof(uint64_t));

        AscendC::LocalTensor<int32_t> buf(AscendC::TPosition::VECIN, UB_ALIGN_SIZE, UB_PAD_COUNT);
        buf.SetValue(0, 1);

        AscendC::DataCopyExtParams dataCopyParams(1, sizeof(int32_t), 0, 0, 0);
        AscendC::DataCopyPad(outputGT, buf, dataCopyParams);

        AscendC::SetAtomicNone();
        ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLTOALL_ATOMIC_INC);
    }

    ZBAL_KERNEL int32_t GetLocalStat(__gm__ uint64_t *stat, uint16_t offset)
    {
        __gm__ uint64_t *target = stat + offset * ZBAL_FLAG_SIZE;

        AscendC::LocalTensor<int32_t> buf(AscendC::TPosition::VECIN, UB_ALIGN_SIZE, UB_PAD_COUNT);

        GlobalTensor<int32_t> globalGT;
        globalGT.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(target), ZBAL_FLAG_SIZE * sizeof(uint64_t));
        SyncFunc<AscendC::HardEvent::MTE3_MTE2>(EVENT_ID0);
        AscendC::DataCopy(buf, globalGT, UB_PAD_COUNT);
        return buf.GetValue(0);
    }

    ZBAL_KERNEL void WaitLocalStat(__gm__ uint64_t *stat, uint16_t offset)
    {
        ZBAL_PROF_START(comm, ZBAL_PROF_ALLTOALL_WAIT_LOCAL_STAT);
        __gm__ uint64_t *target = stat + offset * ZBAL_FLAG_SIZE;

        AscendC::LocalTensor<int32_t> buf(AscendC::TPosition::VECIN, UB_ALIGN_SIZE, UB_PAD_COUNT);

        GlobalTensor<int32_t> globalGT;
        globalGT.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(target), ZBAL_FLAG_SIZE * sizeof(uint64_t));
        SyncFunc<AscendC::HardEvent::MTE3_MTE2>(EVENT_ID0);
        while (true) {
            int32_t base = GetLocalStat(localStatBaseAddr, offset);
            AscendC::DataCopy(buf, globalGT, UB_PAD_COUNT);
            SyncFunc<AscendC::HardEvent::MTE2_S>(EVENT_ID0);
            if (buf.GetValue(0) == base) {
                break;
            }
        }
        ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLTOALL_WAIT_LOCAL_STAT);
    }

    ZBAL_KERNEL void InitLocalStat()
    {
        ZBAL_PROF_START(comm, ZBAL_PROF_ALLTOALL_INIT_STAT);
        if (AscendC::GetBlockIdx() == 0) {
            uint64_t cumsum[ZBAL_MAX_RANK_SIZE];
            uint32_t statBase[ZBAL_MAX_RANK_SIZE];
            AscendC::LocalTensor<uint64_t> buf1(AscendC::TPosition::VECIN, UB_BUFF_INTERVAL, UB_PAD_COUNT);
            for (uint16_t i = 0; i < groupSize; i++) {
                statBase[i] = (outputCountsArray[i] > 0 ? 1 : 0);
                cumsum[i] = (i > 0 ? cumsum[i - 1] : 0) + outputCountsArray[i];
            }

            uint16_t core = 1;
            uint64_t avgElement = outputElement / aivNum;
            uint64_t coreRight = avgElement;

            // count split by core has more local stat, O(groupSize + aivNum)
            for (uint16_t k = 0; k < groupSize; k++) {
                while (cumsum[k] > coreRight) {
                    statBase[k] += 1;
                    core += 1;
                    if (core == aivNum) {
                        k += groupSize;
                        break;
                    }
                    coreRight += avgElement;
                }
                if (cumsum[k] == coreRight) {
                    core += 1;
                    coreRight += avgElement;
                }
            }

            for (uint16_t m = 0; m < groupSize; m++) {
                SetMetaValue(this->localStatBaseAddr, m, statBase[m], groupSize, buf1);
                AscendC::PipeBarrier<PIPE_ALL>();
            }

            SetMetaValue(this->localStatReadyAddr, 0, waitSymbol, groupSize, buf1);
            AscendC::PipeBarrier<PIPE_ALL>();
        }

        AscendC::LocalTensor<uint64_t> buf2(AscendC::TPosition::VECIN, ZBAL_CONST_3 * UB_BUFF_INTERVAL, UB_PAD_COUNT);
        WaitMetaValue(this->localStatReadyAddr, 0, waitSymbol, groupSize, buf2);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLTOALL_INIT_STAT);
    }

    template<typename T>
    ZBAL_KERNEL void AlltoAllvCopy(__gm__ T *output)
    {
        uint16_t copyStartRank, copyEndRank;
        uint64_t startOffset;
        GetCoreCopyRangeInfo(copyStartRank, startOffset, copyEndRank);
        ZBAL_PROF_DUMP(comm, __LINE__, copyStartRank, copyEndRank, startOffset);

        uint64_t outputOffset = outputElement / aivNum * AscendC::GetBlockIdx();
        for (uint16_t rank = copyStartRank; rank <= copyEndRank; rank++) {
            ZBAL_PROF_START(comm, ZBAL_PROF_ALLTOALL_COPY);
            uint64_t copyCount = copyElements[rank];
            if (copyCount > 0) {
                WaitFlag(rank);

                uint64_t inputOffset;
                uint64_t sourceElement;
                __gm__ void *remoteInput;
                GetInputInfo(rank, inputOffset, sourceElement, &remoteInput);

                inputOffset = inputOffset + ((rank == copyStartRank) ? startOffset : 0);
                CpGM2GM(output, outputElement, outputOffset, (__gm__ T *)remoteInput, sourceElement, inputOffset,
                        copyCount);

                outputOffset += copyCount;
                AtomicIncStat(this->localStatSendAddr, rank);
            }

            ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLTOALL_COPY);
        }
    }

    ZBAL_KERNEL void WriteRangeStat(uint16_t commonStartRank, uint16_t commonEndRank)
    {
        AscendC::LocalTensor<uint64_t> buf(AscendC::TPosition::VECIN, 0, UB_PAD_COUNT);
        for (uint16_t k = commonStartRank; k < commonEndRank; k++) {
            WaitLocalStat(this->localStatSendAddr, k);

            AscendC::PipeBarrier<PIPE_ALL>();
            auto ptr = zbal_ptr(this->statAddr, myGroupRank, static_cast<int>(k), memSize, peerRank);
            SetMetaValue((__gm__ uint64_t *)ptr, myGroupRank, waitSymbol, groupSize, buf);
        }
    }

    ZBAL_KERNEL void WaitRangeStat(uint16_t commonStartRank, uint16_t commonEndRank)
    {
        AscendC::LocalTensor<uint64_t> buf(AscendC::TPosition::VECIN, 0, UB_PAD_COUNT);
        if (commonStartRank >= groupSize) {
            commonStartRank = 0;
            commonEndRank = 1;
        }
        for (uint16_t k = commonStartRank; k < commonEndRank; k++) {
            ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_STAT);
            WaitMetaValue(this->statAddr, k, waitSymbol, groupSize, buf);
            ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_STAT);
        }
    }

    template<typename T>
    ZBAL_KERNEL void Process()
    {
#ifdef __DAV_C220_VEC__
        ZBAL_PROF_START(comm, ZBAL_PROF_ALLTOALL_KERNEL_ALL);

        Prepare();

        ReAssignmentCoreNum();

        if (AscendC::GetBlockIdx() < aivNum) {
            uint16_t commonStartRank, commonEndRank;
            GetCoreCommonRangeInfo(commonStartRank, commonEndRank);
            ZBAL_PROF_DUMP(comm, __LINE__, commonStartRank, commonEndRank, groupSize, aivNum);

            P2MPExchange(commonStartRank, commonEndRank);

            InitLocalStat();

            AlltoAllvCopy((__gm__ T *)output);

            WriteRangeStat(commonStartRank, commonEndRank);

            WaitRangeStat(commonStartRank, commonEndRank);
        }

        ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLTOALL_KERNEL_ALL);
#endif
    }

private:
    int64_t aivNum;
    uint16_t groupSize;
    uint16_t myGroupRank;
    uint64_t memSize;
    uintptr_t exchangeAddr;
    uintptr_t paramAddr;
    __gm__ uint64_t *inputAddr;        /* exchange input addr area */
    __gm__ uint64_t *inputCumSumAddr;  /* exchange input splits cumsum area */
    __gm__ uint64_t *inputElementAddr; /* exchange input elements area */
    __gm__ uint64_t *flagAddr;         /* exchange flag area */
    __gm__ uint64_t *statAddr;         /* exchange stat area */
    __gm__ uint64_t *localStatSendAddr;
    __gm__ uint64_t *localStatBaseAddr;
    __gm__ uint64_t *localStatReadyAddr;
    __gm__ void *input;
    __gm__ void *output;
    __gm__ void *inputCumSum;
    __gm__ void *outputCounts;
    __gm__ void *elements;
    __gm__ CommGroupInfo *comm;
    __gm__ uint16_t *peerRank;
    uint64_t inputElement;
    uint64_t outputElement;
    uint64_t waitSymbol;
    uint32_t outputCountsArray[ZBAL_MAX_RANK_SIZE]; /* same with outputCounts */
    uint32_t copyElements[ZBAL_MAX_RANK_SIZE];
};

extern "C" __global__ __aicore__ void ZBALAlltoAllVInner(GM_ADDR input, GM_ADDR output, GM_ADDR inputCumSum,
                                                         GM_ADDR outputCounts, GM_ADDR elements, int dataType,
                                                         GM_ADDR metaAddr, uint64_t waitSymbol)
{
    AlltoAllVKernel op;
    zbal_datatype_t ZBAL_DATA_TYPE = static_cast<zbal_datatype_t>(dataType);
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIV_1_0);

    switch (ZBAL_DATA_TYPE) {
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT8:
            op.Init<int8_t>(input, output, metaAddr, inputCumSum, outputCounts, elements, waitSymbol);
            op.Process<int8_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT16:
            op.Init<int16_t>(input, output, metaAddr, inputCumSum, outputCounts, elements, waitSymbol);
            op.Process<int16_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT32:
            op.Init<int32_t>(input, output, metaAddr, inputCumSum, outputCounts, elements, waitSymbol);
            op.Process<int32_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP16:
            op.Init<float16_t>(input, output, metaAddr, inputCumSum, outputCounts, elements, waitSymbol);
            op.Process<float16_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP32:
            op.Init<float>(input, output, metaAddr, inputCumSum, outputCounts, elements, waitSymbol);
            op.Process<float>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT64:
            op.Init<int64_t>(input, output, metaAddr, inputCumSum, outputCounts, elements, waitSymbol);
            op.Process<int64_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT64:
            op.Init<uint64_t>(input, output, metaAddr, inputCumSum, outputCounts, elements, waitSymbol);
            op.Process<uint64_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT8:
            op.Init<uint8_t>(input, output, metaAddr, inputCumSum, outputCounts, elements, waitSymbol);
            op.Process<uint8_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT16:
            op.Init<uint16_t>(input, output, metaAddr, inputCumSum, outputCounts, elements, waitSymbol);
            op.Process<uint16_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT32:
            op.Init<uint32_t>(input, output, metaAddr, inputCumSum, outputCounts, elements, waitSymbol);
            op.Process<uint32_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP64:
            op.Init<float64_t>(input, output, metaAddr, inputCumSum, outputCounts, elements, waitSymbol);
            op.Process<float64_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_BFP16:
            op.Init<bfloat16_t>(input, output, metaAddr, inputCumSum, outputCounts, elements, waitSymbol);
            op.Process<bfloat16_t>();
            break;
        default:
            break;
    }
}

int32_t ZBALOpAlltoAllV(const void *sendBuff, void *recvBuff, void *sendCumSum, void *recvSplitCount, void *elements,
                        zbal_datatype_t dataType, aclrtStream stream, CommGroupInfo &groupInfo)
{
    static uint32_t blockDim = 0;
    if (blockDim == 0) {
        auto ret = aclrtGetResInCurrentThread(ACL_RT_DEV_RES_VECTOR_CORE, &blockDim);
        if (ret != 0) {
            printf("ZBALOpAlltoAllV get block dim failed, blockDim:%d\n", ret);
            return ret;
        }
    }

    uint8_t *realSendBuff = reinterpret_cast<uint8_t *>(const_cast<void *>(sendBuff));
    uint8_t *realRecvBuff = reinterpret_cast<uint8_t *>(recvBuff);
    uint8_t *realSendCumSum = reinterpret_cast<uint8_t *>(sendCumSum);
    uint8_t *realRecvCount = reinterpret_cast<uint8_t *>(recvSplitCount);
    uint8_t *realElements = reinterpret_cast<uint8_t *>(elements);
    int realDataType = static_cast<int>(dataType);
    uint8_t *metaAddr = reinterpret_cast<uint8_t *>(groupInfo.myMetaGva);
    uint64_t waitSymbol = ++groupInfo.waitSymbol;

    ZBALAlltoAllVInner<<<blockDim, nullptr, stream>>>(realSendBuff, realRecvBuff, realSendCumSum, realRecvCount,
                                                      realElements, realDataType, metaAddr, waitSymbol);
    return 0;
}
