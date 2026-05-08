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
#include "zbal_kernel_utils.h"
#include "zbal_kernel_trace.h"

constexpr uint16_t ZBAL_ALLTOALL_MAX_RANKS = 384;
constexpr uint16_t ZBAL_LT_OFF3 = 3;

class AlltoAllKernel {
public:
    ZBAL_KERNEL AlltoAllKernel() {}

    template<typename T>
    ZBAL_KERNEL void Init(GM_ADDR input, GM_ADDR output, GM_ADDR metaGM, uint64_t elements, uint64_t waitSymbol)
    {
#ifdef __DAV_C220_VEC__
        this->comm = reinterpret_cast<__gm__ CommGroupInfo *>(metaGM);
        this->groupSize = comm->groupSize;
        this->myGroupRank = comm->myGroupRank;
        this->memSize = comm->localDeviceMemSize;
        this->inputAddrSize = groupSize * ZBAL_FLAG_SIZE;
        this->peerRank = reinterpret_cast<__gm__ uint16_t *>(comm->peerGroupRank2WorldRank);
        this->exchangeAddr = comm->myAddressExchangeGva;
        this->paramAddr = comm->myParamDataGva;
        this->inputAddr = reinterpret_cast<__gm__ uint64_t *>(exchangeAddr);
        this->flagAddr = this->inputAddr + inputAddrSize;
        this->statAddr = this->flagAddr + inputAddrSize;
        this->localStatSendAddr = this->statAddr + inputAddrSize;
        this->localStatBaseAddr = this->localStatSendAddr + inputAddrSize;
        this->localStatReadyAddr = this->localStatBaseAddr + inputAddrSize;
        this->input = input;
        this->output = output;
        this->elements = elements;
        this->waitSymbol = waitSymbol;
        this->aivNum = AscendC::GetBlockNum();
#endif
    }

    /**
     * @Brief: each rank write input ptr and flag to all other rank exchange buffer, include self
     */
    ZBAL_KERNEL void Exchange(uint16_t offset)
    {
        ZBAL_PROF_START(comm, ZBAL_PROF_EXCHANGE_ADDR);
        AscendC::LocalTensor<uint64_t> buf(AscendC::TPosition::VECIN, UB_ALIGN_SIZE, UB_PAD_COUNT);

        // write addr
        auto ptr = zbal_ptr(this->inputAddr, myGroupRank, offset, memSize, peerRank);
        SetMetaValue((__gm__ uint64_t *)ptr, myGroupRank, reinterpret_cast<uint64_t>(input), groupSize, buf);

        // write exchangeFlag
        AscendC::PipeBarrier<PIPE_ALL>();
        ptr = zbal_ptr(this->flagAddr, myGroupRank, offset, memSize, peerRank);
        SetMetaValue((__gm__ uint64_t *)ptr, myGroupRank, waitSymbol, groupSize, buf);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_EXCHANGE_ADDR);
    }

    /**
     * @brief: rank's core wait corresponding flag with @offset index on local flag buffer.
     * The flag writen from remote rank, flag ready means target data is ready.
     */
    ZBAL_KERNEL void WaitFlag(const int64_t offset)
    {
        ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_FLAG);
        AscendC::LocalTensor<uint64_t> buf(AscendC::TPosition::VECIN, ZBAL_LT_OFF3 * UB_BUFF_INTERVAL, UB_PAD_COUNT);
        WaitMetaValue(this->flagAddr, offset, waitSymbol, groupSize, buf);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_FLAG);
    }

    ZBAL_KERNEL void Prepare()
    {
        AscendC::LocalTensor<uint64_t> buf1(AscendC::TPosition::VECIN, UB_BUFF_INTERVAL, UB_PAD_COUNT);

        for (uint16_t i = 0; i < groupSize; i++) {
            SetMetaValue(this->localStatSendAddr, i, 0, groupSize, buf1);
            AscendC::PipeBarrier<PIPE_ALL>();

            SetMetaValue(this->localStatBaseAddr, i, 0, groupSize, buf1);
            AscendC::PipeBarrier<PIPE_ALL>();
        }
    }

    ZBAL_KERNEL void ReAssignmentCoreNum()
    {
        if (groupSize <= aivNum && elements < groupSize) {
            aivNum = groupSize;
        }
    }

    ZBAL_KERNEL void GetCoreCommonRangeInfo(uint16_t &commStartRank, uint16_t &commEndRank)
    {
        int64_t blockIdx = AscendC::GetBlockIdx();
        if (groupSize <= aivNum) {
            if (blockIdx < groupSize) {
                commStartRank = blockIdx;
                commEndRank = commStartRank + 1;
            } else {
                commEndRank = groupSize + 1;
                commStartRank = commEndRank + 1;
            }
        } else {
            uint16_t rankPerCore = groupSize / aivNum;
            commStartRank = blockIdx * rankPerCore;
            commEndRank = commStartRank + rankPerCore;
            if (blockIdx == aivNum - 1) {
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

    ZBAL_KERNEL void InitLocalStat()
    {
        ZBAL_PROF_START(comm, ZBAL_PROF_ALLTOALL_INIT_STAT);

        if (AscendC::GetBlockIdx() == 0) {
            uint64_t cumsum[ZBAL_ALLTOALL_MAX_RANKS];
            uint32_t statBase[ZBAL_ALLTOALL_MAX_RANKS];
            AscendC::LocalTensor<uint64_t> buf1(AscendC::TPosition::VECIN, UB_BUFF_INTERVAL, UB_PAD_COUNT);
            for (uint16_t i = 0; i < groupSize; i++) {
                statBase[i] = 1;
                cumsum[i] = (i > 0 ? cumsum[i - 1] : 0) + elements;
            }

            uint16_t core = 1;
            uint64_t avgElement = elements * groupSize / aivNum;
            uint64_t coreRight = avgElement;

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

        AscendC::LocalTensor<uint64_t> buf2(AscendC::TPosition::VECIN, ZBAL_LT_OFF3 * UB_BUFF_INTERVAL, UB_PAD_COUNT);
        WaitMetaValue(this->localStatReadyAddr, 0, waitSymbol, groupSize, buf2);

        ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLTOALL_INIT_STAT);
    }

    ZBAL_KERNEL void GetCoreCopyRangeInfo(uint16_t &copyStartRank, uint64_t &startOffset, uint16_t &copyEndRank)
    {
        ZBAL_PROF_START(comm, ZBAL_PROF_ALLTOALL_CORE_RANGE);

        int64_t coreIndex = AscendC::GetBlockIdx();
        uint64_t curElement = elements * groupSize / aivNum;
        uint64_t curCoreLeft = coreIndex * curElement;
        if (coreIndex == aivNum - 1) {
            curElement = elements * groupSize - (aivNum - 1) * curElement;
        }

        copyStartRank = groupSize + 1;
        copyEndRank = groupSize;

        uint64_t left = 0;
        uint64_t curCoreRight = curCoreLeft + curElement;
        bool findStart = false;
        bool findEnd = false;

        for (uint16_t i = 0; i < groupSize && i < copyEndRank; i++) {
            copyElements[i] = 0;

            uint64_t right = left + elements;
            if (!findStart && right > curCoreLeft) {
                copyStartRank = i;
                startOffset = curCoreLeft - left;
                copyElements[i] = static_cast<uint32_t>(elements - startOffset);
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
                copyElements[i] = static_cast<uint32_t>(elements);
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

    template<typename T>
    ZBAL_KERNEL void AlltoAllCopy(__gm__ T *output)
    {
        uint16_t copyStartRank, copyEndRank;
        uint64_t startOffset;
        GetCoreCopyRangeInfo(copyStartRank, startOffset, copyEndRank);
        ZBAL_PROF_DUMP(comm, __LINE__, copyStartRank, copyEndRank, startOffset);

        AscendC::LocalTensor<uint64_t> buf1(AscendC::TPosition::VECIN, 4*UB_BUFF_INTERVAL, UB_PAD_COUNT);
        uint64_t outputOffset = elements * groupSize / aivNum * AscendC::GetBlockIdx();

        for (uint16_t rank = copyStartRank; rank <= copyEndRank; rank++) {
            ZBAL_PROF_START(comm, ZBAL_PROF_ALLTOALL_COPY);

            uint64_t copyCount = copyElements[rank];
            WaitFlag(rank);

            GetMetaValue(inputAddr, rank, groupSize, buf1);
            __gm__ T *remoteInput = reinterpret_cast<__gm__ T *>(buf1.GetValue(0));
            uint64_t sourceElement = elements * groupSize;
            uint64_t inputOffset = elements * myGroupRank + ((rank == copyStartRank) ? startOffset : 0);

            CpGM2GM(output, elements * groupSize, outputOffset,
                    remoteInput, sourceElement, inputOffset, copyCount);

            outputOffset += copyCount;
            AtomicIncStat(this->localStatSendAddr, rank);
            // }

            ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLTOALL_COPY);
        }
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

            AlltoAllCopy((__gm__ T *)output);

            WriteRangeStat(commonStartRank, commonEndRank);

            WaitRangeStat(commonStartRank, commonEndRank);
        }
#endif
    }

private:
    int64_t aivNum;
    uint16_t groupSize;
    uint16_t myGroupRank;
    uint64_t memSize;
    uint16_t inputAddrSize;
    uint64_t elements;
    uintptr_t exchangeAddr;
    uintptr_t paramAddr;
    __gm__ uint64_t *inputAddr;
    __gm__ uint64_t *flagAddr;
    __gm__ uint64_t *statAddr;
    __gm__ uint64_t *localStatSendAddr;
    __gm__ uint64_t *localStatBaseAddr;
    __gm__ uint64_t *localStatReadyAddr;
    __gm__ void *input;
    __gm__ void *output;
    __gm__ CommGroupInfo *comm;
    __gm__ uint16_t *peerRank;
    uint64_t waitSymbol;
    uint32_t copyElements[ZBAL_ALLTOALL_MAX_RANKS];
};

extern "C" __global__ __aicore__
void ZBALAlltoAllInner(GM_ADDR input, GM_ADDR output, size_t elements, int dataType, GM_ADDR metaAddr, uint64_t symbol)
{
    AlltoAllKernel op;
    zbal_datatype_t ZBAL_DATA_TYPE = static_cast<zbal_datatype_t>(dataType);
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIV_1_0);

    switch (ZBAL_DATA_TYPE) {
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT8:
            op.Init<int8_t>(input, output, metaAddr, elements, symbol);
            op.Process<int8_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT16:
            op.Init<int16_t>(input, output, metaAddr, elements, symbol);
            op.Process<int16_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT32:
            op.Init<int32_t>(input, output, metaAddr, elements, symbol);
            op.Process<int32_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP16:
            op.Init<float16_t>(input, output, metaAddr, elements, symbol);
            op.Process<float16_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP32:
            op.Init<float>(input, output, metaAddr, elements, symbol);
            op.Process<float>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_INT64:
            op.Init<int64_t>(input, output, metaAddr, elements, symbol);
            op.Process<int64_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT64:
            op.Init<uint64_t>(input, output, metaAddr, elements, symbol);
            op.Process<uint64_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT8:
            op.Init<uint8_t>(input, output, metaAddr, elements, symbol);
            op.Process<uint8_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT16:
            op.Init<uint16_t>(input, output, metaAddr, elements, symbol);
            op.Process<uint16_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_UINT32:
            op.Init<uint32_t>(input, output, metaAddr, elements, symbol);
            op.Process<uint32_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_FP64:
            op.Init<float64_t>(input, output, metaAddr, elements, symbol);
            op.Process<float64_t>();
            break;
        case zbal_datatype_t::ZBAL_DATA_TYPE_BFP16:
            op.Init<bfloat16_t>(input, output, metaAddr, elements, symbol);
            op.Process<bfloat16_t>();;
            break;
        default:
            break;
    }
}

int32_t ZBALOpAlltoAllBase(void *sendBuff, void *recvBuff, size_t sendCount, zbal_datatype_t dataType,
                           aclrtStream stream, CommGroupInfo &groupInfo)
{
    static uint32_t blockDim = 0;
    if (blockDim == 0) {
        auto ret = aclrtGetResInCurrentThread(ACL_RT_DEV_RES_VECTOR_CORE, &blockDim);
        if (ret != 0) {
            printf("ZBALOpAlltoAll get block dim failed, blockDim:%d\n", ret);
            return ret;
        }
    }

    uint8_t *realSendBuff = reinterpret_cast<uint8_t *>(sendBuff);
    uint8_t *realRecvBuff = reinterpret_cast<uint8_t *>(recvBuff);
    size_t realCount = sendCount / groupInfo.groupSize;
    int realDataType = static_cast<int>(dataType);
    uint8_t *metaAddr = reinterpret_cast<uint8_t *>(groupInfo.myMetaGva);
    uint64_t waitSymbol = ++groupInfo.waitSymbol;

    ZBALAlltoAllInner<<<blockDim, nullptr, stream>>>(realSendBuff, realRecvBuff, realCount,
                                                     realDataType, metaAddr, waitSymbol);
    return 0;
}
