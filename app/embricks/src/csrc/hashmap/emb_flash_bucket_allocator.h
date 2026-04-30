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
 *
 * This is thread safe
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
    Result Allocate(uint32_t &offset) noexcept;

    /**
     * @brief Free one bucket with offset
     *
     * @param offset       [in] offset to start address, which allocated
     * @return 0 if successful
     */
    Result Free(uint32_t offset) noexcept;

    /**
     * @brief Get the capacity of allocator, i.e. how many buckets can be allocated
     *
     * @return capacity of this allocator
     */
    uint32_t Capacity() const noexcept;

    /**
     * @brief Get allocated buckets count
     *
     * @return allocated buckets count of this allocator
     */
    uint32_t Allocated() noexcept;

    /**
     * @brief operator <<
     */
    friend std::ostream &operator<<(std::ostream &os, const FlashBucketAllocator &o);

public:
    FlashBucketAllocator(const FlashBucketAllocator &) = delete;
    FlashBucketAllocator(FlashBucketAllocator &&) = delete;
    FlashBucketAllocator &operator=(const FlashBucketAllocator &) = delete;
    FlashBucketAllocator &operator=(FlashBucketAllocator &&) = delete;

private:
    Result VerifyOption() noexcept;

private:
    uintptr_t startAddress_ = 0;         /* start address of memory allocated from OS */
    uint64_t capacity_ = 0;              /* size of memory */
    std::mutex mutex_;                   /* mutex for member variables and operation  */
    bool inited_ = false;                /* initialized or not */
    uint32_t bitsetSearchStartPos_ = 0;  /* start position of search in bitset */
    FlashDynamicBitSet allocatedBitSet_; /* bitset to store how many and which 64bytes bucket are allocated */
};

EM_ALWAYS_INLINE FlashBucketAllocator::~FlashBucketAllocator()
{
    UnInitialize();
}

EM_ALWAYS_INLINE Result FlashBucketAllocator::VerifyOption() noexcept
{
    const auto tmpUnitSize = OVERFLOW_BUCKET_POOL_UNIT_SIZE_MB;
    const auto tmpCapMB = EnvHelper::gHashmapOverflowBucketAllocatorSizeMB;

    if (tmpCapMB < tmpUnitSize || tmpCapMB > UN1024) {
        EM_LOG_ERROR("Invalid memory size " << tmpCapMB << " (MB), which should be <= 1024 and >= " << tmpUnitSize);
        return EM_INVALID_PARAM;
    }

    if (tmpCapMB % tmpUnitSize != 0) {
        EM_LOG_ERROR("Invalid memory size " << tmpCapMB << " (MB), which should be multiple times of " << tmpUnitSize);
        return EM_INVALID_PARAM;
    }

    /*
     * translate to bytes, two steps: assign to an uint64_t and multiple to avoid overflow
     */
    capacity_ = tmpCapMB;
    capacity_ = capacity_ * UN1MB;

    return EM_OK;
}

EM_ALWAYS_INLINE Result FlashBucketAllocator::Initialize() noexcept
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

    auto bitsetSize = capacity_ / BUCKET_MEM_SIZE;

    /* allocate memory, include buckets memory and its bitset */
    auto bitSetMemSize = FlashDynamicBitSet::GetMemSize(bitsetSize);
    auto address = mmap(NULL, capacity_ + bitSetMemSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (address == nullptr) {
        EM_LOG_ERROR("Allocate memory [size: " << (capacity_ + bitSetMemSize) << "] failed, errno: " << errno);
        return EM_RESERVE_MEMORY_SPACE_FAILED;
    }

    /* initialize bitset */
    auto bitsetMemAddress = reinterpret_cast<uintptr_t>(address) + capacity_;
    result = allocatedBitSet_.Initialize(bitsetMemAddress, bitSetMemSize, bitsetSize, true);
    if (result != EM_OK) {
        EM_LOG_ERROR("Initialize bitset for allocation failed");
        munmap(address, capacity_ + bitSetMemSize);
        return result;
    }

    /* reset to zero */
    bzero(address, capacity_);

    startAddress_ = reinterpret_cast<uintptr_t>(address);
    inited_ = true;

    EM_LOG_DEBUG(*this);

    return EM_OK;
}

EM_ALWAYS_INLINE void FlashBucketAllocator::UnInitialize() noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!inited_) {
        return;
    }

    auto bitsetMemSize = FlashDynamicBitSet::GetMemSize(allocatedBitSet_.Capacity());
    auto result = munmap(reinterpret_cast<void *>(startAddress_), capacity_ + bitsetMemSize);
    if (result != 0) {
        EM_LOG_WARN("Call munmap to free memory failed, result: " << result << ", errno: " << errno);
    }

    startAddress_ = 0;
    allocatedBitSet_.UnInitialize();
    bitsetSearchStartPos_ = 0;
    capacity_ = 0;

    inited_ = false;

    EM_LOG_DEBUG("UnInitialized, " << *this);
}

EM_ALWAYS_INLINE uintptr_t FlashBucketAllocator::StartAddress() noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!inited_) {
        EM_LOG_DEBUG("Allocator is not initialized");
        return UN0;
    }

    return startAddress_;
}

EM_ALWAYS_INLINE Result FlashBucketAllocator::Allocate(uint32_t &offset) noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (UNLIKELY(!inited_)) {
        EM_LOG_DEBUG("Allocator is not initialized");
        return EM_NOT_INITIALIZED;
    }

    /* return if full */
    if (UNLIKELY(allocatedBitSet_.Full())) {
        EM_LOG_INFO("Allocate bucket failed as no more space left, details: " << *this);
        return EM_NO_MORE_SPACE;
    }

    /* reverse search start position */
    bitsetSearchStartPos_ = bitsetSearchStartPos_ % allocatedBitSet_.Capacity();

    /* find one */
    uint32_t allocatedBit = 0;
    if (UNLIKELY(allocatedBitSet_.FindAndSet(bitsetSearchStartPos_, allocatedBit))) {
        bitsetSearchStartPos_ = allocatedBit;
    }

    /* calculate the offset */
    offset = allocatedBit * BUCKET_MEM_SIZE;

    return EM_OK;
}

EM_ALWAYS_INLINE Result FlashBucketAllocator::Free(uint32_t offset) noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (UNLIKELY(!inited_)) {
        EM_LOG_DEBUG("Allocator is not initialized");
        return EM_NOT_INITIALIZED;
    }

    /* test bit */
    auto bitPos = offset / BUCKET_MEM_SIZE;
    if (UNLIKELY(!allocatedBitSet_.Test(bitPos))) {
        EM_LOG_INFO("Invalid offset, which has not been set to true or out of bound, position " << bitPos);
        return EM_INVALID_PARAM;
    }

    /* clear */
    allocatedBitSet_.Clear(bitPos);

    return EM_OK;
}

EM_ALWAYS_INLINE uint32_t FlashBucketAllocator::Capacity() const noexcept
{
    return capacity_ / BUCKET_MEM_SIZE;
}

EM_ALWAYS_INLINE uint32_t FlashBucketAllocator::Allocated() noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (UNLIKELY(!inited_)) {
        EM_LOG_DEBUG("Allocator is not initialized");
        return EM_NOT_INITIALIZED;
    }

    return allocatedBitSet_.Count();
}

EM_ALWAYS_INLINE std::ostream &operator<<(std::ostream &os, const FlashBucketAllocator &o)
{
    os << "FlashBucketAllocator initialized: " << o.inited_ << ", start address: " << std::hex
       << reinterpret_cast<void *>(o.startAddress_) << std::dec << ", memory size: " << o.capacity_
       << ", bitset start search position: " << o.bitsetSearchStartPos_
       << ", bit allocated: " << o.allocatedBitSet_.Count() << ", total bits: " << o.allocatedBitSet_.Capacity();
    return os;
}

} // namespace hashmap
} // namespace emb
} // namespace ock

#endif // MEMFABRIC_HYBRID_EMB_FLASH_BUCKET_ALLOCATOR_H
