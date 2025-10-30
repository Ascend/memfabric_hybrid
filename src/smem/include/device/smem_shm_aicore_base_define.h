/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef __MEMFABRIC_SMEM_AI_CORE_BASE_DEFINE_H__
#define __MEMFABRIC_SMEM_AI_CORE_BASE_DEFINE_H__

#include "kernel_operator.h"

constexpr int32_t BUFFER_NUM = 1;
constexpr uint32_t SMEM_SHM_ALIGN_SIZE = 32;

#define SMEM_SHM_INLINE_AICORE __attribute__((always_inline)) inline __aicore__

#ifndef SMEM_SHM_ALIGN_DOWN
#define SMEM_SHM_ALIGN_DOWN(val, al) ((val) & ~((al) - 1))
#endif
#ifndef SMEM_SHM_ALIGN_UP
#define SMEM_SHM_ALIGN_UP(val, al) (((val) + ((al) - 1)) & ~((al) - 1))
#endif

#define SMEM_SHM_TYPE_FUNC(fun)     \
    fun(int);                       \
    fun(int8_t);                    \
    fun(int16_t);                   \
    fun(int64_t);                   \
    fun(float);                     \
    fun(float16_t);                 \
    fun(bfloat16_t);                \
    fun(half)

#endif // __MEMFABRIC_SMEM_AI_CORE_BASE_DEFINE_H__
