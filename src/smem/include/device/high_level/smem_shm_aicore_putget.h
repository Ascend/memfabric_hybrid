/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __MEMFABRIC_SMEM_AI_CORE_PUTGET_H__
#define __MEMFABRIC_SMEM_AI_CORE_PUTGET_H__

#include "smem_shm_aicore_copy.h"

#define SMEM_PUT_AICORE(inType)                                                         \
    SMEM_INLINE_AICORE void smem_put_##inType(__gm__ inType *gva,                       \
        __gm__ inType *src, uint32_t rank, uint32_t count)                              \
    {                                                                                   \
        uint64_t offset = smem_shm_get_symmetric_size();                                    \
        uint64_t dst = reinterpret_cast<uint64_t>(gva) + offset * rank;                 \
        smem_copy_gm2gm<inType>(reinterpret_cast<__gm__ inType*>(dst),                  \
            src, count * sizeof(inType));                                               \
    }

SMEM_TYPE_FUNC(SMEM_PUT_AICORE);

#define SMEM_GET_AICORE(inType)                                                         \
    SMEM_INLINE_AICORE void smem_get_##inType(__gm__ inType *gva,                       \
        __gm__ inType *dst, uint32_t rank, uint32_t count)                              \
    {                                                                                   \
        uint64_t offset = smem_shm_get_symmetric_size();                                    \
        uint64_t src = reinterpret_cast<uint64_t>(gva) + offset * rank;                 \
        smem_copy_gm2gm<inType>(dst, reinterpret_cast<__gm__ inType*>(src),             \
            count * sizeof(inType));                                                    \
    }

SMEM_TYPE_FUNC(SMEM_GET_AICORE);

#define SMEM_UPUT_AICORE(inType)                                                        \
    SMEM_INLINE_AICORE void smem_uput_##inType(__gm__ inType *gva,                      \
        __ubuf__ inType *src, uint32_t rank, uint32_t count)                            \
    {                                                                                   \
        uint64_t offset = smem_shm_get_symmetric_size();                                    \
        uint64_t dst = reinterpret_cast<uint64_t>(gva) + offset * rank;                 \
        smem_copy_ub2gm(reinterpret_cast<__gm__ inType*>(dst),                          \
            src, count * sizeof(inType));                                               \
    }

SMEM_TYPE_FUNC(SMEM_UPUT_AICORE);

#define SMEM_UGET_AICORE(inType)                                                        \
    SMEM_INLINE_AICORE void smem_uget_##inType(__gm__ inType *gva,                      \
        __ubuf__ inType *dst, uint32_t rank, uint32_t count)                            \
    {                                                                                   \
        uint64_t offset = smem_shm_get_symmetric_size();                                    \
        uint64_t src = reinterpret_cast<uint64_t>(gva) + offset * rank;                 \
        smem_copy_gm2ub(dst, reinterpret_cast<__gm__ inType*>(src),                     \
            count * sizeof(inType));                                                    \
    }

SMEM_TYPE_FUNC(SMEM_UGET_AICORE);

#endif // __MEMFABRIC_SMEM_AI_CORE_PUTGET_H__