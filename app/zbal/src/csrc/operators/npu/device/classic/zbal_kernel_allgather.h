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

#include "zbal_kernel_base.h"

constexpr uint32_t ZBAL_AG_SLICE_PER_CORE = ZBAL_CONST_3;
constexpr uint32_t ZBAL_AG_RING_NUM = ZBAL_CONST_2;

class ZBALAllGatherSmallKernel : public ZBALBaseKernel {
public:
    ZBAL_KERNEL ZBALAllGatherSmallKernel() {};

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

class ZBALAllGatherBigKernel : public ZBALBaseKernel {
public:
    ZBAL_KERNEL ZBALAllGatherBigKernel() {};

    template<typename T>
    ZBAL_KERNEL void Init(GM_ADDR input, GM_ADDR output, GM_ADDR metaGM, uint64_t elements, uint64_t waitSymbol);

    template<typename T>
    ZBAL_KERNEL void Process(); // ring allgather

private:
    ZBAL_KERNEL void ExchangeOutputAddr(int64_t coreIndex, __gm__ uint64_t *statAddr, int64_t statUpdateRank);

    template<typename T>
    ZBAL_KERNEL void CopyLocal2Output(__gm__ T *input, __gm__ T *output);

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
ZBAL_KERNEL void ZBALAllGatherSmallKernel::Init(GM_ADDR input, GM_ADDR output, GM_ADDR metaGM, uint64_t elements,
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

    ZBALBaseKernel::Init(comm->dataOpType);
#endif
}

ZBAL_KERNEL void ZBALAllGatherSmallKernel::ExchangeInputAddrFlag()
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

        for (int dstRank = startRank; dstRank < endRank; dstRank++) {
            auto dataPtr = zbal_ptr(inputAddr, myGroupRank, dstRank, localDeviceMemSize, peerGroupRank2WorldRank);
            auto flagPtr = zbal_ptr(flagAddr, myGroupRank, dstRank, localDeviceMemSize, peerGroupRank2WorldRank);
            uint64_t dataAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(input));

            ZBALSetFlag(dataPtr, dataAddr, myGroupRank);
            ZBALSetFlag(flagPtr, waitSymbol, myGroupRank);
        }
    } else if (aivIndex < groupSize) {
        auto dataPtr = zbal_ptr(inputAddr, myGroupRank, aivIndex, localDeviceMemSize, peerGroupRank2WorldRank);
        auto flagPtr = zbal_ptr(flagAddr, myGroupRank, aivIndex, localDeviceMemSize, peerGroupRank2WorldRank);
        uint64_t dataAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(input));

        // write addr
        ZBALSetFlag(dataPtr, dataAddr, myGroupRank);
        // write exchangeFlag
        ZBALSetFlag(flagPtr, waitSymbol, myGroupRank);
    }
    ZBAL_PROF_STOP(comm, ZBAL_PROF_EXCHANGE_ADDR);
}

/**
    * @brief: write stat to corresponding remote rank stat buffer when finish data copying as a hint to remote rank
    */
ZBAL_KERNEL void ZBALAllGatherSmallKernel::WriteStat(int64_t targetDataRank)
{
    ZBAL_PROF_START(comm, ZBAL_PROF_WRITE_STAT);
    const int64_t aivIndex = (targetDataRank == -1) ? AscendC::GetBlockIdx() : 0;
    const int64_t corePerRank = AscendC::GetBlockNum() / groupSize;
    const int64_t offset = (targetDataRank == -1) ? aivIndex / corePerRank : targetDataRank;
    if (aivIndex % corePerRank == 0) {
        auto destStatPtr = zbal_ptr(statAddr, myGroupRank, offset, localDeviceMemSize, peerGroupRank2WorldRank);
        ZBALSetFlag(destStatPtr, waitSymbol, myGroupRank);
    }
    ZBAL_PROF_STOP(comm, ZBAL_PROF_WRITE_STAT);
}

template<typename T>
ZBAL_KERNEL void ZBALAllGatherSmallKernel::InnerProcessLargeGroupSize()
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

        ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_FLAG);
        ZBALWaitFlag(flagAddr, waitSymbol, offset);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_FLAG);

        ZBAL_PROF_START(comm, ZBAL_PROF_ALLGATHER_PREPARE_PTR);
        uint64_t inputOutAddr = ZBALGetFlag(inputAddr, offset);

        AscendC::GlobalTensor<T> outputGT;
        outputGT.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(output), elements * groupSize);
        AscendC::GlobalTensor<T> inputGT;
        inputGT.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(inputOutAddr), elements);
        AscendC::PipeBarrier<PIPE_ALL>();
        ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLGATHER_PREPARE_PTR);

        ZBAL_PROF_START(comm, ZBAL_PROF_ALLGATHER_COPY);
        CpGM2GM(inputGT[inputOffset], outputGT[outputOffset], elements);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLGATHER_COPY);

        WriteStat(offset);

        ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_STAT);
        ZBALWaitFlag(statAddr, waitSymbol, offset);
        ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_STAT);
    }
}

template<typename T>
ZBAL_KERNEL void ZBALAllGatherSmallKernel::InnerProcess()
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

    ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_FLAG);
    ZBALWaitFlag(flagAddr, waitSymbol, offset);
    ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_FLAG);

    ZBAL_PROF_START(comm, ZBAL_PROF_ALLGATHER_PREPARE_PTR);
    uint64_t inputDataAddr = ZBALGetFlag(inputAddr, offset);

    AscendC::GlobalTensor<T> outputGT;
    outputGT.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(output), elements * groupSize);
    AscendC::GlobalTensor<T> inputGT;
    inputGT.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(inputDataAddr), elements);
    AscendC::PipeBarrier<PIPE_ALL>();
    ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLGATHER_PREPARE_PTR);

    ZBAL_PROF_START(comm, ZBAL_PROF_ALLGATHER_COPY);
    CpGM2GM(inputGT[inputOffset], outputGT[outputOffset], numPerCore);
    ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLGATHER_COPY);

    WriteStat();

    ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_STAT);
    ZBALWaitFlag(statAddr, waitSymbol, offset);
    ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_STAT);
    AscendC::SyncAll<true>();
}

template<typename T>
ZBAL_KERNEL void ZBALAllGatherSmallKernel::Process()
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
ZBAL_KERNEL void ZBALAllGatherBigKernel::Init(GM_ADDR input, GM_ADDR output, GM_ADDR metaGM, uint64_t elements,
                                              uint64_t waitSymbol)
{
#ifdef __DAV_C220_VEC__
    this->aivNum = AscendC::GetBlockNum();
    this->coreNumPerRing = aivNum / ZBAL_AG_RING_NUM;
    this->statSizePerRank = coreNumPerRing * ZBAL_AG_SLICE_PER_CORE; // left right stat has same size
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

    ZBALBaseKernel::Init(comm->dataOpType);
#endif
}

ZBAL_KERNEL void ZBALAllGatherBigKernel::ExchangeOutputAddr(int64_t coreIndex, __gm__ uint64_t *statAddr,
                                                            int64_t statUpdateRank)
{
    // The stat area of the first cycle is set to ready in advance.
    ZBAL_PROF_START(comm, ZBAL_PROF_WRITE_STAT);
    for (int j = 0; j < ZBAL_AG_SLICE_PER_CORE; j++) {
        auto targetStatAddr = zbal_ptr(statAddr, myGroupRank, statUpdateRank, memSize, worldRanks);
        int64_t writeStatOffset = myGroupRank * this->statSizePerRank + coreIndex * ZBAL_AG_SLICE_PER_CORE + j;

        ZBALSetFlag(targetStatAddr, waitSymbol, writeStatOffset);
    }
    ZBAL_PROF_STOP(comm, ZBAL_PROF_WRITE_STAT);

    ZBAL_PROF_START(comm, ZBAL_PROF_ALLGATHER_PREPARE_PTR);
    if (coreIndex == 0) {
        auto dataPtr = zbal_ptr(outputAddr, myGroupRank, statUpdateRank, memSize, worldRanks);
        auto flagPtr = zbal_ptr(flagAddr, myGroupRank, statUpdateRank, memSize, worldRanks);
        uint64_t dataAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(output));

        ZBALSetFlag(dataPtr, dataAddr, myGroupRank);
        ZBALSetFlag(flagPtr, waitSymbol, myGroupRank);
    }

    ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLGATHER_PREPARE_PTR);
}

template<typename T>
ZBAL_KERNEL void ZBALAllGatherBigKernel::CopyLocal2Output(__gm__ T *input, __gm__ T *output)
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

    CpGM2GM(inputGT[inputOffset], outputGT[outputOffset], numPerCore);
    ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLGATHER_LOCAL_COPY);
}

ZBAL_KERNEL int64_t ZBALAllGatherBigKernel::GetTargetRank(int64_t prev, int64_t next, int64_t aivIndex, int loop)
{
    if (aivIndex < coreNumPerRing) {
        return (prev + groupSize - loop) % groupSize;
    } else {
        return (next + loop) % groupSize;
    }
}

ZBAL_KERNEL void ZBALAllGatherBigKernel::WriteStat(__gm__ uint64_t *statAddr, const int64_t targetStatRank,
                                                   const int64_t targetStatOffset)
{
    ZBAL_PROF_START(comm, ZBAL_PROF_WRITE_STAT);
    auto nextStat = zbal_ptr(statAddr, myGroupRank, targetStatRank, memSize, worldRanks);
    ZBALSetFlag(nextStat, waitSymbol, targetStatOffset);
    ZBAL_PROF_STOP(comm, ZBAL_PROF_WRITE_STAT);
}

template<typename T>
ZBAL_KERNEL void ZBALAllGatherBigKernel::Process() // ring allgather
{
#ifdef __DAV_C220_VEC__
    ZBAL_PROF_START(comm, ZBAL_PROF_ALLGATHER_KERNEL_ALL);
    const int64_t aivIndex = AscendC::GetBlockIdx();
    const int64_t prevRank = (myGroupRank + groupSize - 1) % groupSize;
    const int64_t nextRank = (myGroupRank + 1) % groupSize;
    const int64_t elemPerRing = elements / ZBAL_AG_RING_NUM;
    const int64_t curRingElem = aivIndex < coreNumPerRing ? elemPerRing : elements - elemPerRing;
    const int64_t coreIndex = aivIndex % coreNumPerRing;
    const int64_t elemExtraOffset = aivIndex < coreNumPerRing ? 0 : elemPerRing;
    const int64_t inputPtrIndex = aivIndex < coreNumPerRing ? prevRank : nextRank;
    const int64_t statUpdateRank = aivIndex < coreNumPerRing ? nextRank : prevRank;
    __gm__ uint64_t *statAddr = aivIndex < coreNumPerRing ? this->readLeftStatAddr : this->readRightStatAddr;

    AscendC::TBuf<AscendC::TPosition::VECIN> localBuf;
    pipe.InitBuffer(localBuf, UB_DMA_MAX_SIZE);
    AscendC::LocalTensor<uint64_t> localTensor = localBuf.Get<uint64_t>();
    ClearExchangeMeta(localTensor, outputAddr, exchangeMetaSize);
    BarrierAll(comm);

    CopyLocal2Output((__gm__ T *)input, (__gm__ T *)output); // copy self input to output buffer

    // notify the corresponding rank that the data is ready according to the forward or backward ring.
    ExchangeOutputAddr(coreIndex, statAddr, statUpdateRank);

    ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_FLAG);
    ZBALWaitFlag(flagAddr, waitSymbol, inputPtrIndex);
    ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_FLAG);

    uint64_t curInputAddr = ZBALGetFlag(outputAddr, inputPtrIndex);

    for (int i = 0; i < groupSize - 1; i++) {
        const int64_t readRank = GetTargetRank(prevRank, nextRank, aivIndex, i);
        uint32_t elemPerCore = curRingElem / coreNumPerRing;
        const uint32_t outputOffset = readRank * elements + coreIndex * elemPerCore + elemExtraOffset;
        if (coreIndex == coreNumPerRing - 1) {
            elemPerCore = curRingElem - (coreNumPerRing - 1) * elemPerCore;
        }

        for (int j = 0; j < ZBAL_AG_SLICE_PER_CORE; j++) {
            // current slice copy elements
            uint32_t elemPerSlice = elemPerCore / ZBAL_AG_SLICE_PER_CORE;
            const uint32_t sliceDataOffset = outputOffset + j * elemPerSlice; // read from prev/next same offset
            if (j == ZBAL_AG_SLICE_PER_CORE - 1) {
                elemPerSlice = elemPerCore - (ZBAL_AG_SLICE_PER_CORE - 1) * elemPerSlice;
            }

            int64_t sliceStatOffset = readRank * this->statSizePerRank + coreIndex * ZBAL_AG_SLICE_PER_CORE + j;

            ZBAL_PROF_START(comm, ZBAL_PROF_WAIT_STAT);
            ZBALWaitFlag(statAddr, waitSymbol, sliceStatOffset);
            ZBAL_PROF_STOP(comm, ZBAL_PROF_WAIT_STAT);

            ZBAL_PROF_START(comm, ZBAL_PROF_ALLGATHER_COPY);
            AscendC::GlobalTensor<T> outputGT;
            outputGT.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(output), elements * groupSize);
            AscendC::GlobalTensor<T> inputGT;
            inputGT.SetGlobalBuffer(reinterpret_cast<__gm__ T *>(curInputAddr), elements * groupSize);
            CpGM2GM(inputGT[sliceDataOffset], outputGT[sliceDataOffset], elemPerSlice);
            AscendC::PipeBarrier<PIPE_ALL>();
            ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLGATHER_COPY);

            WriteStat(statAddr, statUpdateRank, sliceStatOffset); // write stat to next rank when data ready
        }
    }
    BarrierAll(comm);
    ZBAL_PROF_STOP(comm, ZBAL_PROF_ALLGATHER_KERNEL_ALL);
#endif
}

#endif // ZBAL_KERNEL_ALLGATHER_H
