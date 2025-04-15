/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __MEMFABRIC_SMEM_AI_CORE_FLAG_H__
#define __MEMFABRIC_SMEM_AI_CORE_FLAG_H__

#include "smem_shm_aicore_copy.h"

SMEM_INLINE_AICORE void smem_set_flag(__ubuf__ int64_t *ubAddr,
__gm__ int64_t *gvaAddr, int64_t flagValue)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    *ubAddr = flagValue;
    AscendC::SetFlag<AscendC::HardEvent::S_MTE3>(EVENT_ID1);
    AscendC::WaitFlag<AscendC::HardEvent::S_MTE3>(EVENT_ID1);
    smem_copy_ub2gm(gvaAddr, ubAddr, sizeof(int64_t));
    AscendC::PipeBarrier<PIPE_ALL>();
}

SMEM_INLINE_AICORE void smem_wait_flag(__ubuf__ int64_t *ubAddr,
__gm__ int64_t *gvaAddr, int64_t expectValue)
{
    while (true) {
        AscendC::PipeBarrier<PIPE_ALL>();
        smem_copy_gm2ub(ubAddr, gvaAddr, sizeof(int64_t));
        AscendC::SetFlag<AscendC::HardEvent::MTE2_S>(EVENT_ID0);
        AscendC::WaitFlag<AscendC::HardEvent::MTE2_S>(EVENT_ID0);
        if (*ubAddr == expectValue) {
            break;
        }
    }
}

#endif // __MEMFABRIC_SMEM_AI_CORE_FLAG_H__
