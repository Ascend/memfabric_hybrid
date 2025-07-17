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
    MemObjMetaLruItem lruItem;
    auto ret = objMetaLookupMap_.Find(key, lruItem);
    if (ret == MMC_OK) {
        objMeta = lruItem.memObjMetaPtr_;
        UpdateLRU(key, lruItem);
        objMeta->ExtendLease(defaultTtlMs_);
    }
    return ret;
}

Result MmcMetaManager::BatchGet(const std::vector<std::string> &keys, std::vector<MmcMemObjMetaPtr> &objMetas,
                                std::vector<Result> &getResults)
{
    objMetas.resize(keys.size());
    getResults.resize(keys.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        const std::string &key = keys[i];
        MemObjMetaLruItem lruItem;
        Result ret = objMetaLookupMap_.Find(key, lruItem);
        MmcMemObjMetaPtr objMeta = lruItem.memObjMetaPtr_;
        getResults[i] = ret;

        if (ret == MMC_OK) {
            objMeta->ExtendLease(defaultTtlMs_ * keys.size());
            objMetas[i] = objMeta;
            MMC_LOG_DEBUG("Key " << key << " found successfully");
        } else {
            objMetas[i] = nullptr;
            MMC_LOG_DEBUG("Key " << key << " not found");
        }
    }

    for (const Result &r : getResults) {
        if (r != MMC_OK) {
            return r;
        }
    }
    return MMC_OK;
}

// TODO: Check threshold， if above， try to remove to free space
// TODO: 检测不能是相同的key
Result MmcMetaManager::Alloc(const std::string &key, const AllocOptions &allocOpt, MmcMemObjMetaPtr &objMeta)
{
    // TODO 1: 不能阻塞
    // TODO 2: 要计算出可释放的空间

    if (globalAllocator_->TouchedThreshold(EVICT_THRESHOLD_HIGH)) {
        DoEviction();
    }

    objMeta = MmcMakeRef<MmcMemObjMeta>();
    std::vector<MmcMemBlobPtr> blobs;

    Result ret = globalAllocator_->Alloc(allocOpt, blobs);
    if (ret == MMC_ERROR) {
        return MMC_ERROR;
    }

    for (auto &blob : blobs) {
        objMeta->AddBlob(blob);
    }

    // put into lru and create lruItem
    lruList_.push_front(key);
    MemObjMetaLruItem lruItem{objMeta, lruList_.begin()};

    // insert into lookup map
    objMetaLookupMap_.Insert(key, lruItem);

    objMeta->ExtendLease(defaultTtlMs_);
    return MMC_OK;
}

Result MmcMetaManager::UpdateState(const std::string &key, const MmcLocation &loc, const BlobActionResult &actRet)
{
    MemObjMetaLruItem lruItem;
    Result ret = objMetaLookupMap_.Find(key, lruItem);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("UpdateState: Cannot find memObjMeta!");
        return MMC_UNMATCHED_KEY;
    }

    MmcBlobFilterPtr filter = MmcMakeRef<MmcBlobFilter>(loc.rank_, loc.mediaType_, NONE);
    MMC_ASSERT(filter != nullptr);
    std::vector<MmcMemBlobPtr> blobs = lruItem.memObjMetaPtr_->GetBlobs(filter);

    if (blobs.size() != 1) {
        MMC_LOG_ERROR("UpdateState: More than one blob in one position!");
        return MMC_ERROR;
    }
    ret = blobs[0]->UpdateState(actRet);
    if (ret != MMC_OK) {
        MMC_LOG_WARN("UpdateState Fail!");
        return ret;
    }
    return MMC_OK;
}

// TODO: 遍历的时候有问题
Result MmcMetaManager::Remove(const std::string &key)
{
    MemObjMetaLruItem lruItem;
    auto ret = objMetaLookupMap_.Find(key, lruItem);
    if (ret != MMC_OK) {
        return MMC_UNMATCHED_KEY;
    }
    MmcMemObjMetaPtr objMeta = lruItem.memObjMetaPtr_;

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
        lruList_.erase(lruItem.lruIter_);
        objMeta = nullptr;
        return MMC_OK;
    } else {
        objMetaLookupMap_.Erase(key);
        std::lock_guard<std::mutex> lk(removeListLock_);
        removeList_.push_back(objMeta);
        return MMC_OK;
    }
}

Result MmcMetaManager::BatchRemove(const std::vector<std::string> &keys, std::vector<Result> &remove_results)
{
    remove_results.resize(keys.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        const std::string &key = keys[i];
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
        const uint64_t nowMs = ock::dagger::Monotonic::TimeNs() / 1000U;
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
    ret = globalAllocator_->Stop(loc);
    if (ret != MMC_OK) {
        return ret;
    }
    // Force delete the blobs
    MmcBlobFilterPtr filter = MmcMakeRef<MmcBlobFilter>(loc.rank_, loc.mediaType_, NONE);
    std::vector<std::string> tempKeys;

    for (auto iter = objMetaLookupMap_.begin(); iter != objMetaLookupMap_.end(); ++iter) {
        std::string key = (*iter).first;
        MemObjMetaLruItem lruItem = (*iter).second;

        ret = ForceRemoveBlobs(lruItem.memObjMetaPtr_, filter);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Fail to force remove blobs in MmcMetaManager::Unmount!");
            return MMC_ERROR;
        }
        if (lruItem.memObjMetaPtr_->NumBlobs() == 0) {
            tempKeys.push_back(key);
            lruList_.erase(lruItem.lruIter_);
        }
    }

    for (const std::string &tempKey : tempKeys) {
        objMetaLookupMap_.Erase(tempKey);
    }

    ret = globalAllocator_->Unmount(loc);
    return ret;
}

Result MmcMetaManager::ExistKey(const std::string &key, Result &found)
{
    found = objMetaLookupMap_.Find(key);
    if (found != MMC_OK) {
        MMC_LOG_WARN("ExistKey cannot match an object with key : " << key);
    }
    return MMC_OK;
}

Result MmcMetaManager::BatchExistKey(const std::vector<std::string> &keys, std::vector<Result> &results, Result &found)
{
    results.reserve(keys.size());
    for (size_t i = 0; i < keys.size(); ++i) {
        results.emplace_back(objMetaLookupMap_.Find(keys[i]));
        if (results.back() == MMC_OK) {
            found = MMC_OK;
        }
    }
    return MMC_OK;
}

void MmcMetaManager::AsyncRemoveThreadFunc()
{
    while (true) {
        std::unique_lock<std::mutex> lock(removeThreadLock_);
        if (!removeThreadCv_.wait_for(lock, std::chrono::milliseconds(defaultTtlMs_), [this] {return removePredicate_;}))
        {
            MMC_LOG_DEBUG("async remove in thread ttl " << defaultTtlMs_ << " will remove count " << removeList_.size() <<
                                                        " thread id " << pthread_self());
            std::lock_guard<std::mutex> lg(removeListLock_);
            MmcMemObjMetaPtr objMeta;
            for (auto iter = removeList_.begin(); iter != removeList_.end();) {
                objMeta = *iter;
                if (!objMeta->IsLeaseExpired()) {
                    iter++;
                    continue;
                }
                std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs();
                for (size_t i = 0; i < blobs.size(); i++) {
                    Result ret = globalAllocator_->Free(blobs[i]);
                    if (ret != MMC_OK) {
                        MMC_LOG_ERROR("Error in free blobs!");
                    }
                }
                objMeta->RemoveBlobs();
                iter = removeList_.erase(iter);
            }
        } else {
            MMC_LOG_DEBUG("async thread destroy, thread id " << pthread_self());
            break;
        }
    }
}

Result MmcMetaManager::Query(const std::string &key, MemObjQueryInfo &queryInfo)
{
    MmcMemObjMetaPtr objMeta;
    if (objMetaLookupMap_.Find(key, objMeta) != MMC_OK) {
        MMC_LOG_WARN("Cannot find MmcMemObjMeta with key : " << key);
        return MMC_UNMATCHED_KEY;
    }
    queryInfo = objMeta->QueryInfo();
    return MMC_OK;
}

}  // namespace mmc
}  // namespace ock