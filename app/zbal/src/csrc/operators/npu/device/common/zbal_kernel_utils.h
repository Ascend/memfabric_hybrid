/*
Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
ZBAL is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
     http://license.coscl.org.cn/MulanPSL2

THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
*/
#ifndef ZBAL_KERNEL_UTILS_H
#define ZBAL_KERNEL_UTILS_H

#include <acl/acl_rt.h>
#include "zbal_def.h"
#include "zbal_defines.h"
#include "kernel_operator.h"
#include "zbal_comm_host_device_struct.h"

#define ZBAL_KERNEL             __attribute__((always_inline)) __aicore__ __inline__
#define ZBAL_CORE_BARRIER_SHIFT 2

constexpr int64_t UB_PAD_COUNT = 4;
constexpr int64_t UB_PAD4_COUNT = 8;
constexpr int64_t UB_ALIGN_SIZE = 32;
constexpr int64_t UB_BUFF_INTERVAL = 64;
constexpr int64_t UB_DMA_MAX_SIZE = 176 * 1024;
constexpr int64_t ZBAL_SMALL_DATA_SIZE = 256;
constexpr uint32_t ZBAL_SDMA_AFFECTION_BLOCK = 16;
constexpr uint32_t ZBAL_TYPE_SIZE_ONE = 1;
constexpr uint32_t ZBAL_TYPE_SIZE_TWO = 2;
constexpr uint32_t ZBAL_TYPE_SIZE_FOUR = 4;
constexpr uint32_t ZBAL_TYPE_SIZE_EIGHT = 8;

using namespace AscendC;
using namespace zbal;

inline uint32_t GetTypeSize(zbal_datatype_t type)
{
    switch (type) {
        case ZBAL_DATA_TYPE_INT8:
        case ZBAL_DATA_TYPE_UINT8:
            return ZBAL_TYPE_SIZE_ONE;
        case ZBAL_DATA_TYPE_INT16:
        case ZBAL_DATA_TYPE_FP16:
        case ZBAL_DATA_TYPE_UINT16:
        case ZBAL_DATA_TYPE_BFP16:
            return ZBAL_TYPE_SIZE_TWO;
        case ZBAL_DATA_TYPE_INT32:
        case ZBAL_DATA_TYPE_FP32:
        case ZBAL_DATA_TYPE_UINT32:
            return ZBAL_TYPE_SIZE_FOUR;
        case ZBAL_DATA_TYPE_INT64:
        case ZBAL_DATA_TYPE_UINT64:
        case ZBAL_DATA_TYPE_FP64:
            return ZBAL_TYPE_SIZE_EIGHT;
        default:
            return 0;
    }
}

template<AscendC::HardEvent event>
ZBAL_KERNEL void SyncFunc(int32_t eventID)
{
    AscendC::SetFlag<event>(eventID);
    AscendC::WaitFlag<event>(eventID);
}

template<typename T>
ZBAL_KERNEL void dcciCacheline(__gm__ T *addr)
{
    using namespace AscendC;
    GlobalTensor<T> global;
    global.SetGlobalBuffer(addr);

    // Important: add hint to avoid dcci being optimized by compiler
    __asm__ __volatile__("");
    DataCacheCleanAndInvalid<T, CacheLine::SINGLE_CACHE_LINE, DcciDst::CACHELINE_OUT>(global);
    __asm__ __volatile__("");
}

template<typename T>
ZBAL_KERNEL T CeilDiv(const T dividend, const T divisor)
{
    return (divisor == 0) ? 0 : ((dividend + divisor - 1) / divisor);
}

ZBAL_KERNEL __gm__ void *zbal_ptr(__gm__ void *ptr, int curPe, int dstPe, uint64_t localSize,
                                  __gm__ uint16_t *peerRanks)
{
    int worldDstPe = static_cast<int>(*((__gm__ uint16_t *)(peerRanks + dstPe)));
    int worldCurPe = static_cast<int>(*((__gm__ uint16_t *)(peerRanks + curPe)));
    uint64_t curPtr = reinterpret_cast<uint64_t>(ptr);
    uint64_t dstPtr = curPtr + (worldDstPe - worldCurPe) * localSize;
    return reinterpret_cast<__gm__ void *>(dstPtr);
}

ZBAL_KERNEL uint64_t ZBALGetFlag(__gm__ void *metaAddr, uint32_t rank)
{
    uint32_t flagOffset = rank * ZBAL_FLAG_SIZE;
    __gm__ uint64_t *flagAddr = (__gm__ uint64_t *)metaAddr + flagOffset;
    dcciCacheline((__gm__ uint8_t *)flagAddr);
    return *flagAddr;
}

ZBAL_KERNEL void ZBALSetFlag(__gm__ void *metaAddr, uint64_t val, uint32_t rank)
{
    uint32_t flagOffset = rank * ZBAL_FLAG_SIZE;
    __gm__ uint64_t *flagAddr = (__gm__ uint64_t *)metaAddr + flagOffset;
    *flagAddr = val;
    dcciCacheline((__gm__ uint8_t *)flagAddr);
}

ZBAL_KERNEL void ZBALWaitFlag(__gm__ void *metaAddr, uint64_t flagVal, uint32_t rank)
{
    while (true) {
        uint64_t flag = ZBALGetFlag(metaAddr, rank);
        if (flag == flagVal) {
            break;
        }
    }
}

ZBAL_KERNEL void BarrierAll(__gm__ CommGroupInfo *comm, bool flag = true, bool stat = true)
{
    AscendC::SyncAll<true>();
    const uint64_t barrierMagic = 1024;
    int64_t aivIndex = AscendC::GetBlockIdx();
    int64_t aivNum = AscendC::GetBlockNum();
    uint16_t startRank = comm->groupSize + 1;
    uint16_t endRank = comm->groupSize;
    __gm__ uint64_t *flagAddr = reinterpret_cast<__gm__ uint64_t *>(comm->myParamDataGva);
    __gm__ uint64_t *statAddr =
        reinterpret_cast<__gm__ uint64_t *>(comm->myParamDataGva + zbal::ZBAL_OPERATE_PARAM_SIZE / 2);
    if (comm->groupSize <= aivNum) {
        if (aivIndex < comm->groupSize) {
            startRank = aivIndex;
            endRank = startRank + 1;
        }
    } else {
        uint16_t avg = comm->groupSize / aivNum;
        uint16_t remain = comm->groupSize % aivNum;
        startRank = aivIndex * avg;
        if (aivIndex < remain) {
            avg += 1;
            startRank += aivIndex;
        } else {
            startRank += remain;
        }
        endRank = startRank + avg;
    }

    if (flag) {
        // flag
        for (uint16_t rank = startRank; rank < endRank; rank++) {
            AscendC::PipeBarrier<PIPE_ALL>();
            auto ptr =
                zbal_ptr(flagAddr, comm->myGroupRank, rank, comm->localDeviceMemSize, comm->peerGroupRank2WorldRank);
            ZBALSetFlag(ptr, barrierMagic, comm->myGroupRank);
        }
        for (uint16_t rank = startRank; rank < endRank; rank++) {
            AscendC::PipeBarrier<PIPE_ALL>();
            uint64_t readyFlag;
            do {
                readyFlag = ZBALGetFlag(flagAddr, rank);
            } while (readyFlag != barrierMagic);

            AscendC::PipeBarrier<PIPE_ALL>();
            ZBALSetFlag(flagAddr, 0, rank);
        }
    }

    if (stat) {
        // stat
        for (uint16_t rank = startRank; rank < endRank; rank++) {
            AscendC::PipeBarrier<PIPE_ALL>();
            auto ptr =
                zbal_ptr(statAddr, comm->myGroupRank, rank, comm->localDeviceMemSize, comm->peerGroupRank2WorldRank);
            ZBALSetFlag(ptr, barrierMagic, comm->myGroupRank);
        }
        for (uint16_t rank = startRank; rank < endRank; rank++) {
            AscendC::PipeBarrier<PIPE_ALL>();
            uint64_t readyFlag;
            do {
                readyFlag = ZBALGetFlag(statAddr, rank);
            } while (readyFlag != barrierMagic);

            AscendC::PipeBarrier<PIPE_ALL>();
            ZBALSetFlag(statAddr, 0, rank);
        }
    }

    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::SyncAll<true>();
}

ZBAL_KERNEL void ClearExchangeMeta(AscendC::LocalTensor<uint64_t> &localTensor, __gm__ uint64_t *exchangeMeta,
                                   uint32_t size)
{
    if (size == 0) {
        return;
    }

    uint32_t copyUbNum = UB_DMA_MAX_SIZE / sizeof(uint64_t);

    for (uint32_t offset = 0; offset < size; offset += copyUbNum) {
        uint32_t chunkSize = (offset + copyUbNum <= size) ? copyUbNum : (size - offset);

        AscendC::LocalTensor<uint32_t> localTensorI32 = localTensor.ReinterpretCast<uint32_t>();
        AscendC::Duplicate<uint32_t>(localTensorI32, 0, copyUbNum * ZBAL_TYPE_SIZE_TWO);
        AscendC::PipeBarrier<PIPE_V>();

        GlobalTensor<uint64_t> globalBuf;
        globalBuf.SetGlobalBuffer(exchangeMeta + offset, chunkSize);
        AscendC::DataCopyExtParams copyParams(1, chunkSize * sizeof(uint64_t), 0, 0, 0);

        AscendC::DataCopyPad(globalBuf, localTensor, copyParams);
        SyncFunc<AscendC::HardEvent::MTE3_MTE2>(EVENT_ID0);
    }
}

template<typename T>
ZBAL_KERNEL void CpGM2GM(AscendC::GlobalTensor<T> outputGT, AscendC::GlobalTensor<T> inputGT, uint64_t count)
{
    uint32_t copyUbSize = UB_DMA_MAX_SIZE;
    uint32_t copyUbNum = copyUbSize / sizeof(T);
    const uint64_t CpOffset = 1024;
    AscendC::LocalTensor<T> buf(AscendC::TPosition::VECIN, CpOffset + UB_ALIGN_SIZE, copyUbNum);
    uint64_t curOffset = 0;
    AscendC::DataCopyPadExtParams<T> copyExtParams;
    while (count > 0) {
        uint64_t curCount = count > copyUbNum ? copyUbNum : count;
        AscendC::DataCopyExtParams copyParams(1, curCount * sizeof(T), 0, 0, 0);

        AscendC::DataCopyPad(buf, inputGT[curOffset], copyParams, copyExtParams);
        SyncFunc<AscendC::HardEvent::MTE2_MTE3>(EVENT_ID0);
        AscendC::DataCopyPad(outputGT[curOffset], buf, copyParams);
        SyncFunc<AscendC::HardEvent::MTE3_MTE2>(EVENT_ID0);
        count -= curCount;
        curOffset += curCount;
    }
    return;
}

const std::vector<RankCoreMapping> allgatherRankCoreMapping = {
    {2, 2, 0, 256}, {2, 4, 256, 1024 * 1024},  {4, 4, 0, 256},   {4, 8, 256, 1024 * 1024},
    {8, 8, 0, 256}, {8, 16, 256, 1024 * 1024}, {16, 16, 0, 256}, {16, 32, 256, 1024 * 1024},
};

inline uint32_t ZBALOpGetAivBlockDim(CommGroupInfo &groupInfo, size_t sendCount, zbal_datatype_t dataType)
{
    static uint32_t physicalBlocks = 0;
    if (physicalBlocks == 0) {
        auto ret = aclrtGetResInCurrentThread(ACL_RT_DEV_RES_VECTOR_CORE, &physicalBlocks);
        if (ret != 0) {
            printf("ZBALOpAllGather get block dim failed, blockDim:%d\n", ret);
            return ret;
        }
    }

    uint32_t blockDim = physicalBlocks;
    uint64_t totalSize = GetTypeSize(dataType) * sendCount;
    if (totalSize <= ZBAL_SMALL_DATA_SIZE && blockDim > groupInfo.groupSize) {
        // avoid no task block for small shape
        blockDim = groupInfo.groupSize;
    } else {
        for (const auto &mapping : allgatherRankCoreMapping) {
            if (mapping.groupSize == groupInfo.groupSize && totalSize > mapping.start && totalSize <= mapping.end) {
                blockDim = mapping.blockDim;
                break;
            }
        }
    }

    if (groupInfo.dataOpType == ZBAL_DATA_OP_DEVICE_SDMA && blockDim > ZBAL_SDMA_AFFECTION_BLOCK) {
        blockDim = ZBAL_SDMA_AFFECTION_BLOCK;
    }

    return blockDim;
}

#endif // ZBAL_KERNEL_UTILS_H