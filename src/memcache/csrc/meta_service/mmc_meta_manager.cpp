/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#include "mmc_meta_manager.h"
#include <chrono>
#include <thread>

namespace ock {
namespace mmc {

// TODO: 当前仅考虑单副本，因此需要所有副本的状态都为ready才能完成Get
Result MmcMetaManager::Get(const std::string &key, MmcMemObjMetaPtr &objMeta)
{
    auto ret = metaContainer_->Get(key, objMeta);
    if (ret != MMC_OK) {
        MMC_LOG_DEBUG("MmcMetaManager::Get Fail! key: " << key << ". ErrCode: " << ret);
        return ret;
    }
    MmcBlobFilterPtr filter = MmcMakeRef<MmcBlobFilter>(UINT32_MAX, MEDIA_NONE, DATA_READY);
    std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs(filter);
    if (blobs.size() != objMeta->NumBlobs()) {
        MMC_LOG_DEBUG("Fail to Get! Not all blobs are ready!" << blobs.size() << "," << objMeta->NumBlobs());
        objMeta = nullptr;
        return MMC_ERROR;
    }
    metaContainer_->Promote(key);
    for (auto blob : blobs) {
        blob->UpdateState(0, 0, MMC_FIND_OK);
    }
    return MMC_OK;
}

Result MmcMetaManager::ExistKey(const std::string &key)
{
    MmcMemObjMetaPtr objMeta;
    return Get(key, objMeta);
}

Result MmcMetaManager::BatchExistKey(const std::vector<std::string> &keys, std::vector<Result> &results)
{
    results.reserve(keys.size());
    for (size_t i = 0; i < keys.size(); ++i) {
        results.emplace_back(ExistKey(keys[i]));
        if (results.back() != MMC_OK && results.back() != MMC_UNMATCHED_KEY) {
            MMC_LOG_ERROR("BatchExistKey get unexpected result: " << results.back()
                          << ", should be " << MMC_OK << " or " << MMC_UNMATCHED_KEY);
            return results.back();
        }
    }
    return MMC_OK;
}

Result MmcMetaManager::BatchGet(const std::vector<std::string> &keys, std::vector<MmcMemObjMetaPtr> &objMetas,
                                std::vector<Result> &getResults)
{
    objMetas.resize(keys.size());
    getResults.resize(keys.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        getResults[i] = Get(keys[i], objMetas[i]);
        if (getResults[i] == MMC_OK) {
            MMC_LOG_DEBUG("Key " << keys[i] << " found successfully");
        } else {
            objMetas[i] = nullptr;
            MMC_LOG_INFO("Key " << keys[i] << " not found");
        }
    }

    for (const Result &r : getResults) {
        if (r != MMC_OK) {
            return r;
        }
    }
    return MMC_OK;
}

Result MmcMetaManager::Alloc(const std::string &key, const AllocOptions &allocOpt, uint32_t requestId,
                             MmcMemObjMetaPtr &objMeta)
{
    // TODO: 不能阻塞
    if (globalAllocator_->TouchedThreshold(EVICT_THRESHOLD_HIGH)) {
        std::vector<std::string> keys = metaContainer_->EvictCandidates(EVICT_THRESHOLD_LOW);
        std::vector<Result> remove_results;
        BatchRemove(keys, remove_results);
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

    metaContainer_->Insert(key, objMeta);
    return MMC_OK;
}

Result MmcMetaManager::BatchAlloc(const std::vector<std::string>& keys,
                                  const std::vector<AllocOptions>& allocOpts,
                                  uint32_t requestId,
                                  std::vector<MmcMemObjMetaPtr>& objMetas,
                                  std::vector<Result>& allocResults)
{
    if (globalAllocator_->TouchedThreshold(EVICT_THRESHOLD_HIGH)) {
        std::vector<std::string> keysToEvict = metaContainer_->EvictCandidates(EVICT_THRESHOLD_LOW);
        std::vector<Result> removeResults;
        BatchRemove(keysToEvict, removeResults);
    }

    objMetas.resize(keys.size(), nullptr);
    allocResults.resize(keys.size(), MMC_ERROR);

    bool allSuccess = true;
    for (size_t i = 0; i < keys.size(); ++i) {
        const std::string& key = keys[i];
        const AllocOptions& opt = allocOpts[i];

        MmcMemObjMetaPtr objMeta = MmcMakeRef<MmcMemObjMeta>();
        std::vector<MmcMemBlobPtr> blobs;

        Result ret = globalAllocator_->Alloc(opt, blobs);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Allocation failed for key: " << key << ", error: " << ret);
            allocResults[i] = ret;
            allSuccess = false;
            continue;
        }

        for (auto& blob : blobs) {
            blob->UpdateState(blob->Rank(), requestId, MMC_ALLOCATED_OK);
            objMeta->AddBlob(blob);
        }

        metaContainer_->Insert(key, objMeta);
        objMetas[i] = objMeta;
        allocResults[i] = MMC_OK;
        MMC_LOG_DEBUG("Allocated for key: " << key);
    }
    
    return allSuccess ? MMC_OK : MMC_ERROR;
}

Result MmcMetaManager::UpdateState(const std::string &key, const MmcLocation &loc, uint32_t rankId, uint32_t operateId,
                                   const BlobActionResult &actRet)
{
    MmcMemObjMetaPtr metaObj;
    // when update state, do not update the lru
    Result ret = metaContainer_->Get(key, metaObj);
    if (ret != MMC_OK) {
        MMC_LOG_DEBUG("UpdateState: Cannot find memObjMeta!");
        return MMC_UNMATCHED_KEY;
    }

    MmcBlobFilterPtr filter = MmcMakeRef<MmcBlobFilter>(loc.rank_, loc.mediaType_, NONE);
    MMC_ASSERT(filter != nullptr);
    std::vector<MmcMemBlobPtr> blobs = metaObj->GetBlobs(filter);

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
    MmcMemObjMetaPtr objMeta;
    Result ret = metaContainer_->Get(key, objMeta);
    if (ret != MMC_OK) {
        MMC_LOG_DEBUG("UpdateState: Cannot find memObjMeta!");
        return MMC_UNMATCHED_KEY;
    }
    objMeta->Lock();
    if (objMeta->NumBlobs() == 0) {
        objMeta->Unlock();
        return MMC_OK;
    }
    std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs();
    for (size_t i = 0; i < blobs.size(); i++) {
        MMC_RETURN_ERROR(blobs[i]->UpdateState(0, 0, MMC_REMOVE_START),
                         "remove op, meta update failed, key " << key);
        if (globalAllocator_->Free(blobs[i]) != MMC_OK) {
            MMC_LOG_ERROR("Error in free blobs!");
        }
    }
    objMeta->RemoveBlobs();
    if (objMeta->NumBlobs() == 0) {
        ret = metaContainer_->Erase(key);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("UpdateState: Fail to erase from container!");
            return MMC_ERROR;
        }
    }
    objMeta->Unlock();
    return MMC_OK;
}

Result MmcMetaManager::BatchRemove(const std::vector<std::string> &keys, std::vector<Result> &results)
{
    results.reserve(keys.size());
    for (const std::string &key : keys) {
        results.emplace_back(Remove(key));
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

    auto it = metaContainer_->Begin();
    auto end = metaContainer_->End();
    while (*it != *end) {
        auto kv = **it;
        std::string key = kv.first;
        MmcMemObjMetaPtr objMeta = kv.second;
        objMeta->Lock();
        ret = ForceRemoveBlobs(objMeta, filter);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Fail to force remove blobs in MmcMetaManager::Unmount!");
            objMeta->Unlock();
            return MMC_ERROR;
        }
        if (objMeta->NumBlobs() == 0) {
            tempKeys.push_back(key);
        }
        objMeta->Unlock();
        ++(*it);
    }

    for (const std::string &tempKey : tempKeys) {
        metaContainer_->Erase(tempKey);
    }

    ret = globalAllocator_->Unmount(loc);
    return ret;
}

void MmcMetaManager::AsyncRemoveThreadFunc()
{
    //    while (true)
    //    {
    //        std::unique_lock<std::mutex> lock(removeThreadLock_);
    //        if (!removeThreadCv_.wait_for(lock, std::chrono::milliseconds(defaultTtlMs_), [this] {return
    //        removePredicate_;}))
    //        {
    //            MMC_LOG_DEBUG("async remove in thread ttl " << defaultTtlMs_ << " will remove count " <<
    //            removeList_.size() <<
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
    if (metaContainer_->Get(key, objMeta) != MMC_OK) {
        MMC_LOG_WARN("Cannot find MmcMemObjMeta with key : " << key);
        return MMC_UNMATCHED_KEY;
    }
    queryInfo = objMeta->QueryInfo();
    return MMC_OK;
}

}  // namespace mmc
}  // namespace ock