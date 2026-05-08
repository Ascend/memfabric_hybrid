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
#include "zbal_defines.h"
#include "zbal_sma_mm_heap.h"

namespace zbal {
namespace sma {
namespace heap {

// SplitMemoryHeap
SplitMemoryHeap::SplitMemoryHeap(void *base, uint64_t size, uint64_t threshold)
    : CustomMemoryHeap(base, size), split_threshold_(threshold)
{
    // 1. Initialize the POSIX spinlock
    // PTHREAD_PROCESS_PRIVATE: Lock is only shared between threads of the same process
    pthread_spin_init(&spinlock_, PTHREAD_PROCESS_PRIVATE);

    // 2. Pre-allocate 64 buckets to cover size range 2^0 to 2^64
    size_buckets_.resize(ALIGN_64);

    // 3. Register the initial memory block if valid
    if (base != nullptr && size > 0) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(base);
        addToFreeStructures(addr, size);
        initialized_ = true;
    }
}

SplitMemoryHeap::~SplitMemoryHeap()
{
    // Destroy the POSIX spinlock upon destruction
    pthread_spin_destroy(&spinlock_);
}

void *SplitMemoryHeap::alignedAllocate(uint64_t alignment, uint64_t size) noexcept
{
    if (!initialized_ || size == 0) {
        return nullptr;
    }

    // RAII Lock Guard
    SpinGuard guard(spinlock_);

    if (alignment == 0) {
        alignment = 1;
    }

    int start_idx = get_bucket_index(size);

    if (size < split_threshold_) {
        // Strategy A: Low Address -> High Address (First-Fit)
        return allocateLowToHigh(start_idx, alignment, size);
    } else {
        // Strategy B: High Address -> Low Address (Best-Address-Fit)
        return allocateHighToLow(start_idx, alignment, size);
    }
}

int32_t SplitMemoryHeap::release(void *address) noexcept
{
    if (!address) {
        return -1;
    }

    SpinGuard guard(spinlock_);

    // 1. Verify allocation record
    auto it = allocated_records_.find(address);
    if (it == allocated_records_.end()) {
        return -1; // Error: Double free or invalid pointer
    }

    uint64_t size = it->second;
    allocated_records_.erase(it);
    used_bytes_ -= size;

    // 2. Return to free list and merge
    uintptr_t addr_val = reinterpret_cast<uintptr_t>(address);
    coalesceAndInsert(addr_val, size);

    return 0; // Success
}

bool SplitMemoryHeap::allocatedSize(void *address, uint64_t &size) const noexcept
{
    // Locking is required for thread-safety even in const methods
    SpinGuard guard(spinlock_);

    auto it = allocated_records_.find(address);
    if (it != allocated_records_.end()) {
        size = it->second;
        return true;
    }
    return false;
}

size_t SplitMemoryHeap::getInUsedSize() noexcept
{
    SpinGuard guard(spinlock_);
    return used_bytes_;
}

size_t SplitMemoryHeap::getTotalSize() noexcept
{
    SpinGuard guard(spinlock_);
    return size_;
}

int SplitMemoryHeap::get_bucket_index(uint64_t size)
{
    if (size == 0) {
        return 0;
    }
    // Uses GCC/Clang built-in for O(1) calculation.
    // This calculates floor(log2(size)).
    return ALIGN_64 - __builtin_clzll(size) - 1;
}

void SplitMemoryHeap::addToFreeStructures(uintptr_t addr, uint64_t size)
{
    if (size == 0) {
        return;
    }

    // Add to global address map
    global_free_map_[addr] = size;

    // Add to appropriate size bucket
    int idx = get_bucket_index(size);
    if (idx < (int)size_buckets_.size()) {
        size_buckets_[idx].insert(addr);
    }
}

void SplitMemoryHeap::removeFromFreeStructures(uintptr_t addr, uint64_t size)
{
    // Remove from global address map
    global_free_map_.erase(addr);

    // Remove from size bucket
    int idx = get_bucket_index(size);
    if (idx < (int)size_buckets_.size()) {
        size_buckets_[idx].erase(addr);
    }
}

void* SplitMemoryHeap::allocateLowToHigh(int start_idx, uint64_t alignment, uint64_t req_size)
{
    // Iterate through buckets starting from the smallest possible size
    for (int i = start_idx; i < (int)size_buckets_.size(); ++i) {
        auto& bucket = size_buckets_[i];
        if (bucket.empty()) continue;

        // Iterate: Low Address -> High Address
        for (auto it = bucket.begin(); it != bucket.end(); ) {
            uintptr_t addr = *it;
            auto next_it = std::next(it); // Safe iterator advancement

            size_t actual_size = global_free_map_[addr];

            // Calculate alignment
            uintptr_t aligned_addr = (addr + (alignment - 1)) & ~(alignment - 1);
            uint64_t padding = aligned_addr - addr;
            uint64_t total_needed = req_size + padding;

            if (actual_size >= total_needed) {
                // Block found
                removeFromFreeStructures(addr, actual_size);

                // 1. Handle Padding (Fragment at lower address)
                if (padding > 0) {
                    addToFreeStructures(addr, padding);
                }

                // 2. Handle Remaining (Fragment at higher address)
                uint64_t remaining = actual_size - total_needed;
                if (remaining > 0) {
                    addToFreeStructures(aligned_addr + req_size, remaining);
                }

                // 3. Record allocation
                void* ptr = reinterpret_cast<void*>(aligned_addr);
                allocated_records_[ptr] = req_size;
                used_bytes_ += req_size;
                return ptr;
            }

            it = next_it;
        }
    }
    return nullptr; // Out of memory
}

void* SplitMemoryHeap::allocateHighToLow(int start_idx, uint64_t alignment, uint64_t req_size)
{
    for (int i = start_idx; i < (int)size_buckets_.size(); ++i) {
        auto& bucket = size_buckets_[i];
        if (bucket.empty()) continue;

        // Reverse Iterate: High Address -> Low Address
        for (auto it = bucket.rbegin(); it != bucket.rend(); ) {
            uintptr_t addr = *it;
            auto next_it = std::next(it); // Advances towards lower addresses

            size_t actual_size = global_free_map_[addr];

            // Calculate split position from the END of the block
            uintptr_t block_end = addr + actual_size;
            uintptr_t candidate_start = block_end - req_size;
            // Align downwards
            uintptr_t aligned_start = candidate_start & ~(alignment - 1);

            // Check if validity
            if (aligned_start >= addr) {
                // Block found
                removeFromFreeStructures(addr, actual_size);

                // 1. Keep the lower part free
                uint64_t remain_low = aligned_start - addr;
                if (remain_low > 0) {
                    addToFreeStructures(addr, remain_low);
                }

                // 2. Upper padding is discarded in this implementation

                void* ptr = reinterpret_cast<void*>(aligned_start);
                allocated_records_[ptr] = req_size;
                used_bytes_ += req_size;
                return ptr;
            }
            it = next_it;
        }
    }
    return nullptr;
}

void SplitMemoryHeap::coalesceAndInsert(uintptr_t addr, uint64_t size)
{
    // 1. Try to merge with the Right (Next) block
    auto next_it = global_free_map_.lower_bound(addr + size);
    if (next_it != global_free_map_.end() && next_it->first == addr + size) {
        uintptr_t next_addr = next_it->first;
        uint64_t next_size = next_it->second;

        removeFromFreeStructures(next_addr, next_size);
        size += next_size;
    }

    // 2. Try to merge with the Left (Previous) block
    auto it = global_free_map_.lower_bound(addr);
    if (it != global_free_map_.begin()) {
        auto prev_it = std::prev(it);
        // Check adjacency
        if (prev_it->first + prev_it->second == addr) {
            uintptr_t prev_addr = prev_it->first;
            uint64_t prev_size = prev_it->second;

            removeFromFreeStructures(prev_addr, prev_size);

            // Update current address and size
            addr = prev_addr;
            size += prev_size;
        }
    }

    // 3. Insert final block
    addToFreeStructures(addr, size);
}

}  // namespace heap
}  // namespace sma
}  // namespace zbal

namespace zbal {
namespace sma {

ZBAL_API int CustomHeapAlignedAllocate(void **devPtr, size_t size,
                                       std::shared_ptr<heap::CustomMemoryHeap> symm_pool)
{
    if (!symm_pool) {
        return Z_ERROR;
    }
    *devPtr = symm_pool->alignedAllocate(ALIGN_32, size);
    if (*devPtr == nullptr) {
        return Z_ERROR_ALLOC;
    } else {
        return Z_OK;
    }
}

ZBAL_API int CustomHeapRelease(void *devPtr, std::shared_ptr<heap::CustomMemoryHeap> symm_pool)
{
    if (!symm_pool) {
        return Z_ERROR;
    }
    return symm_pool->release(devPtr);
}

ZBAL_API int CustomGetTotalSize(size_t &size, std::shared_ptr <heap::CustomMemoryHeap> symm_pool)
{
    if (!symm_pool) {
        return Z_ERROR;
    }
    size = symm_pool->getTotalSize();
    return Z_OK;
}

ZBAL_API int CustomGetInUsedSize(size_t &size, std::shared_ptr <heap::CustomMemoryHeap> symm_pool)
{
    if (!symm_pool) {
        return Z_ERROR;
    }
    size = symm_pool->getInUsedSize();
    return Z_OK;
}

}  // namespace sma
}  // namespace zbal