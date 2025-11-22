/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hybm_logger.h"
#include "hybm_rbtree_range_pool.h"

namespace ock {
namespace mf {
bool RangeSizeFirst::operator()(const ock::mf::SpaceRange &sr1, const ock::mf::SpaceRange &sr2) const noexcept
{
    if (sr1.size != sr2.size) {
        return sr1.size < sr2.size;
    }

    return sr1.offset < sr2.offset;
}

RbtreeRangePool::RbtreeRangePool(uint8_t *address, uint64_t size) noexcept : baseAddress{address}, totalSize{size}
{
    pthread_spin_init(&lock, 0);
    addressTree[0] = size;
    sizeTree.insert({0, size});
}

RbtreeRangePool::~RbtreeRangePool() noexcept
{
    pthread_spin_destroy(&lock);
}

bool RbtreeRangePool::CanAllocate(uint64_t size) const noexcept
{
    SpaceRange anchor{0, AllocateSizeAlignUp(size)};

    pthread_spin_lock(&lock);
    bool exists = (sizeTree.lower_bound(anchor) != sizeTree.end());
    pthread_spin_unlock(&lock);

    return exists;
}

AllocatedElement RbtreeRangePool::Allocate(uint64_t size) noexcept
{
    auto alignedSize = AllocateSizeAlignUp(size);
    SpaceRange anchor{0, alignedSize};
    pthread_spin_lock(&lock);
    auto sizePos = sizeTree.lower_bound(anchor);
    if (sizePos == sizeTree.end()) {
        pthread_spin_unlock(&lock);
        BM_LOG_ERROR("cannot allocate with size: " << size);
        return AllocatedElement{};
    }
    if (baseAddress == nullptr) {
        pthread_spin_unlock(&lock);
        BM_LOG_ERROR("base is a null pointer.");
        return AllocatedElement{};
    }
    auto targetOffset = sizePos->offset;
    auto targetSize = sizePos->size;
    auto addrPos = addressTree.find(targetOffset);
    if (addrPos == addressTree.end()) {
        pthread_spin_unlock(&lock);
        BM_LOG_ERROR("offset: " << targetOffset << "size: " << targetSize << "in size tree, not in address tree.");
        return AllocatedElement{};
    }

    sizeTree.erase(sizePos);
    addressTree.erase(addrPos);
    if (targetSize > alignedSize) {
        SpaceRange left{targetOffset + alignedSize, targetSize - alignedSize};
        addressTree.emplace(left.offset, left.size);
        sizeTree.emplace(left);
    }
    pthread_spin_unlock(&lock);
    return AllocatedElement{baseAddress + targetOffset, size, this};
}

bool RbtreeRangePool::Release(const AllocatedElement &element) noexcept
{
    auto alignedSize = AllocateSizeAlignUp(element.Size());
    auto elemAddr = element.Address();
    if (baseAddress == nullptr || elemAddr == nullptr) {
        BM_LOG_ERROR("The addr is a null pointer.");
        return false;
    }
    if (elemAddr < baseAddress || elemAddr >= baseAddress + totalSize) {
        BM_LOG_ERROR("element address not in this range pool.");
        return false;
    }

    auto offset = static_cast<uint64_t>(elemAddr - baseAddress);
    uint64_t finalOffset = offset;
    uint64_t finalSize = alignedSize;

    pthread_spin_lock(&lock);
    auto prevAddrPos = addressTree.lower_bound(offset);
    if (prevAddrPos != addressTree.begin()) {
        --prevAddrPos;
        if (prevAddrPos != addressTree.end() && prevAddrPos->first + prevAddrPos->second == offset) { // 合并前一个range
            finalOffset = prevAddrPos->first;
            finalSize += prevAddrPos->second;
            sizeTree.erase(SpaceRange{prevAddrPos->first, prevAddrPos->second});
            addressTree.erase(prevAddrPos);
        }
    }

    auto nextAddrPos = addressTree.find(offset + alignedSize);
    if (nextAddrPos != addressTree.end()) { // 合并后一个range
        finalSize += nextAddrPos->second;
        sizeTree.erase(SpaceRange{nextAddrPos->first, nextAddrPos->second});
        addressTree.erase(nextAddrPos);
    }

    addressTree.emplace(finalOffset, finalSize);
    sizeTree.emplace(SpaceRange{finalOffset, finalSize});

    pthread_spin_unlock(&lock);
    return true;
}

uint64_t RbtreeRangePool::AllocateSizeAlignUp(uint64_t inputSize) noexcept
{
    constexpr uint64_t alignSize = 4096UL;
    constexpr uint64_t alignSizeMask = ~(alignSize - 1UL);
    return (inputSize + alignSize - 1UL) & alignSizeMask;
}
} // namespace mf
} // namespace ock