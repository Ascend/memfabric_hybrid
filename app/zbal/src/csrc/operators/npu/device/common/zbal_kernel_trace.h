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
#ifndef ZBAL_KERNEL_TRACE_H
#define ZBAL_KERNEL_TRACE_H

#include <cstdint>
#include "zbal_kernel_utils.h"

namespace { // perf trace

ZBAL_KERNEL void ZBAL_PROF_RECORD(__gm__ CommGroupInfo *comm, uint64_t record)
{
    if (AscendC::GetBlockIdx() < ZBAL_MAX_AIV_SIZE_PER_NPU) {
        auto block = reinterpret_cast<__gm__ uint64_t *>(comm->devMemoryForProfiling);
        uint64_t coreOffset = AscendC::GetBlockIdx() * comm->tracePointPerCore;
        PipeBarrier<PIPE_ALL>();
        dcciCacheline(block + coreOffset + ZBAL_PROFILING_DEVICE_IDX_OFF);
        int64_t index = block[coreOffset + ZBAL_PROFILING_DEVICE_IDX_OFF] + 1; // get as record index
        if (index <= ZBAL_PROFILING_DEVICE_TRACE_OFF) {                        // first record start at [2]
            index = ZBAL_PROFILING_DEVICE_TRACE_OFF;
        }
        if (index >= comm->tracePointPerCore) {
            return;
        }
        PipeBarrier<PIPE_ALL>();
        block[coreOffset + ZBAL_PROFILING_DEVICE_IDX_OFF] = index;
        dcciCacheline(block + coreOffset + ZBAL_PROFILING_DEVICE_IDX_OFF); // flush index to gm at [1]

        block[coreOffset + index] = record;
        dcciCacheline(block + coreOffset + index); // flush record to gm at [index]
    }
}

ZBAL_KERNEL void ZBAL_PROF_START(__gm__ CommGroupInfo *comm, uint16_t frameId)
{
    if (comm->devMemoryForProfiling != 0) {
        PipeBarrier<PIPE_ALL>();
        int64_t cycles = AscendC::GetSystemCycle();
        uint64_t record = (((uint64_t)frameId << ZBAL_PROFILING_FRAME_SHIFT) | cycles);
        ZBAL_PROF_RECORD(comm, record);
    }
}

ZBAL_KERNEL void ZBAL_PROF_STOP(__gm__ CommGroupInfo *comm, uint16_t frameId)
{
    if (comm->devMemoryForProfiling != 0) {
        PipeBarrier<PIPE_ALL>();
        int64_t cycles = AscendC::GetSystemCycle();
        uint64_t record = (((uint64_t)frameId << ZBAL_PROFILING_FRAME_SHIFT) | cycles);
        record = (record | (1ULL << ZBAL_PROFILING_BE_SHIFT));
        ZBAL_PROF_RECORD(comm, record);
    }
}

} // namespace

namespace { // debug trace

ZBAL_KERNEL void ZBAL_DEBUG_RECORD(__gm__ CommGroupInfo *comm, uint64_t record)
{
    ZBAL_PROF_RECORD(comm, record);
}

ZBAL_KERNEL void ZBAL_DEBUG_ATTR(__gm__ CommGroupInfo *comm, uint64_t attr)
{
    if (attr != UINT64_MAX) {
        ZBAL_DEBUG_RECORD(comm, attr);
    }
}

ZBAL_KERNEL void ZBAL_DEBUG_START(__gm__ CommGroupInfo *comm, int lineNo)
{
    PipeBarrier<PIPE_ALL>();
    int64_t cycles = AscendC::GetSystemCycle();
    uint64_t record = (((uint64_t)ZBAL_PROF_LINENO << ZBAL_PROFILING_FRAME_SHIFT) | cycles);
    ZBAL_DEBUG_RECORD(comm, record);
    ZBAL_DEBUG_ATTR(comm, lineNo);
}

ZBAL_KERNEL void ZBAL_DEBUG_STOP(__gm__ CommGroupInfo *comm, int lineNo)
{
    PipeBarrier<PIPE_ALL>();
    int64_t cycles = AscendC::GetSystemCycle();
    uint64_t record = (((uint64_t)ZBAL_PROF_LINENO << ZBAL_PROFILING_FRAME_SHIFT) | cycles);
    record = (record | (1ULL << ZBAL_PROFILING_BE_SHIFT));
    ZBAL_DEBUG_RECORD(comm, record);
}

ZBAL_KERNEL void ZBAL_PROF_DUMP(__gm__ CommGroupInfo *comm, int lineNo, uint64_t v1, uint64_t v2, uint64_t v3,
                                uint64_t v4, uint64_t v5, uint64_t v6)
{
    if (comm->devMemoryForProfiling == 0) {
        return;
    }

    ZBAL_DEBUG_START(comm, lineNo);

    ZBAL_DEBUG_ATTR(comm, v1);
    ZBAL_DEBUG_ATTR(comm, v2);
    ZBAL_DEBUG_ATTR(comm, v3);
    ZBAL_DEBUG_ATTR(comm, v4);
    ZBAL_DEBUG_ATTR(comm, v5);
    ZBAL_DEBUG_ATTR(comm, v6);

    ZBAL_DEBUG_STOP(comm, lineNo);
    return;
}

ZBAL_KERNEL void ZBAL_PROF_DUMP(__gm__ CommGroupInfo *comm, int lineNo, uint64_t v1, uint64_t v2, uint64_t v3,
                                uint64_t v4, uint64_t v5)
{
    ZBAL_PROF_DUMP(comm, lineNo, v1, v2, v3, v4, v5, UINT64_MAX);
}

ZBAL_KERNEL void ZBAL_PROF_DUMP(__gm__ CommGroupInfo *comm, int lineNo, uint64_t v1, uint64_t v2, uint64_t v3,
                                uint64_t v4)
{
    ZBAL_PROF_DUMP(comm, lineNo, v1, v2, v3, v4, UINT64_MAX, UINT64_MAX);
}

ZBAL_KERNEL void ZBAL_PROF_DUMP(__gm__ CommGroupInfo *comm, int lineNo, uint64_t v1, uint64_t v2, uint64_t v3)
{
    ZBAL_PROF_DUMP(comm, lineNo, v1, v2, v3, UINT64_MAX, UINT64_MAX, UINT64_MAX);
}

ZBAL_KERNEL void ZBAL_PROF_DUMP(__gm__ CommGroupInfo *comm, int lineNo, uint64_t v1, uint64_t v2)
{
    ZBAL_PROF_DUMP(comm, lineNo, v1, v2, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX);
}

ZBAL_KERNEL void ZBAL_PROF_DUMP(__gm__ CommGroupInfo *comm, int lineNo, uint64_t v1)
{
    ZBAL_PROF_DUMP(comm, lineNo, v1, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX);
}

ZBAL_KERNEL void ZBAL_PROF_DUMP(__gm__ CommGroupInfo *comm, int lineNo)
{
    ZBAL_PROF_DUMP(comm, lineNo, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX);
}

} // namespace

#endif