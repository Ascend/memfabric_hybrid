/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#include "mmc_meta_manager.h"
#include <chrono>
#include <thread>

namespace ock {
namespace mmc {

Result MmcMetaManager::Get(const std::string &key, MmcMemObjMetaPtr &objMeta)
{
    auto ret = objMetaLookupMap_.Find(key, objMeta);
    if (ret == MMC_OK) {
        objMeta->ExtendLease(1000U);
    }
    return ret;
}

// TODO: Check threshold， if above， try to remove to free space
// TODO: 检测不能是相同的key
Result MmcMetaManager::Alloc(const std::string &key, const AllocOptions &allocOpt, MmcMemObjMetaPtr &objMeta)
{
    objMeta = MmcMakeRef<MmcMemObjMeta>();
    std::vector<MmcMemBlobPtr> blobs;

    Result ret = globalAllocator_->Alloc(allocOpt, blobs);
    if (ret == MMC_ERROR) {
        return MMC_ERROR;
    }

    for (auto &blob : blobs) {
        objMeta->AddBlob(blob);
    }
    objMetaLookupMap_.Insert(key, objMeta);
    objMeta->ExtendLease(1000U);
    return MMC_OK;
}

Result MmcMetaManager::GetBlobs(const std::string &key, const MmcBlobFilterPtr &filter,
                                std::vector<MmcMemBlobPtr> &blobs)
{
    MmcMemObjMetaPtr objMeta;
    Result ret = objMetaLookupMap_.Find(key, objMeta);
    /* TODO check ret firstly, then get blobs from objMeta in case of objMeta is nullptr */
    blobs = objMeta->GetBlobs(filter);
    if (ret == MMC_OK && !blobs.empty()) {
        objMeta->ExtendLease(1000U);
        return MMC_OK;
    }

    return MMC_ERROR;
}

Result MmcMetaManager::UpdateState(const std::string &key, const MmcLocation &loc, const BlobActionResult &actRet)
{
    std::vector<MmcMemBlobPtr> blobs;
    MmcBlobFilterPtr filter = MmcMakeRef<MmcBlobFilter>(loc.rank_, loc.mediaType_, NONE);
    MMC_ASSERT(filter != nullptr);
    if (GetBlobs(key, filter, blobs) != MMC_OK) {
        MMC_LOG_ERROR("UpdateState: Cannot find blobs!");
        return MMC_UNMATCHED_KEY;
    }
    if (blobs.size() != 1) {
        MMC_LOG_ERROR("UpdateState: More than one blob in one position!");
        return MMC_ERROR;
    }
    Result ret = blobs[0]->UpdateState(actRet);
    if (ret != MMC_OK) {
        MMC_LOG_WARN("UpdateState Fail!");
        return ret;
    }
    return MMC_OK;
}

Result MmcMetaManager::Remove(const std::string &key)
{
    MmcMemObjMetaPtr objMeta;
    auto ret = objMetaLookupMap_.Find(key, objMeta);
    if (ret != MMC_OK) {
        return MMC_UNMATCHED_KEY;
    }
    if (objMeta->IsLeaseExpired()) {
        std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs();
        for (size_t i = 0; i < blobs.size(); i++) {
            Result ret = globalAllocator_->Free(blobs[i]);
            if (ret != MMC_OK) {
                MMC_LOG_ERROR("Error in free blobs!");
                return ret;
            }
        }
        ret = objMeta->RemoveBlobs();
        objMetaLookupMap_.Erase(key);
        objMeta = nullptr;
        return MMC_OK;
    } else {
        return MMC_LEASE_NOT_EXPIRED;
    }
}

Result MmcMetaManager::BatchRemove(const std::vector<std::string>& keys, std::vector<Result>& remove_results) {
    remove_results.resize(keys.size());
    
    for (size_t i = 0; i < keys.size(); ++i) {
        const std::string& key = keys[i];
        Result ret = Remove(key);
        remove_results[i] = ret;
        if (ret != MMC_OK && ret != MMC_UNMATCHED_KEY && ret != MMC_LEASE_NOT_EXPIRED) {
            MMC_LOG_ERROR("Failed to remove key: " << key);
        }
    }
    
    return MMC_OK;
}

Result MmcMetaManager::ForceRemoveBlobs(const MmcMemObjMetaPtr &objMeta, const MmcBlobFilterPtr &filter)
{
    Result ret;
    std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs(filter);
    for (auto &blob : blobs) {
        ret = blob->UpdateState(MMC_REMOVE_START);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Fail to update state in ForceRemove!");
            return MMC_UNMATCHED_STATE;
        }
    }
    if (!objMeta->IsLeaseExpired()) {
        const uint64_t nowMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count();
        uint64_t timeLeft = objMeta->Lease() > nowMs ? objMeta->Lease() - nowMs : 0U;
        std::chrono::milliseconds leaseLeft(timeLeft);
        std::this_thread::sleep_for(leaseLeft);
    }
    for (size_t i = 0; i < blobs.size(); i++) {
        Result ret = globalAllocator_->Free(blobs[i]);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Error in free blobs!");
            return ret;
        }
    }
    ret = objMeta->RemoveBlobs(filter);
    return MMC_OK;
}

Result MmcMetaManager::Mount(const MmcLocation &loc, const MmcLocalMemlInitInfo &localMemInitInfo)
{
    return globalAllocator_->Mount(loc, localMemInitInfo);
}

Result MmcMetaManager::Unmount(const MmcLocation &loc)
{
    Result ret;
    // Force delete the blobs
    MmcBlobFilterPtr filter = MmcMakeRef<MmcBlobFilter>(loc.rank_, loc.mediaType_, NONE);
    std::vector<std::string> tempKeys;

    for (auto iter = objMetaLookupMap_.begin(); iter != objMetaLookupMap_.end(); ++iter) {
        ret = ForceRemoveBlobs((*iter).second, filter);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Fail to force remove blobs in MmcMetaManager::Unmount!");
            return MMC_ERROR;
        }
        if ((*iter).second->NumBlobs() == 0) {
            tempKeys.push_back((*iter).first);
        }
    }

    for (const std::string& tempKey : tempKeys) {
        objMetaLookupMap_.Erase(tempKey);
    }

    ret = globalAllocator_->Unmount(loc);
    return ret;
}

Result MmcMetaManager::ExistKey(const std::string &key)
{
    auto ret = objMetaLookupMap_.Find(key);
    return (ret == MMC_OK) ? MMC_OK : MMC_UNMATCHED_KEY;
}

Result MmcMetaManager::BatchExistKey(const std::vector<std::string> &keys, std::vector<Result> &results)
{
    results.reserve(keys.size());
    bool has_exist = false;
    for (size_t i = 0; i < keys.size(); ++i) {
        results.emplace_back(objMetaLookupMap_.Find(keys[i]));
        if (results.back() == MMC_OK) {
            has_exist = true;
        }
    }
    return (has_exist) ? MMC_OK : MMC_UNMATCHED_KEY;
}

}  // namespace mmc
}  // namespace ock