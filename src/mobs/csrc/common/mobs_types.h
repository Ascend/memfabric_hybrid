/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_MO_TYPES_H
#define MEMFABRIC_HYBRID_MO_TYPES_H

#include <cstdint>

namespace ock {
namespace mobs {
using Result = int32_t;

enum MO_ErrorCode : int32_t {
    MO_OK = 0,
    MO_ERROR = -1,
    MO_INVALID_PARAM = -3000,
    MO_MALLOC_FAILED = -3001,
    MO_NEW_OBJECT_FAILED = -3002,
    MO_NOT_STARTED = -3003,
    MO_TIMEOUT = -3004,
    MO_REPEAT_CALL = -3005,
    MO_DUPLICATED_OBJECT = -3006,
    MO_OBJECT_NOT_EXISTS = -3007,
    MO_NOT_INITIALIZED = -3008,
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

constexpr uint32_t MO_DEFAUT_WAIT_TIME = 120; // 120s
} // namespace smem
} // namespace ock

#endif // MEMFABRIC_HYBRID_MO_TYPES_H
