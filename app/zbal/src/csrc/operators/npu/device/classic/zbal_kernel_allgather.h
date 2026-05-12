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
#ifndef ZBAL_KERNEL_ALLGATHER_H
#define ZBAL_KERNEL_ALLGATHER_H

#include <acl/acl_rt.h>
#include "kernel_operator.h"
#include "zbal_def.h"
#include "zbal_kernel_utils.h"
#include "zbal_kernel_trace.h"

constexpr uint32_t AG_SLICE_PER_CORE = 3;
constexpr uint32_t AG_RING_NUM = 2;

constexpr uint32_t UB_BUFF_SIZE_2X = 2;
constexpr uint32_t UB_BUFF_SIZE_3X = 3;
constexpr uint32_t UB_BUFF_SIZE_4X = 4;
constexpr uint32_t UB_BUFF_SIZE_5X = 5;
constexpr uint32_t UB_BUFF_SIZE_6X = 6;
constexpr uint32_t UB_BUFF_SIZE_7X = 7;

class AllGatherSmallKernel {
public:
    ZBAL_KERNEL AllGatherSmallKernel() {};

    template<typename T>
    ZBAL_KERNEL void Init(GM_ADDR input, GM_ADDR output, GM_ADDR metaGM, uint64_t elements, uint64_t waitSymbol);

    template<typename T>
    ZBAL_KERNEL void Process();

    template<typename T>
    ZBAL_KERNEL void InnerProcessLargeGroupSize();

    template<typename T>
    ZBAL_KERNEL void InnerProcess();

private:
    ZBAL_KERNEL void ExchangeInputAddrFlag();

    ZBAL_KERNEL void WaitFlag(const int64_t targetDataRank);

    ZBAL_KERNEL void WaitStat(const int64_t targetDataRank);

    ZBAL_KERNEL void WriteStat(int64_t targetDataRank = -1);

private:
    uint16_t groupSize;
    uint16_t myGroupRank;
    uint64_t localDeviceMemSize;
    uint16_t inputAddrSize;
    int64_t aivNum;
    uint64_t elements;
    uint64_t waitSymbol;
    uintptr_t exchangeAddr;
    uintptr_t paramAddr;
    __gm__ uint64_t *inputAddr;
    __gm__ uint64_t *flagAddr;
    __gm__ uint64_t *statAddr;
    __gm__ void *input;
    __gm__ void *output;
    __gm__ CommGroupInfo *comm;
    __gm__ uint16_t *peerGroupRank2WorldRank;
};

class AllGatherBigKernel {
public:
    ZBAL_KERNEL AllGatherBigKernel() {};

    template<typename T>
    ZBAL_KERNEL void Init(GM_ADDR input, GM_ADDR output, GM_ADDR metaGM, uint64_t elements, uint64_t waitSymbol);

    template<typename T>
    ZBAL_KERNEL void Process(); // ring allgather

private:
    ZBAL_KERNEL void ExchangeOutputAddr(int64_t coreIndex, __gm__ uint64_t *statAddr, int64_t statUpdateRank);

    template<typename T>
    ZBAL_KERNEL void CopyLocal2Output(__gm__ T *input, __gm__ T *output);

    ZBAL_KERNEL void WaitFlag(const int64_t targetDataRank);

    ZBAL_KERNEL void WaitStat(__gm__ uint64_t *statAddr, int64_t targetDataRank);

    ZBAL_KERNEL void WriteStat(__gm__ uint64_t *statAddr, const int64_t targetStatRank, const int64_t targetStatOffset);

    ZBAL_KERNEL int64_t GetTargetRank(int64_t prev, int64_t next, int64_t aivIndex, int loop);

private:
    uint32_t coreNumPerRing;
    uint32_t statSizePerRank;
    uint16_t groupSize;
    uint16_t myGroupRank;
    uint64_t memSize;
    uint16_t elemExchSize; // size for exchange one element
    uint32_t exchangeMetaSize;
    int64_t aivNum;
    uint64_t elements;
    uintptr_t exchangeAddr;
    __gm__ uint64_t *outputAddr;
    __gm__ uint64_t *flagAddr;
    __gm__ uint64_t *readLeftStatAddr;
    __gm__ uint64_t *readRightStatAddr;
    __gm__ void *input;
    __gm__ void *output;
    __gm__ CommGroupInfo *comm;
    __gm__ uint16_t *worldRanks;
    uint64_t waitSymbol;
};

template<typename T>
ZBAL_KERNEL void AllGatherSmallKernel::Init(GM_ADDR input, GM_ADDR output, GM_ADDR metaGM, uint64_t elements,
                                            uint64_t waitSymbol)
{
#ifdef __DAV_C220_VEC__
    this->comm = reinterpret_cast<__gm__ CommGroupInfo *>(metaGM);
    this->groupSize = comm->groupSize;
    this->myGroupRank = comm->myGroupRank;
    this->localDeviceMemSize = comm->localDeviceMemSize;
    this->inputAddrSize = groupSize * ZBAL_FLAG_SIZE;
    this->peerGroupRank2WorldRank = reinterpret_cast<__gm__ uint16_t *>(comm->peerGroupRank2WorldRank);
    this->exchangeAddr = comm->myAddressExchangeGva;
    this->inputAddr = reinterpret_cast<__gm__ uint64_t *>(exchangeAddr);
    this->flagAddr = this->inputAddr + inputAddrSize;
    this->statAddr = this->flagAddr + inputAddrSize;
    this->paramAddr = comm->myParamDataGva;
    this->aivNum = AscendC::GetBlockNum(); // * AscendC::GetTaskRation()
    this->input = input;
    this->output = output;
    this->elements = elements;
    this->waitSymbol = waitSymbol;
#endif
}

ZBAL_KERNEL void AllGatherSmallKernel::ExchangeInputAddrFlag()
{
    ZBAL_PROF_START(comm, ZBAL_PROF_EXCHANGE_ADDR);
    int64_t aivIndex = AscendC::GetBlockIdx();
    if (aivNum < groupSize) {
        uint32_t ranksPerCore = (groupSize + aivNum - 1) / aivNum;
        const int64_t startRank = aivIndex * ranksPerCore;
        int64_t endRank = startRank + ranksPerCore;
        if (endRank > groupSize) {
            endRank = groupSize;
        }
        AscendC::LocalTensor<uint64_t> inputInBuff(AscendC::TPosition::VECIN, UB_ALIGN_SIZE, UB_PAD_COUNT);
        AscendC::LocalTensor<uint64_t> flagInBuff(AscendC::TPosition::VECIN, UB_BUFF_INTERVAL + UB_ALIGN_SIZE,
                                                  UB_PAD_COUNT);
        for (int dstRank = startRank; dstRank < endRank; dstRank++) {
            auto ptr = zbal_ptr(inputAddr, myGroupRank, dstRank, localDeviceMemSize, peerGroupRank2WorldRank);
            SetMetaValue((__gm__ uint64_t *)ptr, myGroupRank, reinterpret_cast<uint64_t>(input), groupSize,
                         inputInBuff);

            AscendC::PipeBarrier<PIPE_ALL>();
            SetMetaValue((__gm__ uint64_t *)ptr + inputAddrSize, myGroupRank, waitSymbol, groupSize, flagInBuff);
        }
    } else if (aivIndex < groupSize) {
        AscendC::LocalTensor<uint64_t> inputInBuff(AscendC::TPosition::VECIN, UB_ALIGN_SIZE, UB_PAD_COUNT);
        AscendC::LocalTensor<uint64_t> flagInBuff(AscendC::TPosition::VECIN, UB_BUFF_INTERVAL + UB_ALIGN_SIZE,
                                                  UB_PAD_COUNT);

        // write addr
        auto ptr = zbal_ptr(inputAddr, myGroupRank, aivIndex, localDeviceMemSize, peerGroupRank2WorldRank);
        SetMetaValue((__gm__ uint64_t *)ptr, myGroupRank, reinterpret_cast<uint64_t>(input), groupSize, inputInBuff);

        // write exchangeFlag
        AscendC::PipeBarrier<PIPE_ALL>();
        SetMetaValue((__gm__ uint64_t *)ptr + inputAddrSize, myGroupRank, waitSymbol, groupSize, flagInBuff);
    }
    ZBAL_PROF_STOP(comm, ZBAL_PROF_EXCHANGE_ADDR);
}

/**
    * @brief: rank's core wait corresponding flag with @targetDataRank index on local flag buffer.
    * The flag writen from remote rank, flag ready means target data is ready.
    */
ZBAL_KERNEL void AllGatherSmallKernel::WaitFlag(const int64_t targetDataRank)
{
    ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_FLAG);
    AscendC::LocalTensor<uint64_t> flagOutBuff(AscendC::TPosition::VECIN,
                                               UB_BUFF_SIZE_3X * UB_BUFF_INTERVAL + UB_ALIGN_SIZE, UB_PAD_COUNT);
    WaitMetaValue(flagAddr, targetDataRank, waitSymbol, groupSize, flagOutBuff);
    ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_FLAG);
}

/**
    * @brief: write stat to corresponding remote rank stat buffer when finish data copying as a hint to remote rank
    */
ZBAL_KERNEL void AllGatherSmallKernel::WriteStat(int64_t targetDataRank)
{
    ZBAL_PROF_START(comm, ZBAL_PROF_WRITE_STAT);
    const int64_t aivIndex = (targetDataRank == -1) ? AscendC::GetBlockIdx() : 0;
    const int64_t corePerRank = AscendC::GetBlockNum() / groupSize;
    const int64_t offset = (targetDataRank == -1) ? aivIndex / corePerRank : targetDataRank;
    if (aivIndex % corePerRank == 0) {
        AscendC::LocalTensor<uint64_t> statWriteBuff(AscendC::TPosition::VECIN,
                                                     UB_BUFF_SIZE_5X * UB_BUFF_INTERVAL + UB_ALIGN_SIZE, UB_PAD_COUNT);
        auto destStatPtr = zbal_ptr(this->statAddr, myGroupRank, offset, localDeviceMemSize, peerGroupRank2WorldRank);
        SetMetaValue((__gm__ uint64_t *)destStatPtr, myGroupRank, waitSymbol, groupSize, statWriteBuff);
    }
    ZBAL_PROF_STOP(comm, ZBAL_PROF_WRITE_STAT);
}

/**
    * @brief: rank's core wait a stat with @targetDataRank index on local stat buffer.
    * One core wait one stat, as the last core finish wait, which mean no rank need local data, current kernel can ends.
    */
ZBAL_KERNEL void AllGatherSmallKernel::WaitStat(const int64_t targetDataRank)
{
    ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_STAT);
    AscendC::LocalTensor<uint64_t> statCheckBuff(AscendC::TPosition::VECIN,
                                                 UB_BUFF_SIZE_6X * UB_BUFF_INTERVAL + UB_ALIGN_SIZE, UB_PAD_COUNT);
    WaitMetaValue(statAddr, targetDataRank, waitSymbol, groupSize, statCheckBuff);
    ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_STAT);
}

template<typename T>
ZBAL_KERNEL void AllGatherSmallKernel::InnerProcessLargeGroupSize()
{
    // tiling parameters
    const int64_t aivIndex = AscendC::GetBlockIdx();
    int64_t ranksPerCore = groupSize / aivNum;
    int64_t remainder = groupSize % aivNum;
    if (aivIndex < remainder) {
        ranksPerCore += 1;
    }
    int64_t startRank = aivIndex >= remainder ? aivIndex * ranksPerCore + remainder : aivIndex * ranksPerCore;
    int64_t endRank = startRank + ranksPerCore;

    for (int64_t offset = startRank; offset < endRank; offset++) {
        uint32_t outputOffset = offset * elements;
        uint32_t inputOffset = 0;

        WaitFlag(offset);

        ZBAL_PROF_START(comm, ZBAL_PROF_ALLGATHER_PREPARE_PTR);
        AscendC::LocalTensor<uint64_t> inputOutBuff(AscendC::TPosition::VECIN,
                                                    UB_BUFF_SIZE_4X * UB_BUFF_INTERVAL + UB_ALIGN_SIZE, UB_PAD_COUNT);
        AscendC::PipeBarrier<PIPE_ALL>();
        GetMetaValue(inputAddr, offset, groupSize, inputOutBuff);
        SyncFunc<AscendC::HardEvent::MTE2_S>(EVENT_ID0);

        AscendC::GlobalTensor<T> outputGT;
        outputGT.SetGlobalBuffer((__gm__ T *)output, elements * groupSize);
        AscendC::GlobalTensor<T> inputGT;
        inputGT.SetGlobalBuffer((__gm__ T *)inputOutBuff.GetValue(0), elements);
        AscendC::PipeBarrier<PIPE_ALL>();
        ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLGATHER_PREPARE_PTR);

        ZBAL_PROF_START(comm, ZBAL_PROF_ALLGATHER_COPY);
        CpGM2GM(outputGT[outputOffset], inputGT[inputOffset], elements);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLGATHER_COPY);

        WriteStat(offset);

        WaitStat(offset);
    }
}

template<typename T>
ZBAL_KERNEL void AllGatherSmallKernel::InnerProcess()
{
    // tiling parameters
    int64_t offset;
    int64_t corePerRank;
    int64_t coreRankIdx;
    const int64_t aivIndex = AscendC::GetBlockIdx();
    const int64_t baseCorePerRank = aivNum / groupSize;
    const int64_t remainder = aivNum % groupSize;
    if (aivIndex < remainder * (baseCorePerRank + 1)) {
        offset = aivIndex / (baseCorePerRank + 1);
        corePerRank = baseCorePerRank + 1;
    } else {
        int64_t adjustedIndex = aivIndex - remainder * (baseCorePerRank + 1);
        offset = remainder + adjustedIndex / baseCorePerRank;
        corePerRank = baseCorePerRank;
    }
    if (offset < remainder) {
        coreRankIdx = aivIndex % (baseCorePerRank + 1);
    } else {
        coreRankIdx = (aivIndex - remainder * (baseCorePerRank + 1)) % baseCorePerRank;
    }
    uint32_t numPerCore = elements / corePerRank;
    uint32_t inputOffset = coreRankIdx * numPerCore;
    uint32_t outputOffset = offset * elements + inputOffset;
    if (coreRankIdx == corePerRank - 1) {
        numPerCore = elements - (corePerRank - 1) * numPerCore;
    }

    WaitFlag(offset);

    ZBAL_PROF_START(comm, ZBAL_PROF_ALLGATHER_PREPARE_PTR);
    AscendC::LocalTensor<uint64_t> inputOutBuff(AscendC::TPosition::VECIN,
                                                UB_BUFF_SIZE_4X * UB_BUFF_INTERVAL + UB_ALIGN_SIZE, UB_PAD_COUNT);
    AscendC::PipeBarrier<PIPE_ALL>();
    GetMetaValue(inputAddr, offset, groupSize, inputOutBuff);
    SyncFunc<AscendC::HardEvent::MTE2_S>(EVENT_ID0);

    AscendC::GlobalTensor<T> outputGT;
    outputGT.SetGlobalBuffer((__gm__ T *)output, elements * groupSize);
    AscendC::GlobalTensor<T> inputGT;
    inputGT.SetGlobalBuffer((__gm__ T *)inputOutBuff.GetValue(0), elements);
    AscendC::PipeBarrier<PIPE_ALL>();
    ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLGATHER_PREPARE_PTR);

    ZBAL_PROF_START(comm, ZBAL_PROF_ALLGATHER_COPY);
    CpGM2GM(outputGT[outputOffset], inputGT[inputOffset], numPerCore);
    ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLGATHER_COPY);

    WriteStat();

    WaitStat(offset);
    AscendC::SyncAll<true>();
}

template<typename T>
ZBAL_KERNEL void AllGatherSmallKernel::Process()
{
#ifdef __DAV_C220_VEC__
    ZBAL_PROF_START(comm, ZBAL_PROF_ALLGATHER_KERNEL_ALL);

    ExchangeInputAddrFlag();

    if (aivNum < groupSize) {
        InnerProcessLargeGroupSize<T>();
    } else {
        InnerProcess<T>();
    }
    ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLGATHER_KERNEL_ALL);
#endif
}

template<typename T>
ZBAL_KERNEL void AllGatherBigKernel::Init(GM_ADDR input, GM_ADDR output, GM_ADDR metaGM, uint64_t elements,
                                          uint64_t waitSymbol)
{
#ifdef __DAV_C220_VEC__
    this->aivNum = AscendC::GetBlockNum();
    this->coreNumPerRing = aivNum / AG_RING_NUM;
    this->statSizePerRank = coreNumPerRing * AG_SLICE_PER_CORE; // left right stat has same size
    this->comm = reinterpret_cast<__gm__ CommGroupInfo *>(metaGM);
    this->groupSize = comm->groupSize;
    this->myGroupRank = comm->myGroupRank;
    this->memSize = comm->localDeviceMemSize;
    this->worldRanks = reinterpret_cast<__gm__ uint16_t *>(comm->peerGroupRank2WorldRank);
    this->elemExchSize = groupSize * ZBAL_FLAG_SIZE;
    this->exchangeAddr = comm->myAddressExchangeGva;
    // |----------|----------|--------------------|--------------------|
    // outputAddr  flagAddr  readLeftStatAddr     readRightStatAddr
    this->outputAddr = reinterpret_cast<__gm__ uint64_t *>(exchangeAddr);
    this->flagAddr = outputAddr + elemExchSize;
    this->readLeftStatAddr = flagAddr + elemExchSize;
    this->readRightStatAddr = readLeftStatAddr + statSizePerRank * elemExchSize;
    this->exchangeMetaSize = 2 * (elemExchSize + statSizePerRank * elemExchSize);
    this->input = input;
    this->output = output;
    this->elements = elements;
    this->waitSymbol = waitSymbol;
#endif
}

ZBAL_KERNEL void AllGatherBigKernel::ExchangeOutputAddr(int64_t coreIndex, __gm__ uint64_t *statAddr,
                                                        int64_t statUpdateRank)
{
    // The stat area of the first cycle is set to ready in advance.
    ZBAL_PROF_START(comm, ZBAL_PROF_WRITE_STAT);
    for (int j = 0; j < AG_SLICE_PER_CORE; j++) {
        AscendC::LocalTensor<uint64_t> writeBuff(AscendC::TPosition::VECIN,
                                                 UB_BUFF_SIZE_2X * UB_BUFF_INTERVAL + UB_ALIGN_SIZE, UB_PAD_COUNT);
        auto targetStatAddr = zbal_ptr(statAddr, myGroupRank, statUpdateRank, memSize, worldRanks);
        int64_t writeStatOffset = myGroupRank * this->statSizePerRank + coreIndex * AG_SLICE_PER_CORE + j;
        SetMetaValue((__gm__ uint64_t *)targetStatAddr, writeStatOffset, waitSymbol, groupSize, writeBuff);
        AscendC::PipeBarrier<PIPE_ALL>();
    }
    ZBAL_PROF_STOP(comm, ZBAL_PROF_WRITE_STAT);

    ZBAL_PROF_START(comm, ZBAL_PROF_ALLGATHER_PREPARE_PTR);
    AscendC::LocalTensor<uint64_t> outputInBuff(AscendC::TPosition::VECIN, UB_ALIGN_SIZE, UB_PAD_COUNT);
    AscendC::LocalTensor<uint64_t> flagInBuff(AscendC::TPosition::VECIN, UB_BUFF_INTERVAL + UB_ALIGN_SIZE,
                                              UB_PAD_COUNT);

    if (coreIndex == 0) {
        auto ptr = zbal_ptr(this->outputAddr, myGroupRank, statUpdateRank, memSize, worldRanks);
        SetMetaValue((__gm__ uint64_t *)ptr, myGroupRank, reinterpret_cast<uint64_t>(output), groupSize, outputInBuff);
        AscendC::PipeBarrier<PIPE_ALL>();
        SetMetaValue((__gm__ uint64_t *)ptr + this->elemExchSize, myGroupRank, waitSymbol, groupSize, flagInBuff);
    }
    ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLGATHER_PREPARE_PTR);
}

template<typename T>
ZBAL_KERNEL void AllGatherBigKernel::CopyLocal2Output(__gm__ T *input, __gm__ T *output)
{
    ZBAL_PROF_START(comm, ZBAL_PROF_ALLGATHER_LOCAL_COPY);
    const int64_t coreIndex = AscendC::GetBlockIdx();
    uint32_t numPerCore = elements / this->aivNum;
    const uint32_t outputOffset = myGroupRank * elements + coreIndex * numPerCore;
    const uint32_t inputOffset = coreIndex * numPerCore;
    if (coreIndex == this->aivNum - 1) {
        numPerCore = elements - (this->aivNum - 1) * numPerCore;
    }

    SyncFunc<AscendC::HardEvent::MTE2_S>(EVENT_ID0);
    AscendC::GlobalTensor<T> outputGT;
    outputGT.SetGlobalBuffer(output, elements * groupSize);
    AscendC::GlobalTensor<T> inputGT;
    inputGT.SetGlobalBuffer(input, elements);

    CpGM2GM(outputGT[outputOffset], inputGT[inputOffset], numPerCore);
    ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLGATHER_LOCAL_COPY);
}

ZBAL_KERNEL int64_t AllGatherBigKernel::GetTargetRank(int64_t prev, int64_t next, int64_t aivIndex, int loop)
{
    if (aivIndex < coreNumPerRing) {
        return (prev + groupSize - loop) % groupSize;
    } else {
        return (next + loop) % groupSize;
    }
}

ZBAL_KERNEL void AllGatherBigKernel::WaitFlag(const int64_t targetDataRank)
{
    ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_FLAG);
    AscendC::LocalTensor<uint64_t> flagBuff(AscendC::TPosition::VECIN,
                                            UB_BUFF_SIZE_5X * UB_BUFF_INTERVAL + UB_ALIGN_SIZE, UB_PAD_COUNT);
    WaitMetaValue(this->flagAddr, targetDataRank, waitSymbol, groupSize, flagBuff);
    ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_FLAG);
}

ZBAL_KERNEL void AllGatherBigKernel::WaitStat(__gm__ uint64_t *statAddr, int64_t targetDataRank)
{
    ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_STAT);
    AscendC::LocalTensor<uint64_t> statOutBuff(AscendC::TPosition::VECIN,
                                               UB_BUFF_SIZE_4X * UB_BUFF_INTERVAL + UB_ALIGN_SIZE, UB_PAD_COUNT);
    WaitMetaValue(statAddr, targetDataRank, waitSymbol, groupSize, statOutBuff);
    ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_STAT);
}

ZBAL_KERNEL void AllGatherBigKernel::WriteStat(__gm__ uint64_t *statAddr, const int64_t targetStatRank,
                                               const int64_t targetStatOffset)
{
    ZBAL_PROF_START(comm, ZBAL_PROF_WRITE_STAT);
    AscendC::LocalTensor<uint64_t> statWriteBuff(AscendC::TPosition::VECIN,
                                                 UB_BUFF_SIZE_7X * UB_BUFF_INTERVAL + UB_ALIGN_SIZE, UB_PAD_COUNT);
    auto nextStat = zbal_ptr(statAddr, myGroupRank, targetStatRank, memSize, worldRanks);
    SetMetaValue((__gm__ uint64_t *)nextStat, targetStatOffset, waitSymbol, groupSize, statWriteBuff);
    ZBAL_PROF_STOP(comm, ZBAL_PROF_WRITE_STAT);
}

template<typename T>
ZBAL_KERNEL void AllGatherBigKernel::Process() // ring allgather
{
#ifdef __DAV_C220_VEC__
    ZBAL_PROF_START(comm, ZBAL_PROF_ALLGATHER_KERNEL_ALL);
    const int64_t aivIndex = AscendC::GetBlockIdx();
    const int64_t prevRank = (myGroupRank + groupSize - 1) % groupSize;
    const int64_t nextRank = (myGroupRank + 1) % groupSize;
    const int64_t elemPerRing = elements / AG_RING_NUM;
    const int64_t curRingElem = aivIndex < coreNumPerRing ? elemPerRing : elements - elemPerRing;
    const int64_t coreIndex = aivIndex % coreNumPerRing;
    const int64_t elemExtraOffset = aivIndex < coreNumPerRing ? 0 : elemPerRing;
    const int64_t inputPtrIndex = aivIndex < coreNumPerRing ? prevRank : nextRank;
    const int64_t statUpdateRank = aivIndex < coreNumPerRing ? nextRank : prevRank;
    __gm__ uint64_t *statAddr = aivIndex < coreNumPerRing ? this->readLeftStatAddr : this->readRightStatAddr;

    ClearExchangeMeta(outputAddr, exchangeMetaSize);
    BarrierAll(comm);

    CopyLocal2Output((__gm__ T *)input, (__gm__ T *)output); // copy self input to output buffer

    // notify the corresponding rank that the data is ready according to the forward or backward ring.
    ExchangeOutputAddr(coreIndex, statAddr, statUpdateRank);

    WaitFlag(inputPtrIndex);

    AscendC::LocalTensor<uint64_t> inputOutBuff(AscendC::TPosition::VECIN,
                                                UB_BUFF_SIZE_6X * UB_BUFF_INTERVAL + UB_ALIGN_SIZE, UB_PAD_COUNT);
    AscendC::PipeBarrier<PIPE_ALL>();
    GetMetaValue(this->outputAddr, inputPtrIndex, groupSize, inputOutBuff);
    SyncFunc<AscendC::HardEvent::MTE2_S>(EVENT_ID0);
    const uint64_t curInput = inputOutBuff.GetValue(0);

    for (int i = 0; i < groupSize - 1; i++) {
        const int64_t readRank = GetTargetRank(prevRank, nextRank, aivIndex, i);
        uint32_t elemPerCore = curRingElem / coreNumPerRing;
        const uint32_t outputOffset = readRank * elements + coreIndex * elemPerCore + elemExtraOffset;
        if (coreIndex == coreNumPerRing - 1) {
            elemPerCore = curRingElem - (coreNumPerRing - 1) * elemPerCore;
        }

        for (int j = 0; j < AG_SLICE_PER_CORE; j++) {
            // current slice copy elements
            uint32_t elemPerSlice = elemPerCore / AG_SLICE_PER_CORE;
            const uint32_t sliceDataOffset = outputOffset + j * elemPerSlice; // read from prev/next same offset
            if (j == AG_SLICE_PER_CORE - 1) {
                elemPerSlice = elemPerCore - (AG_SLICE_PER_CORE - 1) * elemPerSlice;
            }

            int64_t sliceStatOffset = readRank * this->statSizePerRank + coreIndex * AG_SLICE_PER_CORE + j;

            WaitStat(statAddr, sliceStatOffset);

            ZBAL_PROF_START(comm, ZBAL_PROF_ALLGATHER_COPY);
            AscendC::GlobalTensor<T> outputGT;
            outputGT.SetGlobalBuffer((__gm__ T *)output, elements * groupSize);
            AscendC::GlobalTensor<T> inputGT;
            inputGT.SetGlobalBuffer((__gm__ T *)curInput, elements * groupSize);
            CpGM2GM(outputGT[sliceDataOffset], inputGT[sliceDataOffset], elemPerSlice);
            ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLGATHER_COPY);

            WriteStat(statAddr, statUpdateRank, sliceStatOffset); // write stat to next rank when data ready
        }
    }
    BarrierAll(comm);
    ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLGATHER_KERNEL_ALL);
#endif
}

#endif // ZBAL_KERNEL_ALLGATHER_H
