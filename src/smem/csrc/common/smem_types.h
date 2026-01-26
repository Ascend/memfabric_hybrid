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

#ifndef MEMFABRIC_HYBRID_SMEM_TYPES_H
#define MEMFABRIC_HYBRID_SMEM_TYPES_H

#include <cstdint>

namespace ock {
namespace smem {
using Result = int32_t;

enum SMErrorCode : int32_t {
    SM_OK = 0,
    SM_ERROR = -1,
    SM_INVALID_PARAM = -2000,
    SM_MALLOC_FAILED = -2001,
    SM_NEW_OBJECT_FAILED = -2002,
    SM_NOT_STARTED = -2003,
    SM_TIMEOUT = -2004,
    SM_REPEAT_CALL = -2005,
    SM_DUPLICATED_OBJECT = -2006,
    SM_OBJECT_NOT_EXISTS = -2007,
    SM_NOT_INITIALIZED = -2008,
    SM_RESOURCE_IN_USE = -2009,
    SM_RECONNECT = -2010,
    SM_GET_OBJIECT = -2011,
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

constexpr uint32_t SMEM_DEFAUT_WAIT_TIME = 120; // 120s
constexpr uint32_t SMEM_ID_MAX = 63;
constexpr uint64_t SMEM_LOCAL_HBM_SIZE_MAX = 64ULL << 30; // 64G
constexpr uint64_t SMEM_LOCAL_DRAM_SIZE_MAX = 2ULL << 40; // 2T
} // namespace smem
} // namespace ock

#endif // MEMFABRIC_HYBRID_SMEM_TYPES_H
