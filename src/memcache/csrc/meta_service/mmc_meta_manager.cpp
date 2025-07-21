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
Result MmcMetaManager::Alloc(const std::string &key, const AllocOptions &allocOpt, uint32_t requestId,
                             MmcMemObjMetaPtr &objMeta)
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
        blob->UpdateState(blob->Rank(), requestId, MMC_ALLOCATED_OK);
        objMeta->AddBlob(blob);
    }

    // put into lru and create lruItem
    lruList_.push_front(key);
    MemObjMetaLruItem lruItem{objMeta, lruList_.begin()};

    // insert into lookup map
    objMetaLookupMap_.Insert(key, lruItem);

    return MMC_OK;
}

Result MmcMetaManager::UpdateState(const std::string &key, const MmcLocation &loc, uint32_t rankId, uint32_t operateId,
                                   const BlobActionResult &actRet)
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
    ret = blobs[0]->UpdateState(rankId, operateId, actRet);
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
    objMeta->Lock();
    if (objMeta->NumBlobs() == 0){
        objMeta->Unlock();
        return MMC_OK;
    }

    std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs();
    for (size_t i = 0; i < blobs.size(); i++) {
        MMC_LOG_ERROR_AND_RETURN_NOT_OK(blobs[i]->UpdateState(0, 0, MMC_REMOVE_START),
                                        "remove op, meta update failed, key " << key);
        if (globalAllocator_->Free(blobs[i]) != MMC_OK) {
            MMC_LOG_ERROR("Error in free blobs!");
        }

    }
    objMeta->RemoveBlobs();
    if (objMeta->NumBlobs() == 0) {
        objMetaLookupMap_.Erase(key);
        lruList_.erase(lruItem.lruIter_);
    }
    objMeta->Unlock();
    return MMC_OK;

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
    std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs(filter);
    for (size_t i = 0; i < blobs.size(); i++) {
        blobs[i]->UpdateState(blobs[i]->Rank(), 0, MMC_REMOVE_START);
        Result ret = globalAllocator_->Free(blobs[i]);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Error in free blobs!");
        }
    }
    return objMeta->RemoveBlobs(filter);
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
        lruItem.memObjMetaPtr_->Lock();
        ret = ForceRemoveBlobs(lruItem.memObjMetaPtr_, filter);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Fail to force remove blobs in MmcMetaManager::Unmount!");
            lruItem.memObjMetaPtr_->Unlock();
            return MMC_ERROR;
        }
        if (lruItem.memObjMetaPtr_->NumBlobs() == 0) {
            tempKeys.push_back(key);
            lruList_.erase(lruItem.lruIter_);
        }
        lruItem.memObjMetaPtr_->Unlock();
    }

    for (const std::string &tempKey : tempKeys) {
        objMetaLookupMap_.Erase(tempKey);
    }

    ret = globalAllocator_->Unmount(loc);
    return ret;
}

Result MmcMetaManager::ExistKey(const std::string &key)
{
    MemObjMetaLruItem lruItem;
    Result ret = objMetaLookupMap_.Find(key, lruItem);
    if (ret != MMC_OK) {
        MMC_LOG_WARN("ExistKey cannot match an object with key : " << key);
        return MMC_UNMATCHED_KEY;
    } else {
        MmcMemObjMetaPtr objMeta = lruItem.memObjMetaPtr_;
        std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs();
        for (auto blob : blobs) {
            blob->UpdateState(0, 0, MMC_FIND_OK);
        }
        return  MMC_OK;
    }
}

Result MmcMetaManager::BatchExistKey(const std::vector<std::string> &keys, std::vector<Result> &results)
{
    Result ret = MMC_UNMATCHED_KEY;
    results.reserve(keys.size());
    for (size_t i = 0; i < keys.size(); ++i) {
        MemObjMetaLruItem lruItem;
        results.emplace_back(objMetaLookupMap_.Find(keys[i], lruItem));
        if (results.back() == MMC_OK) {
            MmcMemObjMetaPtr objMeta = lruItem.memObjMetaPtr_;
            std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs();
            for (auto blob : blobs) {
                blob->UpdateState(0, 0, MMC_FIND_OK);
            }
            ret = MMC_OK;
        }
    }
    return ret;
}

void MmcMetaManager::AsyncRemoveThreadFunc()
{
//    while (true)
//    {
//        std::unique_lock<std::mutex> lock(removeThreadLock_);
//        if (!removeThreadCv_.wait_for(lock, std::chrono::milliseconds(defaultTtlMs_), [this] {return removePredicate_;}))
//        {
//            MMC_LOG_DEBUG("async remove in thread ttl " << defaultTtlMs_ << " will remove count " << removeList_.size() <<
//                                                        " thread id " << pthread_self());
//            std::lock_guard<std::mutex> lg(removeListLock_);
//            MmcMemObjMetaPtr objMeta;
//            for (auto iter = removeList_.begin(); iter != removeList_.end();) {
//                objMeta = *iter;
//                if (!objMeta->IsLeaseExpired()) {
//                    iter++;
//                    continue;
//                }
//                std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs();
//                for (size_t i = 0; i < blobs.size(); i++) {
//                    Result ret = globalAllocator_->Free(blobs[i]);
//                    if (ret != MMC_OK) {
//                        MMC_LOG_ERROR("Error in free blobs!");
//                    }
//                }
//                objMeta->RemoveBlobs();
//                iter = removeList_.erase(iter);
//            }
//        } else {
//            MMC_LOG_DEBUG("async thread destroy, thread id " << pthread_self());
//            break;
//        }
//    }
}

Result MmcMetaManager::Query(const std::string &key, MemObjQueryInfo &queryInfo)
{
    MmcMemObjMetaPtr objMeta;
    MemObjMetaLruItem lruItem;
    if (objMetaLookupMap_.Find(key, lruItem) != MMC_OK) {
        MMC_LOG_WARN("Cannot find MmcMemObjMeta with key : " << key);
        return MMC_UNMATCHED_KEY;
    }
    objMeta = lruItem.memObjMetaPtr_;
    UpdateLRU(key, lruItem);
    queryInfo = objMeta->QueryInfo();
    return MMC_OK;
}

}  // namespace mmc
}  // namespace ock