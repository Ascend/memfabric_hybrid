/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_GLOBAL_ALLOCATOR_H
#define MEM_FABRIC_MMC_GLOBAL_ALLOCATOR_H

#include "mmc_blob_allocator.h"
#include "mmc_locality_strategy.h"
#include "mmc_read_write_lock.h"

namespace ock {
namespace mmc {

constexpr int LEVEL_BASE = 100;

using MmcMemPoolInitInfo = std::map<MmcLocation, MmcLocalMemlInitInfo>;

inline std::ostream &operator<<(std::ostream &os, const std::vector<MmcMemBlobPtr> &vec)
{
    os << ", Rank[";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) {
            os << ", ";
        }
        os << vec[i]->Rank();
    }
    os << "]";
    return os;
}

class MmcGlobalAllocator : public MmcReferable {
public:
    MmcGlobalAllocator() = default;

    // preferredRank为空时（当前实际至少有一个local rank，代码需要考虑为空场景）， numBlobs任意值
    // preferredRank非空时，不能重复，而且列表大小小于等于 numBlobs
    // preferredRank大于1的时候，必定是强制分配场景
    // handle :
    // replicaNum=4
    // preferredLocalServiceIDs=[0,1]
    // 0,1 强制，剩下的默认处理；默认处理时候需要排除强制rank列表
    Result Alloc(const AllocOptions &allocReq, std::vector<MmcMemBlobPtr> &blobs)
    {
        std::unordered_set<uint32_t> excludeRanks(allocReq.preferredRank_.begin(), allocReq.preferredRank_.end());
        if (allocReq.numBlobs_ < allocReq.preferredRank_.size() ||
            excludeRanks.size() != allocReq.preferredRank_.size()) {
            MMC_LOG_ERROR(allocReq);
            return MMC_ERROR;
        }
        globalAllocLock_.LockRead();
        // size=1 不一定是强制分配
        if (allocReq.preferredRank_.size() <= 1u) {
            auto ret = InnerAlloc(allocReq, blobs, {});
            globalAllocLock_.UnlockRead();
            if (ret != MMC_OK) {
                Free(blobs);
                MMC_LOG_ERROR("Simple alloc failed, ret: " << ret << ", allocReq: " << allocReq);
            }
            return ret;
        }
        Result result{};
        for (const uint32_t rank : allocReq.preferredRank_) {
            AllocOptions tmpAllocReq = allocReq;
            tmpAllocReq.preferredRank_ = {rank};
            tmpAllocReq.numBlobs_ = 1u;
            tmpAllocReq.flags_ = static_cast<uint32_t>(ALLOC_FORCE_BY_RANK);
            if ((result = InnerAlloc(tmpAllocReq, blobs, {})) != MMC_OK) {
                MMC_LOG_ERROR("Force alloc failed, result: " << result << ", allocReq: " << tmpAllocReq);
                globalAllocLock_.UnlockRead();
                Free(blobs);
                return result;
            }
        }
        if (allocReq.numBlobs_ - allocReq.preferredRank_.size() > 0) {
            AllocOptions tmpAllocReq = allocReq;
            tmpAllocReq.numBlobs_ = allocReq.numBlobs_ - allocReq.preferredRank_.size();
            tmpAllocReq.flags_ = static_cast<uint32_t>(ALLOC_ARRANGE);
            if ((result = InnerAlloc(tmpAllocReq, blobs, excludeRanks)) != MMC_OK) {
                MMC_LOG_ERROR("Arrange alloc failed, result: " << result << ", allocReq: " << tmpAllocReq);
                globalAllocLock_.UnlockRead();
                Free(blobs);
                return result;
            }
        }
        globalAllocLock_.UnlockRead();
        if (blobs.size() != allocReq.numBlobs_) {
            MMC_LOG_ERROR("More alloc failed, " << allocReq << blobs);
            Free(blobs);
            return MMC_ERROR;
        }
        MMC_LOG_DEBUG("More alloc, " << allocReq << blobs);
        return MMC_OK;
    }

    void Free(std::vector<MmcMemBlobPtr> &blobs) noexcept
    {
        for (const auto &blob : blobs) {
            Free(blob);
        }
        blobs.clear();
    }

    Result Free(const MmcMemBlobPtr& blob)
    {
        if (blob == nullptr) {
            MMC_LOG_ERROR("Free blob failed, blob is nullptr");
            return MMC_INVALID_PARAM;
        }
        globalAllocLock_.LockRead();
        const MmcLocation location{blob->Rank(), static_cast<MediaType>(blob->Type())};
        const auto iter = allocators_.find(location);
        if (iter == allocators_.end()) {
            globalAllocLock_.UnlockRead();
            MMC_LOG_ERROR("Free blob failed, location not found, rank: "
                << location.rank_ << ", mediaType: " << location.mediaType_);
            return MMC_INVALID_PARAM;
        }

        const auto &allocator = iter->second;
        if (allocator == nullptr) {
            globalAllocLock_.UnlockRead();
            MMC_LOG_ERROR("Free blob failed, allocator is nullptr");
            return MMC_ERROR;
        }
        Result ret = allocator->Release(blob);
        globalAllocLock_.UnlockRead();
        return ret;
    };

    Result Mount(const MmcLocation &loc, const MmcLocalMemlInitInfo &localMemInitInfo)
    {
        globalAllocLock_.LockWrite();
        auto iter = allocators_.find(loc);
        if (iter != allocators_.end()) {
            globalAllocLock_.UnlockWrite();
            MMC_LOG_INFO("not need mount at the existing position");
            return MMC_OK;
        }

        allocators_[loc] =
            MmcMakeRef<MmcBlobAllocator>(loc.rank_, loc.mediaType_, localMemInitInfo.bmAddr_,
                                         localMemInitInfo.capacity_);

        MMC_LOG_INFO("Mount bm on " << loc << ", capacity:" << localMemInitInfo.capacity_ << "  successfully");
        globalAllocLock_.UnlockWrite();
        return MMC_OK;
    }

    Result Start(const MmcLocation &loc)
    {
        globalAllocLock_.LockRead();
        const auto iter = allocators_.find(loc);
        if (iter == allocators_.end()) {
            globalAllocLock_.UnlockRead();
            MMC_LOG_ERROR("location not found, rank: " << loc.rank_ << ", mediaType: " << loc.mediaType_);
            return MMC_INVALID_PARAM;
        }

        const auto &allocator = iter->second;
        if (allocator == nullptr) {
            globalAllocLock_.UnlockRead();
            MMC_LOG_ERROR("Start failed, allocator is nullptr");
            return MMC_ERROR;
        }
        allocator->Start();
        globalAllocLock_.UnlockRead();
        return MMC_OK;
    }

    Result Stop(const MmcLocation &loc)
    {
        globalAllocLock_.LockRead();
        const auto iter = allocators_.find(loc);
        if (iter == allocators_.end()) {
            globalAllocLock_.UnlockRead();
            MMC_LOG_ERROR("location not found, rank: " << loc.rank_ << ", mediaType: " << loc.mediaType_);
            return MMC_INVALID_PARAM;
        }

        const auto &allocator = iter->second;
        if (allocator == nullptr) {
            globalAllocLock_.UnlockRead();
            MMC_LOG_ERROR("Stop failed, allocator is nullptr");
            return MMC_ERROR;
        }
        allocator->Stop();
        MMC_LOG_INFO("Stop one bm successfully, bmRankId=" << loc.rank_);
        globalAllocLock_.UnlockRead();
        return MMC_OK;
    }

    Result Unmount(const MmcLocation &loc)
    {
        globalAllocLock_.LockWrite();
        auto iter = allocators_.find(loc);
        if (iter == allocators_.end()) {
            globalAllocLock_.UnlockWrite();
            MMC_LOG_ERROR("Cannot find the given {rank:" << loc.rank_ << ", type:" << loc.mediaType_
                                                         << "} in the mem pool");
            return MMC_INVALID_PARAM;
        }
        if (iter->second == nullptr) {
            globalAllocLock_.UnlockWrite();
            MMC_LOG_ERROR("Unmount failed, allocator is nullptr");
            return MMC_ERROR;
        }
        if (!iter->second->CanUnmount()) {
            globalAllocLock_.UnlockWrite();
            MMC_LOG_ERROR("Cannot unmount the given {rank:" << loc.rank_ << ", type:" << loc.mediaType_
                                                            << "}  in the mem pool, space is in use");
            return MMC_INVALID_PARAM;
        }
        allocators_.erase(iter);
        MMC_LOG_INFO("Unmount one bm successfully, bmRankId=" << loc.rank_);
        globalAllocLock_.UnlockWrite();
        return MMC_OK;
    }

    Result BuildFromBlobs(const MmcLocation &location, std::map<std::string, MmcMemBlobDesc> &blobMap)
    {
        globalAllocLock_.LockRead();
        const auto iter = allocators_.find(location);
        if (iter == allocators_.end()) {
            globalAllocLock_.UnlockRead();
            MMC_LOG_ERROR("Build from blobs failed, location not found, rank: "
                << location.rank_ << ", mediaType: " << location.mediaType_);
            return MMC_INVALID_PARAM;
        }

        const auto &allocator = iter->second;
        if (allocator == nullptr) {
            globalAllocLock_.UnlockRead();
            MMC_LOG_ERROR("BuildFromBlobs failed, allocator is nullptr");
            return MMC_ERROR;
        }
        Result ret = allocator->BuildFromBlobs(blobMap);
        globalAllocLock_.UnlockRead();
        return ret;
    }

    void GetUsedInfo(uint64_t (&totalSize)[MEDIA_NONE], uint64_t (&usedSize)[MEDIA_NONE])
    {
        globalAllocLock_.LockRead();
        for (auto& allocator : allocators_) {
            auto result = allocator.second->GetUsageInfo();
            totalSize[allocator.first.mediaType_] += result.first;
            usedSize[allocator.first.mediaType_] += result.second;
        }
        globalAllocLock_.UnlockRead();
    }

    uint64_t GetFreeSpace(MediaType type)
    {
        if (type == MEDIA_NONE) {
            return 0;
        }
        uint64_t totalSize[MEDIA_NONE] = {0};
        uint64_t usedSize[MEDIA_NONE] = {0};
        GetUsedInfo(totalSize, usedSize);
        return totalSize[type] - usedSize[type];
    }

    bool NeedEvict(uint64_t level)
    {
        uint64_t totalSize[MEDIA_NONE] = {0};
        uint64_t usedSize[MEDIA_NONE] = {0};
        GetUsedInfo(totalSize, usedSize);
        for (uint32_t i = 0; i < MEDIA_NONE; i++) {
            // 只要一个类型的池触发水位，即淘汰
            if (usedSize[i] > std::numeric_limits<uint64_t>::max() / LEVEL_BASE ||
                (level != 0 && (totalSize[i] > std::numeric_limits<uint64_t>::max() / level))) {
                MMC_LOG_ERROR("overflow: usedSize: " << usedSize[i] << ", LEVEL_BASE: " <<
                    LEVEL_BASE << ", totalSize: " << totalSize[i] << ", level: " << level);
                return false;
            }
            if (usedSize[i] * LEVEL_BASE > totalSize[i] * level) {
                return true;
            }
        }
        return false;
    }

private:
    Result InnerAlloc(const AllocOptions &allocReq, std::vector<MmcMemBlobPtr> &blobs,
                      const std::unordered_set<uint32_t> &excludeRanks) const
    {
        if (allocators_.empty()) {
            MMC_LOG_ERROR("Alloc allocators_ is empty");
            return MMC_ERROR;
        }
        Result ret;
        switch (allocReq.flags_) {
            case ALLOC_ARRANGE:
                ret = MmcLocalityStrategy::ArrangeLocality(allocators_, allocReq, blobs, excludeRanks);
                break;

            case ALLOC_FORCE_BY_RANK:
                ret = MmcLocalityStrategy::ForceAssign(allocators_, allocReq, blobs);
                break;

            case ALLOC_RANDOM:
                ret = MmcLocalityStrategy::RandomAssign(allocators_, allocReq, blobs);
                break;

            default:
                ret = MmcLocalityStrategy::ArrangeLocality(allocators_, allocReq, blobs, excludeRanks);
                break;
        }
        return ret;
    }

    MmcAllocators allocators_;
    ReadWriteLock globalAllocLock_;
};

using MmcGlobalAllocatorPtr = MmcRef<MmcGlobalAllocator>;

}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_GLOBAL_ALLOCATOR_H