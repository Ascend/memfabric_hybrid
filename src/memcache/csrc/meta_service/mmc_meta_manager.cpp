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
        MMC_LOG_ERROR("MmcMetaManager::Get Fail! key: " << key << ". ErrCode: " << ret);
        return ret;
    }
    MmcBlobFilterPtr filter = MmcMakeRef<MmcBlobFilter>(UINT32_MAX, MEDIA_NONE, DATA_READY);
    std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs(filter);
    if (blobs.size() != objMeta->NumBlobs()) {
        MMC_LOG_ERROR("Fail to Get! Not all blobs are ready!" << blobs.size() << "," << objMeta->NumBlobs());
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
    if (globalAllocator_->GetUsageRate() >= (uint64_t)evictThresholdHigh_) {
        std::vector<std::string> keys = metaContainer_->EvictCandidates(evictThresholdHigh_, evictThresholdLow_);
        std::vector<Result> remove_results;
        BatchRemove(keys, remove_results);
    }

    objMeta = MmcMakeRef<MmcMemObjMeta>();
    std::vector<MmcMemBlobPtr> blobs;

    Result ret = globalAllocator_->Alloc(allocOpt, blobs);
    if (ret == MMC_ERROR) {
        if (!blobs.empty()) {
            for (auto &blob : blobs) {
                globalAllocator_->Free(blob);
            }
            blobs.clear();
        }
        return MMC_ERROR;
    }

    for (auto &blob : blobs) {
        MMC_LOG_DEBUG("Blob allocated, key=" << key << ", size=" << blob->Size() << ", rank=" << blob->Rank());
        blob->UpdateState(blob->Rank(), requestId, MMC_ALLOCATED_OK);
        objMeta->AddBlob(blob);
    }

    ret = metaContainer_->Insert(key, objMeta);
    if (ret != MMC_OK) {
        objMeta->FreeBlobs(globalAllocator_);
        MMC_LOG_ERROR("Fail to insert " << key << " into MmcMetaContainer. ret:" << ret);
    }
    return ret;
}

Result MmcMetaManager::BatchAlloc(const std::vector<std::string>& keys,
                                  const std::vector<AllocOptions>& allocOpts,
                                  uint32_t requestId,
                                  std::vector<MmcMemObjMetaPtr>& objMetas,
                                  std::vector<Result>& allocResults)
{
    if (globalAllocator_->GetUsageRate() >= (uint64_t)evictThresholdHigh_) {
        std::vector<std::string> keysToEvict = metaContainer_->EvictCandidates(evictThresholdHigh_, evictThresholdLow_);
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
            MMC_LOG_DEBUG("Blob allocated, key=" << key << ", size=" << blob->Size() << ", rank=" << blob->Rank());
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
        MMC_LOG_ERROR("One blob is expected, actual number: " << blobs.size());
        return MMC_ERROR;
    }
    ret = blobs[0]->UpdateState(rankId, operateId, actRet);
    if (ret != MMC_OK) {
        MMC_LOG_WARN("UpdateState Fail!");
        return ret;
    }
    return MMC_OK;
}

Result MmcMetaManager::BatchUpdateState(const std::vector<std::string> &keys,
                                        const std::vector<MmcLocation> &locs,
                                        const std::vector<uint32_t> &rankIds,
                                        const std::vector<uint32_t> &operateIds,
                                        const std::vector<BlobActionResult> &actRets,
                                        std::vector<Result> &updateResults)
{
    const size_t count = keys.size();
    if (count != locs.size() || count != rankIds.size() || count != operateIds.size() || count != actRets.size()) {
        MMC_LOG_ERROR("BatchUpdateState: Input vectors size mismatch.");
        return MMC_ERROR;
    }
    updateResults.clear();
    updateResults.resize(count, MMC_ERROR);
    std::vector<MmcMemObjMetaPtr> objMetas(count);
    std::vector<Result> getResults(count);
    for (size_t i = 0; i < count; ++i) {
        getResults[i] = metaContainer_->Get(keys[i], objMetas[i]);
    }
    bool allSuccess = true;
    for (size_t i = 0; i < count; ++i) {
        const std::string& key = keys[i];
        if (getResults[i] != MMC_OK) {
            updateResults[i] = (getResults[i] == MMC_UNMATCHED_KEY) ?
                MMC_UNMATCHED_KEY : MMC_ERROR;
            allSuccess = false;
            MMC_LOG_DEBUG("Key not found: " << key << " (code: " << getResults[i] << ")");
            continue;
        }

        objMetas[i]->Lock();
        MmcBlobFilterPtr filter = MmcMakeRef<MmcBlobFilter>(
            locs[i].rank_, locs[i].mediaType_, NONE);
        if (filter == nullptr) {
            updateResults[i] = MMC_ERROR;
            allSuccess = false;
            objMetas[i]->Unlock();
            MMC_LOG_ERROR("Filter creation failed for key: " << key);
            continue;
        }

        std::vector<MmcMemBlobPtr> blobs = objMetas[i]->GetBlobs(filter);
        if (blobs.size() != 1) {
            updateResults[i] = MMC_ERROR;
            allSuccess = false;
            objMetas[i]->Unlock();
            MMC_LOG_ERROR("Invalid blob count for key: " << key
                          << ". Expected 1, found " << blobs.size());
            continue;
        }
        updateResults[i] = blobs[0]->UpdateState(
            rankIds[i], operateIds[i], actRets[i]);
        objMetas[i]->Unlock();

        if (updateResults[i] != MMC_OK) {
            allSuccess = false;
            MMC_LOG_WARN("Update failed for key: " << key << " (code: " << updateResults[i] << ")");
        } else {
            MMC_LOG_DEBUG("Updated key: " << key << ", Rank: " << rankIds[i]);
        }
    }
    return allSuccess ? MMC_OK : MMC_ERROR;
}

// TODO: 遍历的时候有问题
Result MmcMetaManager::Remove(const std::string &key)
{
    MmcMemObjMetaPtr objMeta;
    MMC_RETURN_ERROR(metaContainer_->Get(key, objMeta), "remove: Cannot find memObjMeta!");
    MMC_RETURN_ERROR(metaContainer_->Erase(key), "remove: Fail to erase from container!");
    {
        std::lock_guard<std::mutex> lg(removeListLock_);
        removeList_.push_back(objMeta);
    }
    {
        std::lock_guard<std::mutex> lk(removeThreadLock_);
        removeThreadCv_.notify_all();
    }
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
            MMC_LOG_ERROR("Fail to force remove key:" << key << " blobs in when unmount!");
            objMeta->Unlock();
            return MMC_ERROR;
        }
        if (objMeta->NumBlobs() == 0) {
            tempKeys.push_back(key);
        }
        objMeta->Unlock();
        ++(*it);
    }

    for (const std::string& tempKey : tempKeys) {
        metaContainer_->Erase(tempKey);
        MMC_LOG_INFO("Unmount {rank:" << loc.rank_ << ", type:" << loc.mediaType_ << "} key:" << tempKey);
    }

    ret = globalAllocator_->Unmount(loc);
    return ret;
}

void MmcMetaManager::AsyncRemoveThreadFunc()
{
    auto lastCheckTime = std::chrono::steady_clock::now();
    constexpr auto checkInterval = std::chrono::seconds(MMC_THRESHOLD_PRINT_SECONDS);
    while (true) {
        std::unique_lock<std::mutex> lock(removeThreadLock_);
        if (removeThreadCv_.wait_for(lock, std::chrono::milliseconds(defaultTtlMs_),
                                     [this] {return removeList_.size() || !started_;})) {
            if (!started_) {
                MMC_LOG_INFO("async thread destroy, thread id " << pthread_self());
                break;
            }
            MMC_LOG_DEBUG("async remove in thread ttl " << defaultTtlMs_ << " will remove count " <<
            removeList_.size() << " thread id " << pthread_self());
            std::lock_guard<std::mutex> lg(removeListLock_);
            MmcMemObjMetaPtr objMeta;
            for (auto iter = removeList_.begin(); iter != removeList_.end();) {
                (*iter)->FreeBlobs(globalAllocator_);
                iter = removeList_.erase(iter);
            }
        }

        auto now = std::chrono::steady_clock::now();
        if (now - lastCheckTime >= checkInterval) {
            MMC_LOG_INFO("allocator usage rate: " << globalAllocator_->GetUsageRate());

            lastCheckTime = now;
        }
    }
}

Result MmcMetaManager::Query(const std::string &key, MemObjQueryInfo &queryInfo)
{
    MmcMemObjMetaPtr objMeta;
    if (metaContainer_->Get(key, objMeta) != MMC_OK) {
        MMC_LOG_WARN("Cannot find MmcMemObjMeta with key : " << key);
        return MMC_UNMATCHED_KEY;
    }

    std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs();
    uint32_t i = 0;
    for (auto blob : blobs) {
        if (i >= MAX_BLOB_COPIES) {
            break;
        }
        queryInfo.blobRanks_[i] = blob->Rank();
        queryInfo.blobTypes_[i] = blob->Type();
        i++;
    }
    queryInfo.numBlobs_ = i;
    queryInfo.size_ = objMeta->Size();
    queryInfo.prot_ = objMeta->Prot();
    queryInfo.valid_ = true;
    return MMC_OK;
}

}  // namespace mmc
}  // namespace ock