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

#include <bitset>

#include "emb_flash_hashmap_types.h"

namespace ock {
namespace emb {
namespace hashmap {
/**
 * Mem slice which 64 bytes
 */
struct BucketMemSlice {
    BucketMemSlice *next = nullptr;
};

/**
 * Allocate one block from system, size of one block is 2MB, which equal to one 2MB huge page,
 * this block will be sliced to several slices, size of each slice is 64 bytes,
 * the slices are linked one by one, and using the 64 bytes for linked ptr,
 * when we allocate one slice just take one from the head,
 * when we free one slice just insert to the head
 */
struct BucketMemBlock {
    uintptr_t addressStart = 0;
    uintptr_t addressEnd = 0;
    BucketMemSlice *headSlice = nullptr;
    BucketMemBlock *nextBlock = nullptr;
    bool slicesCreated = false;

    /**
     * @brief Check if a ptr belongs to this block
     *
     * @param ptr          [in] the pointer that to be checked
     *
     * @return true if belong to this block
     */
    bool InRange(uintptr_t ptr) const noexcept;

    /**
     * @brief Allocate one mem slice from the block
     *
     * @param ptr          [out] ptr that allocated
     * @return true if successful
     */
    bool Allocate(uintptr_t &ptr) noexcept;

    /**
     * @brief Free the mem slice back to block
     *
     * @param ptr          [in] ptr of the slice to be freed
     * @return true if freed
     */
    bool Free(uintptr_t ptr) noexcept;

    /**
     * @brief Split the block into slices
     *
     * @return true if successful
     */
    bool MakeSlices() noexcept;
};

inline bool BucketMemBlock::InRange(uintptr_t ptr) const noexcept
{
    if (UNLIKELY(ptr == 0)) {
        return false;
    }

    return (addressStart <= ptr && ptr < addressEnd);
}

inline bool BucketMemBlock::Allocate(uintptr_t &ptr) noexcept
{
    if (headSlice != nullptr) {
        ptr = reinterpret_cast<uintptr_t>(headSlice);
        headSlice = headSlice->next;
        return true;
    }

    return false;
}

inline bool BucketMemBlock::Free(uintptr_t ptr) noexcept
{
    if (!InRange(ptr)) {
        return false;
    }

    auto originalNext = headSlice;
    auto newNext = reinterpret_cast<BucketMemSlice *>(ptr);
    newNext->next = originalNext;
    headSlice = newNext;
    return true;
}

inline bool BucketMemBlock::MakeSlices() noexcept
{
    if (slicesCreated) {
        return true;
    }

    if (addressStart == 0 || addressEnd == 0) {
        EM_LOG_ERROR("Invalid address start or end is 0");
        return false;
    }

    if ((addressEnd - addressStart) == kMemBlockSize) {
        EM_LOG_ERROR("Invalid address that (addressEnd - addressStart) = "
                     << (addressEnd - addressStart) << ", which is not equal to " << kMemBlockSize);
        return false;
    }

    bzero(reinterpret_cast<void *>(addressStart), kMemBlockSize);

    for (uint32_t i = 0; i < kBucketsPerMemBlock - 1; i++) {
        auto tmpSlice = reinterpret_cast<BucketMemSlice *>(addressStart + kBucketSize * i);
        tmpSlice->next = reinterpret_cast<BucketMemSlice *>(addressStart + kBucketSize * (i + 1));
    }

    headSlice = reinterpret_cast<BucketMemSlice *>(addressStart);

    slicesCreated = true;
}

/**
 * Mem pool that response for reserving configured memory space and allocate 2MB
 * 1 allocate from start to end
 * 2 no free provided
 * 3 no physical memory will be attached in this class
 */
class FlashBucketMemPool {
public:
    static FlashBucketMemPool &Instance() noexcept;

public:
    ~FlashBucketMemPool();

    /**
     * @brief Do initialization include reserve space and set member variables
     * @return 0 if successful
     */
    Result Initialize() noexcept;

    /**
     * @brief Do un-initialization
     */
    void UnInitialize() noexcept;

    /**
     * @brief Allocate 2MB space from start to end, since no physical page is not allocated,
     * so this operation should be quite fast
     *
     * @param address      [out] allocated space
     *
     * @return 0 if successful
     */
    Result Allocate2MB(uintptr_t &address) noexcept;

    /**
     * @brief Get start address of mem pool
     *
     * @return start address
     */
    uintptr_t StartAddress() const noexcept;

    /**
     * @brief operator << for ostream
     */
    friend std::ostream &operator<<(std::ostream &os, const FlashBucketMemPool &obj)
    {
        os << "FlashBucketMemPool [startAddress: " << std::hex << reinterpret_cast<void *>(obj.startAddress_)
           << std::dec << ", capacity: " << obj.capacity_ << " bytes, inited: " << obj.inited_
           << ", next2MB: " << obj.next2MB_ << ", remaining2MB: " << obj.remaining2MB_ << "]";

        return os;
    }

private:
    FlashBucketMemPool() = default;

    FlashBucketMemPool(const FlashBucketMemPool &) = delete;
    FlashBucketMemPool(FlashBucketMemPool &&) = delete;
    FlashBucketMemPool &operator=(const FlashBucketMemPool &) = delete;
    FlashBucketMemPool &operator=(FlashBucketMemPool &&) = delete;

    Result VerifyOption() noexcept;

private:
    uintptr_t startAddress_ = 0; /* address of reserved memory space */
    uint64_t capacity_ = 0;      /* size of reserved memory space */
    std::mutex mutex_;           /* mutex for member variables */
    bool inited_ = false;        /* initialized or not */
    uint32_t next2MB_ = 0;       /* index of next 2MB to be allocated */
    uint32_t remaining2MB_ = 0;  /* how many can be still allocated  */
};

inline Result FlashBucketMemPool::Allocate2MB(uintptr_t &address) noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (remaining2MB_ <= 0) {
        EM_LOG_INFO("No more space in mem pool, remaining2MB_: " << remaining2MB_ << ", inited_: " << inited_);
        return EM_NO_MORE_SPACE;
    }

    /* set address that allocated */
    address = startAddress_ + UN2MB * next2MB_;
    /* increase next2MB */
    ++next2MB_;
    /* decrease remaining 2MB count */
    --remaining2MB_;

    return EM_OK;
}

inline uintptr_t FlashBucketMemPool::StartAddress() const noexcept
{
    return startAddress_;
}

/**
 * Flash bucket allocator
 */
class FlashBucketAllocator {
public:
    void *Allocate(uint64_t size) noexcept;

    void Free(void *p) noexcept;

private:
    BucketMemBlock *head_ = nullptr;
    BucketSpinLock spinLock_;
};

} // namespace hashmap
} // namespace emb
} // namespace ock

#endif // MEMFABRIC_HYBRID_EMB_FLASH_BUCKET_ALLOCATOR_H
