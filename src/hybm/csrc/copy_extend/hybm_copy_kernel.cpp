/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/
#include "acl/acl.h"
#include "kernel_operator.h"

#define HYBM_AICORE_KERNEL __attribute__((always_inline)) __aicore__ __inline__
const uint32_t COPY_BUF_SIZE = 64 * 1024; // 最大支持192KB
const uint32_t SINGLE_COPY_SLICE = 128;

HYBM_AICORE_KERNEL void copy_ub2gm(__gm__ uint8_t* dst, __ubuf__ uint8_t* src,
                                   uint32_t size, bool toL2Cache)
{
    AscendC::LocalTensor<uint8_t> ubTensor;
    AscendC::GlobalTensor<uint8_t> gmTensor;
    AscendC::DataCopyExtParams dataCopyParams(1, size, 0, 0, 0);
    ubTensor.address_.logicPos = static_cast<uint8_t>(AscendC::TPosition::VECIN);
    ubTensor.address_.bufferAddr = reinterpret_cast<uint64_t>(src);
    gmTensor.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t*>(dst));
    if (!toL2Cache) {
        gmTensor.SetL2CacheHint(AscendC::CacheMode::CACHE_MODE_DISABLE);
    }
    AscendC::DataCopyPad(gmTensor, ubTensor, dataCopyParams);
}

HYBM_AICORE_KERNEL void copy_gm2ub(__ubuf__ uint8_t* dst, __gm__ uint8_t* src,
                                   uint32_t size, bool toL2Cache)
{
    AscendC::LocalTensor<uint8_t> ubTensor;
    AscendC::GlobalTensor<uint8_t> gmTensor;
    AscendC::DataCopyExtParams dataCopyParams(1, size, 0, 0, 0);
    ubTensor.address_.logicPos = static_cast<uint8_t>(AscendC::TPosition::VECIN);
    ubTensor.address_.bufferAddr = reinterpret_cast<uint64_t>(dst);
    gmTensor.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t*>(src));
    if (!toL2Cache) {
        gmTensor.SetL2CacheHint(AscendC::CacheMode::CACHE_MODE_DISABLE);
    }
    AscendC::DataCopyPadExtParams<uint8_t> padParams;
    AscendC::DataCopyPad(ubTensor, gmTensor, dataCopyParams, padParams);
}

HYBM_AICORE_KERNEL void copy_gm2gm(__gm__ uint8_t *dst, __gm__ uint8_t *src, __ubuf__ uint8_t *buf,
                                   uint32_t ub_size, uint32_t elem_size)
{
    uint64_t repeat_times = elem_size / ub_size;
    uint64_t repeat_elem = ub_size;
    uint64_t remain = elem_size % ub_size;
    for (uint64_t i = 0; i < repeat_times; i++) {
        copy_gm2ub(buf, src + i * repeat_elem, ub_size, true);
        AscendC::SetFlag<AscendC::HardEvent::MTE2_MTE3>(EVENT_ID0);
        AscendC::WaitFlag<AscendC::HardEvent::MTE2_MTE3>(EVENT_ID0);
        copy_ub2gm(dst + i * repeat_elem, buf, ub_size, true);
        AscendC::SetFlag<AscendC::HardEvent::MTE3_MTE2>(EVENT_ID0);
        AscendC::WaitFlag<AscendC::HardEvent::MTE3_MTE2>(EVENT_ID0);
    }
    if (remain > 0) {
        copy_gm2ub(buf, src + repeat_times * repeat_elem, remain, true);
        AscendC::SetFlag<AscendC::HardEvent::MTE2_MTE3>(EVENT_ID0);
        AscendC::WaitFlag<AscendC::HardEvent::MTE2_MTE3>(EVENT_ID0);
        copy_ub2gm(dst + repeat_times * repeat_elem, buf, remain, true);
        AscendC::SetFlag<AscendC::HardEvent::MTE3_MTE2>(EVENT_ID0);
        AscendC::WaitFlag<AscendC::HardEvent::MTE3_MTE2>(EVENT_ID0);
    }
}

extern "C" __global__ __aicore__ void hybm_copy_kernel(GM_ADDR dst, GM_ADDR src, uint64_t len)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    uint32_t idx = AscendC::GetBlockIdx();
    uint32_t num = AscendC::GetBlockNum();
    uint64_t offset = ((len + SINGLE_COPY_SLICE - 1U) / SINGLE_COPY_SLICE + num - 1U) / num * SINGLE_COPY_SLICE;
    uint32_t size = min(offset * (idx + 1), len);
    offset = offset * idx;
    size -= offset;

    copy_gm2gm(dst + offset, src + offset, 0, COPY_BUF_SIZE, size);
}

extern "C" void hybm_copy_extend(void *src, void *dst, uint64_t len, uint32_t dim, void *stream)
{
    hybm_copy_kernel<<<dim, nullptr, stream>>>((uint8_t *)dst, (uint8_t *)src, len);
}

HYBM_AICORE_KERNEL void dcci_cacheline(__gm__ uint8_t *addr)
{
    AscendC::GlobalTensor<uint8_t> global;
    global.SetGlobalBuffer(addr);

    // Important: add hint to avoid dcci being optimized by compiler
    __asm__ __volatile__("");
    AscendC::DataCacheCleanAndInvalid<uint8_t, AscendC::CacheLine::SINGLE_CACHE_LINE,
                                      AscendC::DcciDst::CACHELINE_OUT>(global);
    __asm__ __volatile__("");
}

extern "C" __global__ __aicore__ void hybm_batch_copy_kernel(GM_ADDR param, uint32_t count, GM_ADDR mask)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    uint32_t idx = AscendC::GetBlockIdx();
    uint32_t num = AscendC::GetBlockNum();
    uint32_t offset = (count + num - 1) / num;
    uint32_t start = offset * idx;
    uint32_t end = min(start + offset, count);

    for (uint32_t i = start; i < end; i++) {
        GM_ADDR src = reinterpret_cast<GM_ADDR>(reinterpret_cast<__gm__ uint64_t *>(param)[i * 3]);
        GM_ADDR dst = reinterpret_cast<GM_ADDR>(reinterpret_cast<__gm__ uint64_t *>(param)[i * 3 + 1]);
        uint64_t len = reinterpret_cast<__gm__ uint64_t *>(param)[i * 3 + 2];
        copy_gm2gm(dst, src, 0, COPY_BUF_SIZE, len);
    }

    __gm__ uint64_t *ptr = reinterpret_cast<__gm__ uint64_t *>(mask);
    if (*ptr == idx) {
        *ptr = idx + 1;
        dcci_cacheline(mask);
    } // clear mask
}

extern "C" void hybm_batch_copy_extend(void *param, uint32_t count, void *mask, uint32_t dim, void *stream)
{
    hybm_batch_copy_kernel<<<dim, nullptr, stream>>>((uint8_t *)param, count, (uint8_t *)mask);
}