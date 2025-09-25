/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "mmc_blob_allocator.h"

namespace ock {
namespace mmc {
bool MmcBlobAllocator::CanAlloc(uint64_t blobSize)
{
    if (!started_) {
        MMC_LOG_WARN("Allocator rank: " << rank_ << " mediaType: " << mediaType_ << " is stopped");
        return false;
    }
    SpaceRange anchor{0, AllocSizeAlignUp(blobSize)};

    spinlock_.lock();
    if (!started_) {
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
    if (!started_) {
        MMC_LOG_WARN("Allocator rank: " << rank_ << " mediaType: " << mediaType_ << " is stopped");
        return nullptr;
    }
    auto alignedSize = AllocSizeAlignUp(blobSize);
    SpaceRange anchor{0, alignedSize};

    spinlock_.lock();
    if (!started_) {
        spinlock_.unlock();
        MMC_LOG_WARN("Allocator rank: " << rank_ << " mediaType: " << mediaType_ << " is stopped");
        return nullptr;
    }
    auto sizePos = sizeTree_.lower_bound(anchor);
    if (sizePos == sizeTree_.end()) {
        spinlock_.unlock();
        MMC_LOG_WARN("Allocator rank: " << rank_ << " mediaType: " << mediaType_ << ", cap:" << allocatedSize_ << "/"
                                        << capacity_ << " cannot allocate with size: " << blobSize);
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

    return MmcMakeRef<MmcMemBlob>(rank_, bmAddr_ + targetOffset, blobSize, mediaType_, ALLOCATED);
}

Result MmcBlobAllocator::Release(const MmcMemBlobPtr &blob)
{
    if (blob == nullptr) {
        MMC_LOG_ERROR("blob is null");
        return MMC_ERROR;
    }
    auto alignedSize = AllocSizeAlignUp(blob->Size());
    auto blobAddr = blob->Gva();
    if (blobAddr < bmAddr_ || blobAddr + alignedSize > bmAddr_ + capacity_) {
        MMC_LOG_ERROR("blob address not in allocator");
        return MMC_ERROR;
    }

    auto offset = blobAddr - bmAddr_;
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

Result MmcBlobAllocator::BuildFromBlobs(std::map<std::string, MmcMemBlobDesc> &blobMap)
{
    spinlock_.lock();
    if (started_) {
        spinlock_.unlock();
        MMC_LOG_ERROR("rebuild allocator failed, rank: " << rank_ << " mediaType: " << mediaType_
            << ", allocator must not started and empty");
        return MMC_ERROR;
    }
    
    // 处理每个已分配的blob
    for (auto it = blobMap.begin(); it != blobMap.end();) {
        if (it->second.rank_ != rank_) {
            MMC_LOG_WARN("rebuild blob not match, allocator rank: " << rank_ << ", blob rank: " << it->second.rank_);
            it = blobMap.erase(it);
            continue;
        }

        if (it->second.mediaType_ != mediaType_) {
             ++it;
            continue;
        }
        uint64_t gva = it->second.gva_;
        uint64_t size = it->second.size_;
        uint64_t offset = gva - bmAddr_;  // 转换为分配器内部偏移

        Result res = ValidateAndAddAllocation(offset, size);
        if (res != MMC_OK) {
            MMC_LOG_ERROR("rebuild allocator failed, rank: " << rank_ << " mediaType: " << mediaType_
                                                             << ", blob off:" << offset << ", size: " << size);
            it = blobMap.erase(it);
            continue;
        }
        ++it;
    }
    spinlock_.unlock();
    return MMC_OK;
}

Result MmcBlobAllocator::ValidateAndAddAllocation(uint64_t offset, uint64_t size)
{
    // 验证地址范围有效性
    if (offset > std::numeric_limits<uint64_t>::max() - size) {
        MMC_LOG_ERROR("blob range overflow: offset:" << offset << ", size:" << size);
        return MMC_ERROR;
    }
    if (offset >= capacity_ || (offset + size) > capacity_) {
        MMC_LOG_ERROR("Blob out of range: offset: " << offset
                   << ", size: " << size << ", capacity: " << capacity_);
        return MMC_ERROR;
    }
    
    // 计算对齐后的大小
    uint64_t alignedSize = AllocSizeAlignUp(size);
    
    // 查找包含该分配的空闲块
    auto it = addressTree_.upper_bound(offset);
    if (it != addressTree_.begin()) {
        --it;  // 回退到可能包含offset的块
    }
    
    if (it == addressTree_.end() || it->first > offset ||
        (it->first + it->second) < (offset + alignedSize)) {
        MMC_LOG_ERROR("No matching free block for blob: offset=" << offset
                   << ", size=" << size);
        return MMC_ERROR;
    }
    
    // 从空闲树中移除该块
    uint64_t blockOffset = it->first;
    uint64_t blockSize = it->second;
    sizeTree_.erase(SpaceRange{blockOffset, blockSize});
    addressTree_.erase(it);
    
    // 分割剩余空间（前部）
    if (blockOffset < offset) {
        uint64_t frontSize = offset - blockOffset;
        SpaceRange left{blockOffset, frontSize};
        addressTree_.emplace(left.offset_, left.size_);
        sizeTree_.emplace(left);
    }
    
    // 分割剩余空间（后部）
    uint64_t endOffset = offset + alignedSize;
    uint64_t remainingSize = (blockOffset + blockSize) - endOffset;
    if (remainingSize > 0) {
        SpaceRange left{endOffset, remainingSize};
        addressTree_.emplace(left.offset_, left.size_);
        sizeTree_.emplace(left);
    }
    
    // 更新已分配大小
    allocatedSize_ += alignedSize;
    return MMC_OK;
}

uint64_t MmcBlobAllocator::AllocSizeAlignUp(uint64_t size)
{
    constexpr uint64_t alignSize = 4096UL;
    if (size > UINT64_MAX - alignSize + 1UL) {
        MMC_LOG_ERROR("Invalid size: " << size << " will occur overflow");
        return UINT64_MAX;
    }
    constexpr uint64_t alignSizeMask = ~(alignSize - 1UL);
    return (size + alignSize - 1UL) & alignSizeMask;
}
}
}