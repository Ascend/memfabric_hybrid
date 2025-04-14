/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_SMEM_TYPES_H
#define MEMFABRIC_HYBRID_SMEM_TYPES_H

#include <cstdint>

namespace ock {
namespace smem {
using Result = int32_t;

enum SMErrorCode : int32_t {
    SM_OK = 0,
    SM_ERROR = -1,
    SM_INVALID_PARAM = -2,
    SM_MALLOC_FAILED = -3,
    SM_NEW_OBJECT_FAILED = -4,
    SM_NOT_STARTED = -5,
    SM_TIMEOUT = -6,
    SM_REPEAT_CALL = -7,
    SM_DUPLICATED_OBJECT = -8,
    SM_OBJECT_NOT_EXISTS = -9,
};

constexpr int32_t N16 = 16;
constexpr int32_t N64 = 64;
constexpr int32_t N256 = 256;
constexpr int32_t N1024 = 1024;
constexpr int32_t N8192 = 8192;

constexpr uint32_t UN2 = 2;
constexpr uint32_t UN32 = 32;
constexpr uint32_t UN58 = 58;
constexpr uint32_t UN128 = 128;
constexpr uint32_t UN65536 = 65536;
constexpr uint32_t UN16777216 = 16777216;
}  // namespace smem
}  // namespace ock

#endif  // MEMFABRIC_HYBRID_SMEM_TYPES_H
