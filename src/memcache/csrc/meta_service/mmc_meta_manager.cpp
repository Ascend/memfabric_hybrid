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
    auto ret = metaContainer_->Get(key, objMeta);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("Get key: " << key << " failed. ErrCode: " << ret);
        objMeta = nullptr;
        return ret;
    }
    metaContainer_->Promote(key);
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
        auto ret = ExistKey(keys[i]);
        results.emplace_back(ret);
        if (ret != MMC_OK && ret != MMC_UNMATCHED_KEY) {
            MMC_LOG_ERROR("get unexpected result: " << ret << ", should be " << MMC_OK << " or " << MMC_UNMATCHED_KEY);
            continue;
        }
    }
    return MMC_OK;
}

Result MmcMetaManager::BatchGet(const std::vector<std::string>& keys, std::vector<MmcMemObjMetaPtr>& objMetas,
                                std::vector<Result>& getResults)
{
    objMetas.resize(keys.size());
    getResults.resize(keys.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        getResults[i] = Get(keys[i], objMetas[i]);
        if (getResults[i] != MMC_OK) {
            objMetas[i] = nullptr;
            MMC_LOG_ERROR("Key " << keys[i] << " not found");
        }
    }
    return MMC_OK;
}

void MmcMetaManager::CheckAndEvict()
{
    if (globalAllocator_->GetUsageRate() >= (uint64_t)evictThresholdHigh_) {
        std::vector<std::string> keys = metaContainer_->EvictCandidates(evictThresholdHigh_, evictThresholdLow_);
        std::vector<Result> remove_results;
        BatchRemove(keys, remove_results);
    }
}

Result MmcMetaManager::Alloc(const std::string &key, const AllocOptions &allocOpt, uint64_t operateId,
                             MmcMemObjMetaPtr &objMeta)
{
    CheckAndEvict();

    MmcMemObjMetaPtr tempMetaObj = MmcMakeRef<MmcMemObjMeta>();
    std::vector<MmcMemBlobPtr> blobs;

    Result ret = globalAllocator_->Alloc(allocOpt, blobs);
    if (ret != MMC_OK) {
        if (!blobs.empty()) {
            for (auto& blob : blobs) {
                globalAllocator_->Free(blob);
            }
            blobs.clear();
        }
        MMC_LOG_ERROR("Alloc " << allocOpt.blobSize_ << "failed, ret:" << ret);
        return ret;
    }

    uint32_t opRankId = GetRankIdByOperateId(operateId);
    uint32_t opSeq = GetSequenceByOperateId(operateId);
    for (auto& blob : blobs) {
        MMC_LOG_DEBUG("Blob allocated, key=" << key << ", size=" << blob->Size() << ", rank=" << blob->Rank());
        blob->UpdateState(key, opRankId, opSeq, MMC_ALLOCATED_OK);
        tempMetaObj->AddBlob(blob);
    }

    ret = metaContainer_->Insert(key, tempMetaObj);
    if (ret != MMC_OK) {
        tempMetaObj->FreeBlobs(key, globalAllocator_);
        MMC_LOG_ERROR("Fail to insert " << key << " into MmcMetaContainer. ret:" << ret);
    } else {
        objMeta = tempMetaObj;
    }
    return ret;
}

Result MmcMetaManager::BatchAlloc(const std::vector<std::string>& keys,
                                  const std::vector<AllocOptions>& allocOpts,
                                  uint64_t operateId,
                                  std::vector<MmcMemObjMetaPtr>& objMetas,
                                  std::vector<Result>& allocResults)
{
    CheckAndEvict();

    objMetas.resize(keys.size(), nullptr);
    allocResults.resize(keys.size(), MMC_ERROR);

    for (size_t i = 0; i < keys.size(); ++i) {
        const std::string& key = keys[i];
        const AllocOptions& opt = allocOpts[i];

        MmcMemObjMetaPtr objMeta = nullptr;
        auto ret = Alloc(key, opt, operateId, objMeta);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Allocation failed for key: " << key << ", reqId:" << operateId << ", error: " << ret);
            allocResults[i] = ret;
            continue;
        }
        objMetas[i] = objMeta;
        allocResults[i] = MMC_OK;
    }
    // 此处不返回失败，因为业务的结果由 allocResults 带回，将此原则推广到有结果返回值的所有请求
    return MMC_OK;
}

// todo update 方法没有兼容多blob情况
Result MmcMetaManager::UpdateState(const std::string& key, const MmcLocation& loc, const BlobActionResult& actRet,
                                   uint64_t operateId)
{
    MmcMemObjMetaPtr metaObj;
    // when update state, do not update the lru
    Result ret = metaContainer_->Get(key, metaObj);
    if (ret != MMC_OK || metaObj == nullptr) {
        MMC_LOG_ERROR("UpdateState: Cannot find " << key << " memObjMeta! ret:" << ret);
        return MMC_UNMATCHED_KEY;
    }
    MmcBlobFilterPtr filter = MmcMakeRef<MmcBlobFilter>(loc.rank_, loc.mediaType_, NONE);
    return metaObj->UpdateBlobsState(key, filter, operateId, actRet);
}

Result MmcMetaManager::BatchUpdateState(const std::vector<std::string>& keys, const std::vector<MmcLocation>& locs,
                                        const std::vector<BlobActionResult>& actRets, uint64_t operateId,
                                        std::vector<Result>& updateResults)
{
    const size_t count = keys.size();
    if (count != locs.size() || count != actRets.size()) {
        MMC_LOG_ERROR("BatchUpdateState: Input vectors size mismatch {keyNum:"
                      << keys.size() << ", locNum:" << locs.size() << ", actRetNum:" << actRets.size() << "}");
        return MMC_ERROR;
    }
    updateResults.clear();
    updateResults.resize(count, MMC_ERROR);
    for (size_t i = 0; i < count; ++i) {
        updateResults[i] = UpdateState(keys[i], locs[i], actRets[i], operateId);
    }
    return MMC_OK;
}

Result MmcMetaManager::Remove(const std::string &key)
{
    MmcMemObjMetaPtr objMeta;
    MMC_RETURN_ERROR(metaContainer_->Erase(key, objMeta), "remove: Fail to erase from container!");
    {
        std::lock_guard<std::mutex> lg(removeListLock_);
        removeList_.push_back({key, objMeta});
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

Result MmcMetaManager::Mount(const MmcLocation &loc, const MmcLocalMemlInitInfo &localMemInitInfo,
    std::map<std::string, MmcMemBlobDesc> &blobMap)
{
    Result ret = globalAllocator_->Mount(loc, localMemInitInfo);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("allocator mount failed, loc rank: " << loc.rank_ << " mediaType_: " << loc.mediaType_);
        return ret;
    }

    ret = globalAllocator_->BuildFromBlobs(loc, blobMap);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("build from blobs failed, loc rank: " << loc.rank_ << " mediaType_: " << loc.mediaType_);
        return ret;
    }

    if (!blobMap.empty()) {
        ret = RebuildMeta(blobMap);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("rebuild meta failed, loc rank: " << loc.rank_ << " mediaType_: " << loc.mediaType_);
            return ret;
        }
    }
    return MMC_OK;
}

Result MmcMetaManager::RebuildMeta(std::map<std::string, MmcMemBlobDesc> &blobMap)
{
    Result ret;
    for (auto &blob : blobMap) {
        std::string key = blob.first;
        MmcMemBlobDesc desc = blob.second;
        MmcMemBlobPtr blobPtr = MmcMakeRef<MmcMemBlob>(desc.rank_, desc.gva_, desc.size_,
            static_cast<MediaType>(desc.mediaType_), READABLE);
        MmcMemObjMetaPtr objMeta;

        if (metaContainer_->Get(key, objMeta) == MMC_OK) {
            if (objMeta->AddBlob(blobPtr) != MMC_OK) {
                globalAllocator_->Free(blobPtr);
            }
            continue;
        }

        objMeta = MmcMakeRef<MmcMemObjMeta>();
        objMeta->AddBlob(blobPtr);
        ret = metaContainer_->Insert(key, objMeta);
        if (ret == MMC_OK) {
            continue;
        }

        if (metaContainer_->Get(key, objMeta) == MMC_OK) {
            if (objMeta->AddBlob(blobPtr) != MMC_OK) {
                globalAllocator_->Free(blobPtr);
            }
        } else {
            globalAllocator_->Free(blobPtr);
        }
    }
    return MMC_OK;
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
        ret = objMeta->FreeBlobs(key, globalAllocator_, filter);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Fail to force remove key:" << key << " blobs in when unmount!");
            return MMC_ERROR;
        }
        if (objMeta->NumBlobs() == 0) {
            tempKeys.push_back(key);
        }
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
                (*iter).second->FreeBlobs((*iter).first, globalAllocator_);
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

    std::vector<MmcMemBlobDesc> blobs;
    objMeta->GetBlobsDesc(blobs);
    uint32_t i = 0;
    for (auto blob : blobs) {
        if (i >= MAX_BLOB_COPIES) {
            break;
        }
        queryInfo.blobRanks_[i] = blob.rank_;
        queryInfo.blobTypes_[i] = blob.mediaType_;
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