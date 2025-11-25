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
