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

class MmcGlobalAllocator : public MmcReferable {
public:
    MmcGlobalAllocator() = default;

    Result Alloc(const AllocOptions &allocReq, std::vector<MmcMemBlobPtr> &blobs)
    {
        globalAllocLock_.LockRead();
        if (allocators_.empty()) {
            globalAllocLock_.UnlockRead();
            MMC_LOG_ERROR("Alloc allocators_ is empty");
            return MMC_ERROR;
        }
        Result ret;
        switch (allocReq.flags_) {
            case ALLOC_ARRANGE:
                ret = MmcLocalityStrategy::ArrangeLocality(allocators_, allocReq, blobs);
                break;
            
            case ALLOC_FORCE_BY_RANK:
                ret = MmcLocalityStrategy::ForceAssign(allocators_, allocReq, blobs);
                break;
            
            case ALLOC_RANDOM:
                ret = MmcLocalityStrategy::RandomAssign(allocators_, allocReq, blobs);
                break;
            
            default:
                ret = MmcLocalityStrategy::ArrangeLocality(allocators_, allocReq, blobs);
                break;
        }
        globalAllocLock_.UnlockRead();
        if (ret != MMC_OK && !blobs.empty()) {
            for (auto& blob : blobs) {
                Free(blob);
            }
            blobs.clear();
            MMC_LOG_ERROR("Alloc " << allocReq.blobSize_ << "failed, ret:" << ret);
        }
        return ret;
    };

    Result Free(const MmcMemBlobPtr& blob)
    {
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
            if (usedSize[i] * LEVEL_BASE > totalSize[i] * level) {
                return true;
            }
        }
        return false;
    }

private:
    MmcAllocators allocators_;
    ReadWriteLock globalAllocLock_;
};

using MmcGlobalAllocatorPtr = MmcRef<MmcGlobalAllocator>;

}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_GLOBAL_ALLOCATOR_H