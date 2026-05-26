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

using namespace zbal;
constexpr uint16_t ZBAL_INOUT_LEN = 2;

template<typename T>
class AlltoAllVKernel : public BaseKernel {
public:
    ZBAL_KERNEL AlltoAllVKernel() {}

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

        pipe.InitBuffer(elementsBuf_, Ceil(ZBAL_INOUT_LEN * sizeof(uint64_t), UB_ALIGN_SIZE) * UB_ALIGN_SIZE);
        pipe.InitBuffer(outputCountsBuf_, Ceil(groupSize * sizeof(uint64_t), UB_ALIGN_SIZE) * UB_ALIGN_SIZE);

        uint32_t int32BufSize = Ceil(sizeof(int32_t), UB_ALIGN_SIZE) * UB_ALIGN_SIZE;
        pipe.InitBuffer(atomicIncQue_, 1, int32BufSize);
        pipe.InitBuffer(waitLocalStatBuf_, int32BufSize);
        pipe.InitBuffer(getLocalStatBuf_, int32BufSize);

        BaseKernel::Init();
#endif
    }

    ZBAL_KERNEL void Prepare()
    {
        AscendC::LocalTensor<uint64_t> elementsLT = elementsBuf_.Get<uint64_t>();
        AscendC::GlobalTensor<uint64_t> elementsGT;
        elementsGT.SetGlobalBuffer(reinterpret_cast<__gm__ uint64_t *>(elements), ZBAL_INOUT_LEN);
        AscendC::DataCopyExtParams copyParams{1U, static_cast<uint32_t>(ZBAL_INOUT_LEN * sizeof(uint64_t)), 0U, 0U, 0U};
        AscendC::DataCopyPadExtParams<uint64_t> padExtParams{false, 0U, 0U, 0U};
        AscendC::DataCopyPad(elementsLT, elementsGT, copyParams, padExtParams);
        this->inputElement = elementsLT.GetValue(0);
        this->outputElement = elementsLT.GetValue(1);

        AscendC::LocalTensor<uint64_t> outputCountsLT = outputCountsBuf_.Get<uint64_t>();
        AscendC::GlobalTensor<uint64_t> outputCountsGT;
        outputCountsGT.SetGlobalBuffer(reinterpret_cast<__gm__ uint64_t *>(outputCounts), groupSize);
        AscendC::DataCopyExtParams copyParams2{1U, static_cast<uint32_t>(groupSize * sizeof(uint64_t)), 0U, 0U, 0U};
        AscendC::DataCopyPadExtParams<uint64_t> padExtParams2{false, 0U, 0U, 0U};
        AscendC::DataCopyPad(outputCountsLT, outputCountsGT, copyParams2, padExtParams2);

        for (uint16_t i = 0; i < groupSize; i++) {
            SetFlag(this->localStatSendAddr, 0, i);
            SetFlag(this->localStatBaseAddr, 0, i);
            outputCountsArray[i] = outputCountsLT.GetValue(i);
            copyElements[i] = 0;
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
        // exchange input addr
        auto ptr = zbal_ptr(this->inputAddr, myGroupRank, offset, memSize, peerRank);
        SetFlag(ptr, reinterpret_cast<uint64_t>(input), myGroupRank);

        // exchange inputCount
        ptr = zbal_ptr(this->inputCumSumAddr, myGroupRank, offset, memSize, peerRank);
        SetFlag(ptr, reinterpret_cast<uint64_t>(inputCumSum), myGroupRank);

        // exchange input elements
        ptr = zbal_ptr(this->inputElementAddr, myGroupRank, offset, memSize, peerRank);
        SetFlag(ptr, inputElement, myGroupRank);

        // write exchangeFlag
        ptr = zbal_ptr(this->flagAddr, myGroupRank, offset, memSize, peerRank);
        SetFlag(ptr, waitSymbol, myGroupRank);
    }

    ZBAL_KERNEL void GetInputInfo(uint16_t offset, uint64_t &inputOff, uint64_t &srcElement, __gm__ void **srcAddress)
    {
        uint64_t srcCumSumAddr = GetFlag(inputCumSumAddr, offset);
        inputOff = GetFlag(reinterpret_cast<__gm__ uint64_t *>(srcCumSumAddr), myGroupRank);
        srcElement = GetFlag(inputElementAddr, offset);
        *srcAddress = reinterpret_cast<__gm__ void *>(GetFlag(this->inputAddr, offset));
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
            uint16_t remainder = groupSize % aivNum;
            uint16_t coreIdx = AscendC::GetBlockIdx();
            if (coreIdx < remainder) {
                commStartRank = coreIdx * (rankPerCore + 1);
                commEndRank = commStartRank + rankPerCore + 1;
            } else {
                commStartRank = coreIdx * rankPerCore + remainder;
                commEndRank = commStartRank + rankPerCore;
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
        // condition: outputElement < aivNum, let the last core do all copy
        if (curCoreLeft == curCoreRight && curCoreRight == 0) {
            return;
        }

        ZBAL_PROF_START(comm, ZBAL_PROF_ALLTOALL_CORE_RANGE);
        bool findStart = false;
        bool findEnd = false;
        for (uint16_t i = 0; i < groupSize && i < copyEndRank; i++) {
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
                break;
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

        AscendC::LocalTensor<int32_t> buf = atomicIncQue_.AllocTensor<int32_t>();
        buf.SetValue(0, 1);

        AscendC::DataCopyExtParams dataCopyParams(1, sizeof(int32_t), 0, 0, 0);
        AscendC::DataCopyPad(outputGT, buf, dataCopyParams);

        AscendC::SetAtomicNone();

        atomicIncQue_.EnQue(buf);
        buf = atomicIncQue_.DeQue<int32_t>();
        atomicIncQue_.FreeTensor(buf);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLTOALL_ATOMIC_INC);
    }

    ZBAL_KERNEL int32_t GetLocalStat(__gm__ uint64_t *stat, uint16_t offset)
    {
        __gm__ uint64_t *target = stat + offset * ZBAL_FLAG_SIZE;

        AscendC::LocalTensor<int32_t> buf = getLocalStatBuf_.Get<int32_t>();

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

        AscendC::LocalTensor<int32_t> buf = waitLocalStatBuf_.Get<int32_t>();

        GlobalTensor<int32_t> globalGT;
        globalGT.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(target), ZBAL_FLAG_SIZE * sizeof(uint64_t));
        SyncFunc<AscendC::HardEvent::MTE3_MTE2>(EVENT_ID0);
        int32_t base = GetLocalStat(localStatBaseAddr, offset);
        while (true) {
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
            for (uint16_t i = 0; i < groupSize; i++) {
                statBase[i] = (outputCountsArray[i] > 0 ? 1 : 0);
                cumsum[i] = (i > 0 ? cumsum[i - 1] : 0) + outputCountsArray[i];
            }

            uint16_t core = 1;
            uint64_t avgElement = outputElement / aivNum;
            uint64_t coreRight = avgElement;

            // if avgElement eq 0, last core do all copy
            if (avgElement > 0) {
                // count split by core has more local stat, O(groupSize + aivNum)
                for (uint16_t k = 0; k < groupSize; k++) {
                    while (cumsum[k] >= coreRight) {
                        if (cumsum[k] > coreRight) {
                            statBase[k] += 1;
                        }
                        core += 1;
                        if (core >= aivNum) {
                            k += groupSize;     // exit outer loop
                            break;
                        }
                        coreRight += avgElement;
                    }
                }
            }

            for (uint16_t m = 0; m < groupSize; m++) {
                SetFlag(this->localStatBaseAddr, statBase[m], m);
            }

            SetFlag(this->localStatReadyAddr, waitSymbol, 0);
        }

        WaitFlag(this->localStatReadyAddr, waitSymbol, 0);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLTOALL_INIT_STAT);
    }

    ZBAL_KERNEL void AlltoAllvCopy()
    {
        uint16_t copyStartRank;
        uint16_t copyEndRank;
        uint64_t startOffset;
        GetCoreCopyRangeInfo(copyStartRank, startOffset, copyEndRank);

        uint64_t outputOffset = outputElement / aivNum * AscendC::GetBlockIdx();
        for (uint16_t rank = copyStartRank; rank <= copyEndRank; rank++) {
            ZBAL_PROF_START(comm, ZBAL_PROF_ALLTOALL_COPY);
            uint64_t copyCount = copyElements[rank];
            if (copyCount > 0) {
                ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_FLAG);
                WaitFlag(this->flagAddr, waitSymbol, rank);
                ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_FLAG);

                uint64_t inputOffset;
                uint64_t sourceElement;
                __gm__ void *remoteInput;
                GetInputInfo(rank, inputOffset, sourceElement, &remoteInput);
                inputOffset = inputOffset + ((rank == copyStartRank) ? startOffset : 0);

                AscendC::GlobalTensor<T> outputGT;
                outputGT.SetGlobalBuffer((__gm__ T *)output, outputElement);
                AscendC::GlobalTensor<T> inputGT;
                inputGT.SetGlobalBuffer((__gm__ T *)remoteInput, sourceElement);
                CpGM2GM(inputGT[inputOffset], outputGT[outputOffset], copyCount);

                outputOffset += copyCount;
                AtomicIncStat(this->localStatSendAddr, rank);
            }

            ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLTOALL_COPY);
        }
    }

    ZBAL_KERNEL void WriteRangeStat(uint16_t commonStartRank, uint16_t commonEndRank)
    {
        for (uint16_t k = commonStartRank; k < commonEndRank; k++) {
            WaitLocalStat(this->localStatSendAddr, k);

            auto ptr = zbal_ptr(this->statAddr, myGroupRank, static_cast<int>(k), memSize, peerRank);
            SetFlag(ptr, waitSymbol, myGroupRank);
        }
    }

    ZBAL_KERNEL void WaitRangeStat(uint16_t commonStartRank, uint16_t commonEndRank)
    {
        for (uint16_t k = commonStartRank; k < commonEndRank; k++) {
            ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_STAT);
            WaitFlag(this->statAddr, waitSymbol, k);
            ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_STAT);
        }
    }

    ZBAL_KERNEL void Process()
    {
#ifdef __DAV_C220_VEC__
        ZBAL_PROF_START(comm, ZBAL_PROF_ALLTOALL_KERNEL_ALL);

        Prepare();

        ReAssignmentCoreNum();

        if (AscendC::GetBlockIdx() < aivNum) {
            uint16_t commonStartRank;
            uint16_t commonEndRank;
            GetCoreCommonRangeInfo(commonStartRank, commonEndRank);

            P2MPExchange(commonStartRank, commonEndRank);

            InitLocalStat();

            AlltoAllvCopy();

            WriteRangeStat(commonStartRank, commonEndRank);

            WaitRangeStat(commonStartRank, commonEndRank);
        }

        BarrierAll(comm);

        ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLTOALL_KERNEL_ALL);
#endif
    }

private:
    int64_t aivNum;
    TBuf<> elementsBuf_;
    TBuf<> outputCountsBuf_;
    TQue<QuePosition::VECIN, 1> atomicIncQue_;
    TBuf<> getLocalStatBuf_;
    TBuf<> waitLocalStatBuf_;
    uint16_t groupSize;
    uint16_t myGroupRank;
    uint64_t memSize;
    uintptr_t exchangeAddr;
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

#define ZBAL_ALLTOALLV_TYPE_MAP(F)     \
    F(int8_t, ZBAL_DATA_TYPE_INT8)     \
    F(int16_t, ZBAL_DATA_TYPE_INT16)   \
    F(int32_t, ZBAL_DATA_TYPE_INT32)   \
    F(float16_t, ZBAL_DATA_TYPE_FP16)  \
    F(float, ZBAL_DATA_TYPE_FP32)      \
    F(int64_t, ZBAL_DATA_TYPE_INT64)   \
    F(uint64_t, ZBAL_DATA_TYPE_UINT64) \
    F(uint8_t, ZBAL_DATA_TYPE_UINT8)   \
    F(uint16_t, ZBAL_DATA_TYPE_UINT16) \
    F(uint32_t, ZBAL_DATA_TYPE_UINT32) \
    F(float64_t, ZBAL_DATA_TYPE_FP64)  \
    F(bfloat16_t, ZBAL_DATA_TYPE_BFP16)

#define ZBAL_ALLTOALLV_CASE(TYPE, ENUM)                                                    \
    case zbal_datatype_t::ENUM: {                                                          \
        AlltoAllVKernel<TYPE> op;                                                          \
        op.Init(input, output, metaAddr, inputCumSum, outputCounts, elements, waitSymbol); \
        op.Process();                                                                      \
        break;                                                                             \
    }

extern "C" __global__ __aicore__ void ZBALAlltoAllVInner(GM_ADDR input, GM_ADDR output, GM_ADDR inputCumSum,
                                                         GM_ADDR outputCounts, GM_ADDR elements, int dataType,
                                                         GM_ADDR metaAddr, uint64_t waitSymbol)
{
    zbal_datatype_t ZBAL_DATA_TYPE = static_cast<zbal_datatype_t>(dataType);
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIV_1_0);

    switch (ZBAL_DATA_TYPE) {
        ZBAL_ALLTOALLV_TYPE_MAP(ZBAL_ALLTOALLV_CASE)
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
