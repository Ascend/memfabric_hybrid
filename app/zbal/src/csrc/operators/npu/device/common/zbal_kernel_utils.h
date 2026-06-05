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