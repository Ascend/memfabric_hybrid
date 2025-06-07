/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_MMC_TYPES_H
#define MEMFABRIC_HYBRID_MMC_TYPES_H

#include <cstdint>

namespace ock {
namespace mmc {
using Result = int32_t;

enum MmcErrorCode : int32_t {
    MMC_OK = 0,
    MMC_ERROR = -1,
    MMC_INVALID_PARAM = -3000,
    MMC_MALLOC_FAILED = -3001,
    MMC_NEW_OBJECT_FAILED = -3002,
    MMC_NOT_STARTED = -3003,
    MMC_TIMEOUT = -3004,
    MMC_REPEAT_CALL = -3005,
    MMC_DUPLICATED_OBJECT = -3006,
    MMC_OBJECT_NOT_EXISTS = -3007,
    MMC_NOT_INITIALIZED = -3008,
    MMC_NET_SEQ_DUP = -3009,
    MMC_NET_SEQ_NO_FOUND = -3010,
    MMC_ALREADY_NOTIFIED = -3011,
};

constexpr int32_t N16 = 16;
constexpr int32_t N64 = 64;
constexpr int32_t N256 = 256;
constexpr int32_t N1024 = 1024;
constexpr int32_t N8192 = 8192;

constexpr uint32_t UN2 = 2;
constexpr uint32_t UN16 = 16;
constexpr uint32_t UN32 = 32;
constexpr uint32_t UN58 = 58;
constexpr uint32_t UN128 = 128;
constexpr uint32_t UN65536 = 65536;
constexpr uint32_t UN16777216 = 16777216;

constexpr uint32_t MMC_DEFAUT_WAIT_TIME = 120; // 120s
} // namespace smem
} // namespace ock

#endif // MEMFABRIC_HYBRID_MMC_TYPES_H
