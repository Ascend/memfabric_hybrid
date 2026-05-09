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
#ifndef ZBAL_PYTORCH_MM_HEAP_H
#define ZBAL_PYTORCH_MM_HEAP_H

#include <pthread.h>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <iostream>

#include "zbal_common_includes.h"

namespace zbal {
namespace adaptor {
namespace heap {

struct MemoryRange {
    const uint64_t offset_;
    const uint64_t size_;

    MemoryRange(uint64_t o, uint64_t s) noexcept : offset_{o}, size_{s} {}
};

struct RangeSizeFirstComparator {
    bool operator()(const MemoryRange &mr1,
                    const MemoryRange &mr2) const noexcept;
};

class MemoryHeap {
public:
    MemoryHeap(void *base, uint64_t size) noexcept;

    ~MemoryHeap() noexcept;

public:
    void *allocate(uint64_t size) noexcept;

    void *alignedAllocate(uint64_t alignment, uint64_t size) noexcept;

    bool changeSize(void *address, uint64_t size) noexcept;

    size_t getTotalSize() noexcept;

    size_t getInUsedSize() noexcept;

    int32_t release(void *address) noexcept;

    bool allocatedSize(void *address, uint64_t &size) const noexcept;

private:
    static uint64_t allocated_size_align_up(uint64_t input_size) noexcept;

    static bool alignment_matches(const MemoryRange &mr, uint64_t alignment,
                                  uint64_t size, uint64_t &head_skip) noexcept;

    void reduce_size_in_lock(const std::map<uint64_t, uint64_t>::iterator &pos,
                             uint64_t new_size) noexcept;

    bool expend_size_in_lock(const std::map<uint64_t, uint64_t>::iterator &pos,
                             uint64_t new_size) noexcept;

private:
    bool initialized_{false};
    uint8_t *const base_;
    const uint64_t size_;
    uint64_t used_size_;
    mutable pthread_spinlock_t spinlock_{};
    std::map<uint64_t, uint64_t> address_idle_tree_;
    std::map<uint64_t, uint64_t> address_used_tree_;
    std::set<MemoryRange, RangeSizeFirstComparator> size_idle_tree_;
};

}  // namespace heap

// Heap API remains for dma
ZBAL_API int HeapAlignedAllocate(void **devPtr, size_t size,
                                 std::shared_ptr<heap::MemoryHeap> symm_pool);

ZBAL_API int HeapRelease(void *devPtr, std::shared_ptr<heap::MemoryHeap> symm_pool);

ZBAL_API int GetTotalSize(size_t &size, std::shared_ptr<heap::MemoryHeap> symm_pool);

ZBAL_API int GetInUsedSize(size_t &size, std::shared_ptr<heap::MemoryHeap> symm_pool);

}  // namespace adaptor
}  // namespace zbal

#endif  // ZBAL_PYTORCH_MM_HEAP_H
