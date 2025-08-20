/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#include "mmc_meta_manager.h"
#include <chrono>
#include <thread>

namespace ock {
namespace mmc {

Result MmcMetaManager::Get(const std::string& key, uint64_t operateId, MmcBlobFilterPtr filterPtr,
                           MmcMemMetaDesc& objMeta)
{
    MmcMemObjMetaPtr memObj;
    auto ret = metaContainer_->Get(key, memObj);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("Get key: " << key << " failed. ErrCode: " << ret);
        return ret;
    }
    metaContainer_->Promote(key);

    std::unique_lock<std::mutex> guard(metaItemMtxs_[GetIndex(memObj)]);
    objMeta.prot_ = memObj->Prot();
    objMeta.priority_ = memObj->Priority();
    objMeta.size_ = memObj->Size();

    std::vector<MmcMemBlobPtr> blobs = memObj->GetBlobs(filterPtr);
    for (auto blob : blobs) {
        uint32_t opRankId = GetRankIdByOperateId(operateId);
        uint32_t opSeq = GetSequenceByOperateId(operateId);
        auto ret = blob->UpdateState(key, opRankId, opSeq, MMC_READ_START);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("update key " << key << " blob state failed with error: " << ret);
            continue;
        }
        objMeta.blobs_.push_back(blob->GetDesc());
        break;  // 只需返回一个
    }
    objMeta.numBlobs_ = objMeta.blobs_.size();
    return MMC_OK;
}

Result MmcMetaManager::ExistKey(const std::string& key)
{
    MmcMemObjMetaPtr memObj;
    return metaContainer_->Get(key, memObj);
}

void MmcMetaManager::CheckAndEvict(MetaNetServerPtr metaNetServer)
{
    if (globalAllocator_->GetUsageRate() < (uint64_t)evictThresholdHigh_) {
        return;
    }

    auto moveFunc = [&](const std::string& key, const MmcMemObjMetaPtr& objMeta) -> bool {
        std::unique_lock<std::mutex> guard(metaItemMtxs_[GetIndex(objMeta)]);

        MediaType type = objMeta->MoveTo();
        if (type == MediaType::MEDIA_NONE) {
            PushRemoveList(key, objMeta);
            return true;  // 向下淘汰已无可能，直接删除
        }
        MmcLocation src{UINT32_MAX, MEDIA_NONE};
        MmcLocation dst{UINT32_MAX, type};
        auto ret = MoveBlob(metaNetServer, key, src, dst);
        if (ret != MMC_OK) {
            MMC_LOG_WARN("key: " << key << " move blob from " << src << " to " << dst << " failed, ret " << ret);
            PushRemoveList(key, objMeta);
            return true;  // 向下淘汰失败，直接删除
        }
        return false;
    };

    metaContainer_->MultiLevelElimination(evictThresholdHigh_, evictThresholdLow_, moveFunc);
}

Result MmcMetaManager::Alloc(const std::string &key, const AllocOptions &allocOpt, uint64_t operateId,
                             MmcMemMetaDesc& objMeta)
{
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
        if (ret != MMC_DUPLICATED_OBJECT) {
            MMC_LOG_ERROR("Fail to insert " << key << " into MmcMetaContainer. ret:" << ret);
        }
    } else {
        std::unique_lock<std::mutex> guard(metaItemMtxs_[GetIndex(tempMetaObj)]);
        objMeta.prot_ = tempMetaObj->Prot();
        objMeta.priority_ = tempMetaObj->Priority();
        objMeta.size_ = tempMetaObj->Size();
        tempMetaObj->GetBlobsDesc(objMeta.blobs_);
        objMeta.numBlobs_ = objMeta.blobs_.size();
    }
    return ret;
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
    std::unique_lock<std::mutex> guard(metaItemMtxs_[GetIndex(metaObj)]);
    return metaObj->UpdateBlobsState(key, filter, operateId, actRet);
}

void MmcMetaManager::PushRemoveList(const std::string& key, const MmcMemObjMetaPtr& meta)
{
    {
        std::lock_guard<std::mutex> lg(removeListLock_);
        removeList_.push_back({key, meta});
    }
    {
        std::lock_guard<std::mutex> lk(removeThreadLock_);
        removeThreadCv_.notify_all();
    }
}

Result MmcMetaManager::Remove(const std::string &key)
{
    MmcMemObjMetaPtr objMeta;
    MMC_RETURN_ERROR(metaContainer_->Erase(key, objMeta), "remove: Fail to erase from container!");
    PushRemoveList(key, objMeta);
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
    if (blobMap.empty()) {
        return globalAllocator_->Start(loc);
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

Result MmcMetaManager::Mount(const std::vector<MmcLocation>& locs,
                             const std::vector<MmcLocalMemlInitInfo>& localMemInitInfos,
                             std::map<std::string, MmcMemBlobDesc>& blobMap)
{
    if (locs.size() != localMemInitInfos.size()) {
        MMC_LOG_ERROR("Mount: loc size:" << locs.size() << " != localMemInitInfo size:" << localMemInitInfos.size());
        return MMC_INVALID_PARAM;
    }

    for (uint32_t i = 0; i < locs.size(); i++) {
        Result ret = Mount(locs[i], localMemInitInfos[i], blobMap);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Mount failed ret:" << ret << " loc rank:" << locs[i].rank_
                                              << " mediaType_: " << locs[i].mediaType_);
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
            std::unique_lock<std::mutex> guard(metaItemMtxs_[GetIndex(objMeta)]);
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
            std::unique_lock<std::mutex> guard(metaItemMtxs_[GetIndex(objMeta)]);
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

    auto matchFunc = [&](const std::string& key, const MmcMemObjMetaPtr& objMeta) -> bool {
        std::unique_lock<std::mutex> guard(metaItemMtxs_[GetIndex(objMeta)]);
        auto ret = objMeta->FreeBlobs(key, globalAllocator_, filter, false);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Fail to force remove key:" << key << " blobs in when unmount! ret:" << ret);
            return false;
        }
        if (objMeta->NumBlobs() == 0) {
            return true;
        }
        return false;
    };

    metaContainer_->EraseIf(matchFunc);

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
                MMC_LOG_WARN("async thread destroy, thread id " << pthread_self());
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
            MMC_LOG_DEBUG("allocator usage rate: " << globalAllocator_->GetUsageRate());
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
    std::unique_lock<std::mutex> guard(metaItemMtxs_[GetIndex(objMeta)]);
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

Result MmcMetaManager::CopyBlob(MetaNetServerPtr metaNetServer, const MmcMemObjMetaPtr& objMeta,
                                const MmcMemBlobDesc& srcBlob, const MmcLocation& dstLoc)
{
    AllocOptions allocOpt{};
    allocOpt.blobSize_ = srcBlob.size_;
    allocOpt.numBlobs_ = 1;
    allocOpt.mediaType_ = dstLoc.mediaType_;
    allocOpt.preferredRank_ = dstLoc.rank_;
    allocOpt.flags_ = dstLoc.rank_ == UINT32_MAX ? 0 : ALLOC_FORCE_BY_RANK;

    std::vector<MmcMemBlobPtr> blobs;
    Result ret = MMC_OK;
    do {
        ret = globalAllocator_->Alloc(allocOpt, blobs);
        if (ret != MMC_OK || blobs.empty()) {
            MMC_LOG_ERROR("alloc failed, ret " << ret);
            ret = MMC_MALLOC_FAILED;
            break;
        }

        BlobCopyRequest request{srcBlob, blobs[0]->GetDesc()};
        Response response;
        // rpc 到目标节点复制
        ret = metaNetServer->SyncCall(request.dstBlob_.rank_, request, response, 60);
        if (ret != MMC_OK || response.ret_ != MMC_OK) {
            MMC_LOG_ERROR("copy blob from rank " << request.srcBlob_.rank_ << " to rank " << request.dstBlob_.rank_
                                                 << " failed:" << ret << "," << response.ret_);
            ret = MMC_ERROR;
            break;
        }
        // 挂载
        ret = objMeta->AddBlob(blobs[0]);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("AddBlob failed, ret " << ret);
            break;
        }
    } while (0);

    if (ret != MMC_OK && !blobs.empty()) {
        for (auto& blob : blobs) {
            globalAllocator_->Free(blob);
        }
    }
    return ret;
}

Result MmcMetaManager::MoveBlob(MetaNetServerPtr metaNetServer, const std::string& key, const MmcLocation& src,
                                const MmcLocation& dst)
{
    MmcMemObjMetaPtr objMeta;
    if (metaContainer_->Get(key, objMeta) != MMC_OK) {
        MMC_LOG_ERROR("Cannot find MmcMemObjMeta with key : " << key);
        return MMC_UNMATCHED_KEY;
    }
    std::unique_lock<std::mutex> guard(metaItemMtxs_[GetIndex(objMeta)]);
    std::vector<MmcMemBlobDesc> blobsDesc;
    MmcBlobFilterPtr filter = MmcMakeRef<MmcBlobFilter>(src.rank_, src.mediaType_, READABLE);
    objMeta->GetBlobsDesc(blobsDesc, filter);
    if (blobsDesc.empty()) {
        MMC_LOG_ERROR("blob is empty with key : " << key);
        return MMC_UNMATCHED_KEY;
    }

    auto ret = CopyBlob(metaNetServer, objMeta, blobsDesc[0], dst);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("key: " << key << " copy blob failed, ret " << ret);
        return ret;
    }

    filter = MmcMakeRef<MmcBlobFilter>(src.rank_, src.mediaType_, NONE);
    ret = objMeta->FreeBlobs(key, globalAllocator_, filter);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("key: " << key << " free blob failed, ret " << ret);
    }
    return MMC_OK;
}

Result MmcMetaManager::ReplicateBlob(MetaNetServerPtr metaNetServer, const std::string& key, const MmcLocation& loc)
{
    MmcMemObjMetaPtr objMeta;
    if (metaContainer_->Get(key, objMeta) != MMC_OK) {
        MMC_LOG_ERROR("Cannot find MmcMemObjMeta with key : " << key);
        return MMC_UNMATCHED_KEY;
    }
    std::unique_lock<std::mutex> guard(metaItemMtxs_[GetIndex(objMeta)]);
    std::vector<MmcMemBlobDesc> blobsDesc;
    MmcBlobFilterPtr filter = MmcMakeRef<MmcBlobFilter>(UINT32_MAX, MEDIA_NONE, READABLE);
    objMeta->GetBlobsDesc(blobsDesc, filter);
    if (blobsDesc.empty()) {
        MMC_LOG_ERROR("blob is empty with key : " << key);
        return MMC_UNMATCHED_KEY;
    }

    return CopyBlob(metaNetServer, objMeta, blobsDesc[0], loc);
}

}  // namespace mmc
}  // namespace ock