/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Embricks is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MEMFABRIC_HYBRID_EMB_FLASH_BUCKET_ALLOCATOR_H
#define MEMFABRIC_HYBRID_EMB_FLASH_BUCKET_ALLOCATOR_H

#include "emb_flash_hashmap_types.h"
#include "emb_flash_dynamic_bitset.h"
#include "emb_env_helper.h"

namespace ock {
namespace emb {
namespace hashmap {
/**
 * The allocator is designed for overflowed bucket for hashmap,
 * by default, one allocator allocated from 16MB from OS, it is not expandable,
 * 16MB contains 262144 buckets, if larger overflow buckets exist, what should do are:
 * 1) increase the capacity of hashmap itself first, this the first choice
 * 2) increase the capacity of this by set env 'EMB_HASHMAP_OVERFLOW_BUCKET_ALLOCATOR_SIZE_MB'
 *
 * After this allocator is initialized, physical memory should be allocated from OS,
 * so we can get bucket from this allocator in very short time
 */
class FlashBucketAllocator {
public:
    FlashBucketAllocator() = default;
    ~FlashBucketAllocator();

    /**
     * @brief Do initialization include reserve space, set member variables, and allocate physical memory
     * @return 0 if successful
     */
    Result Initialize() noexcept;

    /**
     * @brief Do un-initialization
     */
    void UnInitialize() noexcept;

    /**
     * @brief Get start address of memory allocated from os
     *
     * @return
     */
    uintptr_t StartAddress() noexcept;

    /**
     * @brief Allocate one bucket
     *
     * @param offset       [out] offset to start address, which allocated
     * @return 0 if successful
     */
    Result Allocate(uint64_t &offset) noexcept;

    /**
     * @brief Free one bucket with offset
     *
     * @param offset       [in] offset to start address, which allocated
     * @return 0 if successful
     */
    Result Free(uint64_t offset) noexcept;

public:
    FlashBucketAllocator(const FlashBucketAllocator &) = delete;
    FlashBucketAllocator(FlashBucketAllocator &&) = delete;
    FlashBucketAllocator &operator=(const FlashBucketAllocator &) = delete;
    FlashBucketAllocator &operator=(FlashBucketAllocator &&) = delete;

private:
    Result VerifyOption() noexcept;

private:
    uintptr_t startAddress_ = 0;         /* start address of memory allocated from OS */
    uint64_t capacity_ = 0;              /* size of reserved memory space */
    std::mutex mutex_;                   /* mutex for member variables */
    bool inited_ = false;                /* initialized or not */
    uint32_t bitsetSearchStartPos_ = 0;  /* start position of search in bitset */
    FlashDynamicBitSet allocatedBitSet_; /* bitset to store how many and which 64bytes block are allocated */
};

inline FlashBucketAllocator::~FlashBucketAllocator()
{
    UnInitialize();
}

inline Result FlashBucketAllocator::VerifyOption() noexcept
{
    const auto tmpUnitSize = OVERFLOW_BUCKET_POOL_UNIT_SIZE_MB;
    const auto tmpCapacityMB = EnvHelper::gHashmapOverflowBucketAllocatorSizeMB;

    if (tmpCapacityMB < tmpUnitSize || tmpCapacityMB > UN1024) {
        EM_LOG_ERROR("Invalid size for overflowed bucket allocator, which should be <= 1024 and >= " << tmpUnitSize);
        return EM_INVALID_PARAM;
    }

    if (tmpCapacityMB % tmpUnitSize == 0) {
        EM_LOG_ERROR("Invalid size for overflowed bucket allocator, which should be multiple times of " << tmpUnitSize);
        return EM_INVALID_PARAM;
    }

    /*
     * translate to bytes, two steps: assign to an uint64_t and multiple to avoid overflow
     */
    capacity_ = tmpCapacityMB;
    capacity_ = capacity_ * UN1MB;

    return EM_OK;
}

inline Result FlashBucketAllocator::Initialize() noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (inited_) {
        EM_LOG_DEBUG("Already initialized");
        return EM_OK;
    }

    /* verify and assign start address and capacity */
    auto result = VerifyOption();
    if (result != EM_OK) {
        return result;
    }

    auto bitsetSize = capacity_ / OVERFLOW_BUCKET_POOL_UNIT_SIZE_MB;
    result = allocatedBitSet_.Initialize(bitsetSize);
    if (result != EM_OK) {
        EM_LOG_ERROR("Initialize bitset for allocation failed");
        return result;
    }

    /*
     * reserve memory space
     * here we only reserve memory space, no physical memory allocated
     */
    auto address = mmap(NULL, capacity_, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (address == nullptr) {
        EM_LOG_ERROR("Allocate memory space [size: " << capacity_ << "] failed, errno: " << errno);
        return EM_RESERVE_MEMORY_SPACE_FAILED;
    }

    /* reset to zero */
    bzero(address, capacity_);

    startAddress_ = reinterpret_cast<uintptr_t>(address);
    inited_ = true;

    EM_LOG_DEBUG("Initialized: " << std::hex << reinterpret_cast<void *>(startAddress_) << std::dec
                                 << ", capacity: " << capacity_ << " bytes, remaining: " << allocatedBitSet_.Capacity()
                                 << ", allocated: " << allocatedBitSet_.Count());

    return EM_OK;
}

inline void FlashBucketAllocator::UnInitialize() noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!inited_) {
        return;
    }

    auto result = munmap(reinterpret_cast<void *>(startAddress_), capacity_);
    if (result != 0) {
        EM_LOG_WARN("Call munmap failed, result: " << result << ", errno: " << errno);
    }

    inited_ = false;
}
} // namespace hashmap
} // namespace emb
} // namespace ock

#endif // MEMFABRIC_HYBRID_EMB_FLASH_BUCKET_ALLOCATOR_H
