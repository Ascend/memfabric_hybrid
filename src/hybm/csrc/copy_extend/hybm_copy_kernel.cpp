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

using namespace AscendC;

template <AscendC::HardEvent event>
HYBM_AICORE_KERNEL void hybm_sync(int32_t eventId)
{
    AscendC::SetFlag<event>(eventId);
    AscendC::WaitFlag<event>(eventId);
}

HYBM_AICORE_KERNEL void copy_ub2gm(__gm__ uint8_t* dst, __ubuf__ uint8_t* src, uint32_t size)
{
    AscendC::LocalTensor<uint8_t> ubTensor;
    AscendC::GlobalTensor<uint8_t> gmTensor;
    AscendC::DataCopyExtParams dataCopyParams(1, size, 0, 0, 0);
    ubTensor.address_.logicPos = static_cast<uint8_t>(AscendC::TPosition::VECIN);
    ubTensor.address_.bufferAddr = reinterpret_cast<uint64_t>(src);
    gmTensor.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t*>(dst));

    AscendC::DataCopyPad(gmTensor, ubTensor, dataCopyParams);
}

HYBM_AICORE_KERNEL void copy_gm2ub(__ubuf__ uint8_t* dst, __gm__ uint8_t* src, uint32_t size)
{
    AscendC::LocalTensor<uint8_t> ubTensor;
    AscendC::GlobalTensor<uint8_t> gmTensor;
    AscendC::DataCopyExtParams dataCopyParams(1, size, 0, 0, 0);
    ubTensor.address_.logicPos = static_cast<uint8_t>(AscendC::TPosition::VECIN);
    ubTensor.address_.bufferAddr = reinterpret_cast<uint64_t>(dst);
    gmTensor.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t*>(src));

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
        copy_gm2ub(buf, src + i * repeat_elem, ub_size);
        hybm_sync<AscendC::HardEvent::MTE2_MTE3>(EVENT_ID0);
        copy_ub2gm(dst + i * repeat_elem, buf, ub_size);
        hybm_sync<AscendC::HardEvent::MTE3_MTE2>(EVENT_ID0);
    }
    if (remain > 0) {
        copy_gm2ub(buf, src + repeat_times * repeat_elem, remain);
        hybm_sync<AscendC::HardEvent::MTE2_MTE3>(EVENT_ID0);
        copy_ub2gm(dst + repeat_times * repeat_elem, buf, remain);
        hybm_sync<AscendC::HardEvent::MTE3_MTE2>(EVENT_ID0);
    }
}

extern "C" __global__ __aicore__ void hybm_copy_kernel(GM_ADDR dst, GM_ADDR src, uint64_t len)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    uint32_t idx = AscendC::GetBlockIdx();
    uint32_t num = AscendC::GetBlockNum();
    uint64_t offset = ((len + SINGLE_COPY_SLICE - 1U) / SINGLE_COPY_SLICE + num - 1U) / num * SINGLE_COPY_SLICE;
    uint64_t size = min(offset * (idx + 1), len);
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

HYBM_AICORE_KERNEL void update_mask(GM_ADDR mask, uint32_t idx)
{
    __gm__ uint64_t *ptr = reinterpret_cast<__gm__ uint64_t *>(mask);
    do {
        dcci_cacheline(mask);
    } while (*ptr != idx);
    *ptr = idx + 1;
    dcci_cacheline(mask);
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

    update_mask(mask, idx);
}

extern "C" void hybm_batch_copy_extend(void *param, uint32_t count, void *mask, uint32_t dim, void *stream)
{
    hybm_batch_copy_kernel<<<dim, nullptr, stream>>>((uint8_t *)param, count, (uint8_t *)mask);
}

HYBM_AICORE_KERNEL void hybm_reduce_max(__ubuf__ float *buf, uint32_t count)
{
    using PrimType = PrimT<float>;
    uint64_t repsFp32 = count >> 6;        // 6 is count / elemPerRefFp32
    uint64_t offsetsFp32 = repsFp32 << 6;  // 6 is repsFp32 * elemPerRefFp32
    uint64_t remsFp32 = count & 0x3f;      // 0x3f 63, count % elemPerRefFp32
    const uint64_t elemPerRefFp32 = 64UL;  // 256 bit / sizeof(float)
    if (likely(repsFp32 > 1)) {
        // 8 is rep stride
        MaxImpl<PrimType, true>((__ubuf__ PrimType*)buf, (__ubuf__ PrimType*)(buf + elemPerRefFp32),
                                (__ubuf__ PrimType*)buf, elemPerRefFp32, repsFp32 - 1, {1, 1, 1, 0, 8, 0});
        PipeBarrier<PIPE_V>();
    }
    if (unlikely(remsFp32 > 0) && unlikely(offsetsFp32 > 0)) {
        MaxImpl<PrimType, true>((__ubuf__ PrimType*)buf, (__ubuf__ PrimType*)(buf + offsetsFp32),
                                (__ubuf__ PrimType*)buf, remsFp32, 1, {1, 1, 1, 0, 8, 0});
        PipeBarrier<PIPE_V>();
    }
    uint32_t mask = (repsFp32 > 0) ? elemPerRefFp32 : count;
    // 8 is rep stride
    WholeReduceMaxImpl<float, true>(buf, buf, mask, 1, 8, 1, 8, ReduceOrder::ORDER_VALUE_INDEX);
}

// bf16/half-->int8
template <typename T>
HYBM_AICORE_KERNEL void hybm_quant_process(GM_ADDR dst, GM_ADDR src, __ubuf__ uint8_t *buf, uint32_t num,
                                           __gm__ float *scale, __gm__ float *offset)
{
    uint32_t len = num * sizeof(float);
    __ubuf__ uint8_t *buf2 = buf + len;
    float dynamicScale = 0.0;

    copy_gm2ub(buf2, src, num * 2);
    PipeBarrier<PIPE_ALL>();
    CastImpl((__ubuf__ float *)buf, (__ubuf__ T *)buf2, RoundMode::CAST_NONE, num); // bf16/half-->float
    PipeBarrier<PIPE_V>();

    AbsImpl((__ubuf__ float *)buf2, (__ubuf__ float *)buf, num);
    PipeBarrier<PIPE_V>();
    hybm_reduce_max((__ubuf__ float *)buf2, num);
    hybm_sync<AscendC::HardEvent::V_S>(EVENT_ID0);
    dynamicScale = float(127.0) / (((__ubuf__ float *)buf2)[0] + 1e-12f);
    hybm_sync<AscendC::HardEvent::S_V>(EVENT_ID0);
    MulsImpl<float, true>((__ubuf__ float*)buf, (__ubuf__ float*)buf, dynamicScale, num);
    PipeBarrier<PIPE_V>();

    CastImpl((__ubuf__ int32_t *)buf2, (__ubuf__ float *)buf, RoundMode::CAST_RINT, num); // float-->int32
    PipeBarrier<PIPE_V>();
    SetDeqScale((half)1.000000e+00f);
    PipeBarrier<PIPE_V>();
    CastImpl((__ubuf__ half *)buf, (__ubuf__ int32_t *)buf2, RoundMode::CAST_ROUND, num); // int32-->half
    PipeBarrier<PIPE_V>();
    CastImpl((__ubuf__ int8_t *)buf2, (__ubuf__ half *)buf, RoundMode::CAST_TRUNC, num); // half-->int8
    PipeBarrier<PIPE_ALL>();

    copy_ub2gm(dst, buf2, num);
    PipeBarrier<PIPE_ALL>();

    if (likely((uint64_t)scale > 0x100000000ULL)) { // maybe scale is nullptr
        *scale = float(1.0) / dynamicScale;
        dcci_cacheline((__gm__ uint8_t *)scale);
    }
}

extern "C" __global__ __aicore__ void hybm_batch_copy_quant_kernel(GM_ADDR param, uint32_t count, uint32_t unit,
                                                                   uint32_t flags, GM_ADDR mask)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    uint32_t idx = AscendC::GetBlockIdx();
    uint32_t num = AscendC::GetBlockNum();
    uint32_t offset = (count + num - 1) / num;
    uint32_t start = offset * idx;
    uint32_t end = min(start + offset, count);
    uint32_t single = unit * 2;

    for (uint32_t i = start; i < end; i++) {
        GM_ADDR src = reinterpret_cast<GM_ADDR>(reinterpret_cast<__gm__ uint64_t *>(param)[i * 5]);
        GM_ADDR dst = reinterpret_cast<GM_ADDR>(reinterpret_cast<__gm__ uint64_t *>(param)[i * 5 + 1]);
        uint64_t len = reinterpret_cast<__gm__ uint64_t *>(param)[i * 5 + 2];
        __gm__ float *scale = reinterpret_cast<__gm__ float *>(reinterpret_cast<__gm__ uint64_t *>(param)[i * 5 + 3]);
        __gm__ float *offset = reinterpret_cast<__gm__ float *>(reinterpret_cast<__gm__ uint64_t *>(param)[i * 5 + 4]);
        for (uint32_t j = 0, t = 0; j < static_cast<uint32_t>(len); j += single, t += 1) {
            if (flags == 0) {
                hybm_quant_process<bfloat16_t>(dst + j / 2, src + j, 0, unit, scale + t, offset + t);
            } else {
                hybm_quant_process<half>(dst + j / 2, src + j, 0, unit, scale + t, offset + t);
            }
        }
    }

    update_mask(mask, idx);
}

extern "C" void hybm_batch_copy_quant(void *param, uint32_t count, uint32_t unit, uint32_t flags, void *mask,
                                      uint32_t dim, void *stream)
{
    hybm_batch_copy_quant_kernel<<<dim, nullptr, stream>>>((uint8_t *)param, count, unit, flags, (uint8_t *)mask);
}