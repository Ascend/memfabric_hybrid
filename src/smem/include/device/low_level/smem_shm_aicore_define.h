/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __MEMFABRIC_SMEM_AI_CORE_DEFINE_H__
#define __MEMFABRIC_SMEM_AI_CORE_DEFINE_H__

#include "kernel_operator.h"

constexpr int32_t BUFFER_NUM = 1;
constexpr uint32_t SMEM_ALIGN_SIZE = 32;

#define SMEM_INLINE_AICORE __attribute__((always_inline)) inline __aicore__

#ifndef SMEM_ALIGN_DOWN
#define SMEM_ALIGN_DOWN(val, al) ((val) & ~((al) - 1))
#endif
#ifndef SMEM_ALIGN_UP
#define SMEM_ALIGN_UP(val, al) (((val) + ((al) - 1)) & ~((al) - 1))
#endif

#define SMEM_TYPE_FUNC(fun)      \
    fun(int);               \
    fun(int8_t);         \
    fun(int16_t);       \
    fun(int64_t);       \
    fun(float);           \
    fun(float16_t);   \
    fun(bfloat16_t); \
    fun(half)

#endif // __MEMFABRIC_SMEM_AI_CORE_DEFINE_H__
