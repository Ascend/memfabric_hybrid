/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This program is free software, you can redistribute it and/or modify it under
 * the terms and conditions of CANN Open Software License Agreement Version 2.0
 * (the "License"). Please refer to the License for details. You may not use
 * this file except in compliance with the License. THIS SOFTWARE IS PROVIDED ON
 * AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS
 * FOR A PARTICULAR PURPOSE. See LICENSE in the root of the software repository
 * for the full text of the License.
 */
#ifndef ZBAL_SMA_MM_HEAP_H
#define ZBAL_SMA_MM_HEAP_H

#include <pthread.h>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <iostream>

#include "zbal_common_includes.h"
#include "zbal_sma_common.h"
#include "zbal_sma_config.h"

constexpr int32_t ALIGN_32 = 32;
constexpr int32_t ALIGN_64 = 64;

namespace zbal {
namespace sma {
namespace heap {

class CustomMemoryHeap {
public:
    CustomMemoryHeap(void *base, uint64_t size) : base_(static_cast<uint8_t*>(base)), size_(size) {};

    virtual ~CustomMemoryHeap() noexcept = default;

public:
    virtual void *alignedAllocate(uint64_t alignment, uint64_t size) noexcept = 0;

    virtual size_t getTotalSize() noexcept = 0;

    virtual size_t getInUsedSize() noexcept = 0;

    virtual int32_t release(void *address) noexcept = 0;

    virtual bool allocatedSize(void *address, uint64_t &size) const noexcept = 0;

    inline bool isInitialized() const noexcept { return initialized_; }

    // to be deprecated
    inline uint8_t *getBaseAddr() const noexcept { return base_; }

protected:
    bool initialized_{false};

    uint8_t *const base_;
    const uint64_t size_;
};

// =========================================================
// SplitMemoryHeap Class Declaration
// Features: O(log N) Search, Bi-directional Allocation, POSIX Spinlock
// =========================================================
class SplitMemoryHeap : public CustomMemoryHeap {
public:
    SplitMemoryHeap(void *base, uint64_t size, uint64_t threshold = SMAConfig::small_heap_threshold());
    ~SplitMemoryHeap() override;

    // Core Interface Implementation
    void *alignedAllocate(uint64_t alignment, uint64_t size) noexcept override;
    int32_t release(void *address) noexcept override;
    bool allocatedSize(void *address, uint64_t &size) const noexcept override;
    size_t getTotalSize() noexcept override;
    size_t getInUsedSize() noexcept override;

private:
    // Configuration
    uint64_t split_threshold_;
    size_t used_bytes_{0};

    // Locking: POSIX Spinlock
    mutable pthread_spinlock_t spinlock_{};

    // --- RAII Helper for Spinlock ---
    // Defined inline for performance (compiler optimization)
    struct SpinGuard {
        pthread_spinlock_t& lock_ref;

        explicit SpinGuard(pthread_spinlock_t& lock) : lock_ref(lock)
        {
            pthread_spin_lock(&lock_ref);
        }

        ~SpinGuard()
        {
            pthread_spin_unlock(&lock_ref);
        }

        // Disable copy/move to prevent accidental unlocking
        SpinGuard(const SpinGuard&) = delete;
        SpinGuard& operator=(const SpinGuard&) = delete;
    };

    // --- Data Structures ---

    // 1. Global Address View: Key=Address, Value=Size
    // Used for O(log N) Coalescing (Merging)
    std::map<uintptr_t, size_t> global_free_map_;

    // 2. Segregated Size View: buckets[k] contains addresses of blocks with size in [2^k, 2^(k+1))
    // The internal std::set is sorted by Address.
    std::vector<std::set<uintptr_t>> size_buckets_;

    // 3. Metadata for allocated blocks
    std::unordered_map<void*, uint64_t> allocated_records_;

    // --- Internal Helper Methods ---

    static int get_bucket_index(uint64_t size);

    void addToFreeStructures(uintptr_t addr, uint64_t size);
    void removeFromFreeStructures(uintptr_t addr, uint64_t size);

    void* allocateLowToHigh(int start_idx, uint64_t alignment, uint64_t req_size);
    void* allocateHighToLow(int start_idx, uint64_t alignment, uint64_t req_size);

    void coalesceAndInsert(uintptr_t addr, uint64_t size);
};

}  // namespace heap

// Helper: Bit manipulation for bucket indexing
inline int get_bucket_index(uint64_t size)
{
    if (size == 0) {
        return 0;
    }
    // Uses GCC/Clang built-in for O(1) calculation.
    return ALIGN_64 - __builtin_clzll(size) - 1;
}

// Heap API remains for dma
ZBAL_API int CustomHeapAlignedAllocate(void **devPtr, size_t size,
                                       std::shared_ptr<heap::CustomMemoryHeap> symm_pool);

ZBAL_API int CustomHeapRelease(void *devPtr, std::shared_ptr<heap::CustomMemoryHeap> symm_pool);

ZBAL_API int CustomGetTotalSize(size_t &size, std::shared_ptr<heap::CustomMemoryHeap> symm_pool);

ZBAL_API int CustomInUsedSize(size_t &size, std::shared_ptr<heap::CustomMemoryHeap> symm_pool);

}  // namespace sma
}  // namespace zbal

#endif  // ZBAL_SMA_MM_HEAP_H
