/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef ZBAL_SMA_COMMON_H
#define ZBAL_SMA_COMMON_H

#include <set>
#include <cstdint>
#include <memory>
#include <c10/core/Allocator.h>

#include <torch_npu/csrc/core/npu/NPUEvent.h>
#include <torch_npu/csrc/core/npu/NPUFunctions.h>
#include <torch_npu/csrc/core/npu/NPUStream.h>
#include <torch_npu/csrc/core/npu/sys_ctrl/npu_sys_ctrl.h>
#include <torch_npu/csrc/core/npu/NPUGraphsUtils.h>
#include <torch_npu/csrc/core/npu/NPUCachingAllocator.h>

#include "zbal_common_includes.h"

namespace zbal {
namespace sma {
/**
* @brief Type of block type
*/
enum DeviceBlockType {
    BT_SMALL,
    BT_BIG,
};

constexpr int32_t ALIGN_32 = 32;
constexpr int32_t ALIGN_64 = 64;

constexpr size_t kMinBlockSize = 512;        // all sizes are rounded to at least 512 bytes(for L1)
constexpr size_t kSmallSize = 1048576;       // largest "small" allocation is 1 MiB
constexpr size_t kSmallBuffer = 2097152;     // "small" allocations are packed in 2 MiB blocks
constexpr size_t kLargeBuffer = 20971520;    // "large" allocations may be packed in 20 MiB blocks
constexpr size_t kSmallAlloc = kSmallSize;   // allocations under 1 MiB may use kSmallBuffer
constexpr size_t kMiddleAlloc = 10485760;    // allocations between 1 and 10 MiB may use kLargeBuffer
                                             // allocations over 10MiB using rounded size with kRoundLarge
constexpr size_t kRoundLarge = 2097152;      // round up large allocs to 2 MiB
constexpr size_t kKB = 1024;                 // 1 KB
constexpr size_t kMB = 1024 * kKB;           // 1 MB
constexpr size_t kGB = 1024 * kMB;           // 1 GB
constexpr size_t kSmallHeapSize = 512 * kMB; // 512MB for small heap size in dualHeap allocator
constexpr size_t kSmallThreshold = 1 * kMB;  // 1MB to tell which is small in dualHeap/splitHeap allocator

constexpr int64_t kMaxTraceLen = -1; // max trace len per device for trace_record observer, use rolling update
// note that export segments cause lock action and heavily reduce the performance & take large memory to cache
// constexpr size_t kMaxSegmentsLen = 8;      // max segments len per device for segments observer, use rolling update

} // namespace sma
} // namespace zbal

#endif // ZBAL_SMA_COMMON_H
