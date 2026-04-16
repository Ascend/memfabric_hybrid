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
#include "emb_flash_bucket_allocator.h"
#include "emb_env_helper.h"

namespace ock {
namespace emb {
namespace hashmap {
FlashBucketMemPool &FlashBucketMemPool::Instance() noexcept
{
    static FlashBucketMemPool gPool;
    return gPool;
}

FlashBucketMemPool::~FlashBucketMemPool()
{
    UnInitialize();
}

Result FlashBucketMemPool::Initialize() noexcept
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

    auto bitsetSize = capacity_ / UN2MB;
    result = allocatedBitSet_.Initialize(bitsetSize);
    if (result != EM_OK) {
        EM_LOG_ERROR("Initialize bitset for allocation failed");
        return result;
    }

    /*
     * reserve memory space
     * here we only reserve memory space, no physical memory allocated
     */
    auto address = mmap(reinterpret_cast<void *>(startAddress_), capacity_, PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (address == nullptr) {
        EM_LOG_ERROR("Reserve memory space [start address: " << std::hex << reinterpret_cast<void *>(startAddress_)
                                                             << std::dec << ", size: " << capacity_
                                                             << "] failed, errno: " << errno);
        return EM_RESERVE_MEMORY_SPACE_FAILED;
    }

    EM_LOG_DEBUG("Initialized memory address: " << std::hex << reinterpret_cast<void *>(startAddress_) << std::dec
                                                << ", capacity: " << capacity_
                                                << " bytes, remaining2MB: " << allocatedBitSet_.Capacity()
                                                << ", allocated: " << allocatedBitSet_.Count());

    inited_ = true;
    return EM_OK;
}

Result FlashBucketMemPool::VerifyOption() noexcept
{
    const auto startAddressTB = EnvHelper::gHashmapOverflowBucketPoolStartAddrTB;
    if (startAddressTB < UN1 || startAddressTB >= UN128) {
        EM_LOG_ERROR("Invalid start address for overflowed bucket memory pool, which should be 1~127");
        return EM_INVALID_PARAM;
    }

    const auto poolSizeGB = EnvHelper::gHashmapOverflowBucketPoolSizeGB;
    if (poolSizeGB < UN1 || poolSizeGB > UN64) {
        EM_LOG_ERROR("Invalid size for overflowed bucket memory pool, which should be 1~64");
        return EM_INVALID_PARAM;
    }

    startAddress_ = EnvHelper::gHashmapOverflowBucketPoolStartAddrTB;
    startAddress_ = startAddress_ * UN1TB;

    capacity_ = poolSizeGB;
    capacity_ = capacity_ * UN1GB;

    return EM_OK;
}

void FlashBucketMemPool::UnInitialize() noexcept
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