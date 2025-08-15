/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_GLOBAL_ALLOCATOR_H
#define MEM_FABRIC_MMC_GLOBAL_ALLOCATOR_H

#include "mmc_blob_allocator.h"
#include "mmc_locality_strategy.h"
#include "mmc_read_write_lock.h"

namespace ock {
namespace mmc {

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
        Result ret = MmcLocalityStrategy::ArrangeLocality(allocators_, allocReq, blobs);
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
            MmcMakeRef<MmcBlobAllocator>(loc.rank_, loc.mediaType_, localMemInitInfo.bmAddr_, localMemInitInfo.capacity_);
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

    uint64_t GetUsageRate()
    {
        uint64_t totalSize = 0;
        uint64_t usedSize = 0;
        globalAllocLock_.LockRead();
        for (auto &allocator : allocators_) {
            auto result = allocator.second->GetUsageInfo();
            totalSize += result.first;
            usedSize += result.second;
        }
        globalAllocLock_.UnlockRead();
        return (usedSize * 100 / totalSize);
    }

private:
    MmcAllocators allocators_;
    ReadWriteLock globalAllocLock_;
};

using MmcGlobalAllocatorPtr = MmcRef<MmcGlobalAllocator>;

}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_GLOBAL_ALLOCATOR_H