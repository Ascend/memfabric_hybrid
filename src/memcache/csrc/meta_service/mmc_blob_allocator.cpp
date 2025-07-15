/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#include "mmc_blob_allocator.h"

namespace ock {
namespace mmc {
bool MmcBlobAllocator::CanAlloc(uint64_t blobSize)
{
    if (isStop_ == true) {
        MMC_LOG_WARN("Allocator rank: " << rank_ << " mediaType: " << mediaType_ << " is stopped");
        return false;
    }
    SpaceRange anchor{0, AllocSizeAlignUp(blobSize)};

    spinlock_.lock();
    if (isStop_ == true) {
        spinlock_.unlock();
        MMC_LOG_WARN("Allocator rank: " << rank_ << " mediaType: " << mediaType_ << " is stopped");
        return false;
    }
    bool exists = (sizeTree_.lower_bound(anchor) != sizeTree_.end());
    spinlock_.unlock();

    return exists;
}

MmcMemBlobPtr MmcBlobAllocator::Alloc(uint64_t blobSize)
{
    if (isStop_ == true) {
        MMC_LOG_WARN("Allocator rank: " << rank_ << " mediaType: " << mediaType_ << " is stopped");
        return nullptr;
    }
    auto alignedSize = AllocSizeAlignUp(blobSize);
    SpaceRange anchor{0, alignedSize};

    spinlock_.lock();
    if (isStop_ == true) {
        spinlock_.unlock();
        MMC_LOG_WARN("Allocator rank: " << rank_ << " mediaType: " << mediaType_ << " is stopped");
        return nullptr;
    }
    auto sizePos = sizeTree_.lower_bound(anchor);
    if (sizePos == sizeTree_.end()) {
        spinlock_.unlock();
        MMC_LOG_WARN("Allocator rank: " << rank_ << " mediaType: " << mediaType_
            << " cannot allocate with size: " << blobSize);
        return nullptr;
    }

    auto targetOffset = sizePos->offset_;
    auto targetSize = sizePos->size_;
    auto addrPos = addressTree_.find(targetOffset);
    if (addrPos == addressTree_.end()) {
        spinlock_.unlock();
        MMC_LOG_ERROR("Allocator rank: " << rank_ << " mediaType: " << mediaType_
            << " offset in size tree, not in address tree");
        return nullptr;
    }

    sizeTree_.erase(sizePos);
    addressTree_.erase(addrPos);
    if (targetSize > alignedSize) {
        SpaceRange left{targetOffset + alignedSize, targetSize - alignedSize};
        addressTree_.emplace(left.offset_, left.size_);
        sizeTree_.emplace(left);
    }
    allocatedSize_ += alignedSize;
    spinlock_.unlock();

    return MmcMakeRef<MmcMemBlob>(rank_, bm_ + targetOffset, blobSize, mediaType_, ALLOCATED);
}

Result MmcBlobAllocator::Release(const MmcMemBlobPtr &blob)
{
    if (blob == nullptr) {
        MMC_LOG_ERROR("blob is null");
        return MMC_ERROR;
    }
    auto alignedSize = AllocSizeAlignUp(blob->Size());
    auto blobAddr = blob->Gva();
    if (blobAddr < bm_ || blobAddr + alignedSize > bm_ + capacity_) {
        MMC_LOG_ERROR("blob address not in allocator");
        return MMC_ERROR;
    }

    auto offset = blobAddr - bm_;
    uint64_t finalOffset = offset;
    uint64_t finalSize = alignedSize;

    spinlock_.lock();
    auto prevAddrPos = addressTree_.lower_bound(offset);
    if (prevAddrPos != addressTree_.end() && prevAddrPos->first == offset) {
        spinlock_.unlock();
        MMC_LOG_ERROR("blob already released");
        return MMC_ERROR;
    }
    if (prevAddrPos != addressTree_.begin()) {
        --prevAddrPos;
        if (prevAddrPos->first + prevAddrPos->second > offset) { 
            spinlock_.unlock();
            MMC_LOG_ERROR("blob already released");
            return MMC_ERROR;
        }
        if (prevAddrPos != addressTree_.end() &&
            prevAddrPos->first + prevAddrPos->second == offset) {  // 合并前一个range
            finalOffset = prevAddrPos->first;
            finalSize += prevAddrPos->second;
            sizeTree_.erase(SpaceRange{prevAddrPos->first, prevAddrPos->second});
            addressTree_.erase(prevAddrPos);
        }
    }

    auto nextAddrPos = addressTree_.find(offset + alignedSize);
    if (nextAddrPos != addressTree_.end()) {
        finalSize += nextAddrPos->second;
        sizeTree_.erase(SpaceRange{nextAddrPos->first, nextAddrPos->second});
        addressTree_.erase(nextAddrPos);
    }

    addressTree_.emplace(finalOffset, finalSize);
    sizeTree_.emplace(SpaceRange{finalOffset, finalSize});

    allocatedSize_ -= alignedSize;

    spinlock_.unlock();
    return MMC_OK;
}

uint64_t MmcBlobAllocator::AllocSizeAlignUp(uint64_t size)
{
    constexpr uint64_t alignSize = 4096UL;
    constexpr uint64_t alignSizeMask = ~(alignSize - 1UL);
    return (size + alignSize - 1UL) & alignSizeMask;
}
}
}