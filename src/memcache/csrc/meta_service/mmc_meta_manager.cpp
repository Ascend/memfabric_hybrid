/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "mmc_meta_manager.h"

#include "mmc_logger.h"
#include "mmc_types.h"

namespace ock {
namespace mmc {

constexpr int TIMEOUT_SECOND = 60;

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
    auto ret = metaContainer_->Get(key, memObj);
    if (ret != MMC_OK) {
        MMC_LOG_INFO("Failed to get key:" << key << " ret:" << ret);
        return ret;
    }
    {
        std::unique_lock<std::mutex> guard(metaItemMtxs_[GetIndex(memObj)]);
        MmcBlobFilterPtr filterPtr = MmcMakeRef<MmcBlobFilter>(UINT32_MAX, MEDIA_NONE, READABLE);
        if (filterPtr == nullptr) {
            MMC_LOG_ERROR("Failed to alloc filter");
            return MMC_MALLOC_FAILED;
        }
        std::vector<MmcMemBlobPtr> blobs = memObj->GetBlobs(filterPtr);
        if (blobs.empty()) {
            MMC_LOG_ERROR("Key is exist but do not have readable blob key:" << key);
            return MMC_OBJECT_NOT_EXISTS;
        }
    }
    metaContainer_->Promote(key);
    return MMC_OK;
}

void MmcMetaManager::CheckAndEvict()
{
    const auto needEvictList = globalAllocator_->GetNeedEvictList(evictThresholdHigh_);
    if (needEvictList.empty()) {
        return;
    }
    bool expected = false;
    if (!evictCheck_.compare_exchange_strong(expected, true)) {
        return;
    }
    auto moveFunc = [this](const std::string& key, const MmcMemObjMetaPtr& objMeta) -> EvictResult {
        return this->EvictCallBackFunction(key, objMeta);
    };

    auto evictFuture = threadPool_->Enqueue(
        [&](
            const std::vector<MediaType> &needEvictListL,
            const std::function<EvictResult(const std::string& key, const MmcMemObjMetaPtr& objMeta)> &moveFuncL
            ) {
            metaContainer_->MultiLevelElimination(evictThresholdHigh_, evictThresholdLow_, needEvictListL, moveFuncL);
            bool expected = true;
            evictCheck_.compare_exchange_strong(expected, false);
        },
        needEvictList,
        moveFunc);
    if (!evictFuture.valid()) {
        MMC_LOG_ERROR("submit evict task failed");
    }
}

Result MmcMetaManager::Alloc(const std::string &key, const AllocOptions &allocOpt, uint64_t operateId,
                             MmcMemMetaDesc& objMeta)
{
    MmcMemObjMetaPtr tempMetaObj = MmcMakeRef<MmcMemObjMeta>();
    if (tempMetaObj == nullptr) {
        MMC_LOG_ERROR("Fail to malloc tempMetaObj");
        return MMC_MALLOC_FAILED;
    }
    std::vector<MmcMemBlobPtr> blobs;

    Result ret = globalAllocator_->Alloc(allocOpt, blobs);
    if (ret != MMC_OK) {
        if (!blobs.empty()) {
            for (auto& blob : blobs) {
                globalAllocator_->Free(blob);
            }
            blobs.clear();
        }
        MMC_LOG_ERROR("Alloc " << allocOpt.blobSize_ << " failed, ret:" << ret);
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
        tempMetaObj->FreeBlobs(key, globalAllocator_, nullptr, false);
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

Result MmcMetaManager::UpdateState(const std::string& key, const MmcLocation& loc, const BlobActionResult& actRet,
                                   uint64_t operateId)
{
    MmcMemObjMetaPtr metaObj;
    // when update state, do not update the lru
    Result ret = metaContainer_->Get(key, metaObj);
    if (ret != MMC_OK || metaObj == nullptr) {
        MMC_LOG_ERROR("UpdateState: Cannot find " << key << " memObjMeta! ret:" << ret
                                                  << ", action:" << static_cast<uint32_t>(actRet));
        return MMC_UNMATCHED_KEY;
    }
    MmcBlobFilterPtr filter = MmcMakeRef<MmcBlobFilter>(loc.rank_, loc.mediaType_, NONE);
    std::unique_lock<std::mutex> guard(metaItemMtxs_[GetIndex(metaObj)]);
    ret = metaObj->UpdateBlobsState(key, filter, operateId, actRet);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("UpdateState: Failed to update blob state, ret: " << ret);
        return ret;
    }
    if (actRet == MMC_WRITE_FAIL) {
        ret = Remove(key);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("UpdateState: Failed remove key " << key << ", ret: " << ret);
            return ret;
        }
    }
    return MMC_OK;
}

void MmcMetaManager::PushRemoveList(const std::string& key, const MmcMemObjMetaPtr& meta)
{
    auto future =
        threadPool_->Enqueue([&](const std::string keyL, const MmcMemObjMetaPtr metaL,
                                 MmcGlobalAllocatorPtr allocator) { return metaL->FreeBlobs(keyL, allocator); },
                             key, meta, globalAllocator_);
    if (!future.valid()) {
        meta->FreeBlobs(key, globalAllocator_);
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
    metaContainer_->RegisterMedium(loc.mediaType_);
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
    Result ret = MMC_OK;
    uint32_t i = 0;
    for (; i < locs.size(); i++) {
        ret = Mount(locs[i], localMemInitInfos[i], blobMap);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Mount failed ret:" << ret << " loc rank:" << locs[i].rank_
                                              << " mediaType_: " << locs[i].mediaType_);
            break;
        }
    }
    if (ret != MMC_OK) {
        MMC_LOG_INFO("Mount locs partially failed, unmounting mounted locs...");
        for (; i > 0; i--) {
            auto unmountRet = Unmount(locs[i - 1]);
            if (unmountRet != MMC_OK) {
                MMC_LOG_ERROR("Unmount failed ret:" << unmountRet << " loc rank:" << locs[i - 1].rank_
                                                    << " mediaType_: " << locs[i - 1].mediaType_);
            }
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
        if (objMeta != nullptr) {
            objMeta->AddBlob(blobPtr);
            ret = metaContainer_->Insert(key, objMeta);
            if (ret == MMC_OK) {
                continue;
            }
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
    Result ret = globalAllocator_->Stop(loc);
    if (ret != MMC_OK) {
        return ret;
    }
    // Force delete the blobs
    MmcBlobFilterPtr filter = MmcMakeRef<MmcBlobFilter>(loc.rank_, loc.mediaType_, NONE);

    auto matchFunc = [this, &filter](const std::string& key, const MmcMemObjMetaPtr& objMeta) -> bool {
        if (objMeta == nullptr) {
            MMC_LOG_ERROR("objMeta is null");
            return false;
        }
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

Result MmcMetaManager::CopyBlob(const MmcMemObjMetaPtr& objMeta, const MmcMemBlobDesc& srcBlob,
                                const MmcLocation& dstLoc)
{
    if (objMeta == nullptr) {
        MMC_LOG_ERROR("objMeta is null");
        return MMC_INVALID_PARAM;
    }
    AllocOptions allocOpt{};
    allocOpt.blobSize_ = srcBlob.size_;
    allocOpt.numBlobs_ = 1;
    allocOpt.mediaType_ = dstLoc.mediaType_;
    allocOpt.preferredRank_.clear();
    allocOpt.preferredRank_.push_back(dstLoc.rank_);
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
        ret = metaNetServer_->SyncCall(request.dstBlob_.rank_, request, response, TIMEOUT_SECOND);
        if (ret != MMC_OK || response.ret_ != MMC_OK) {
            MMC_LOG_ERROR("copy blob from rank " << request.srcBlob_.rank_ << " to rank " << request.dstBlob_.rank_
                                                 << " failed:" << ret << "," << response.ret_);
            ret = MMC_ERROR;
            break;
        }

        ret = blobs[0]->UpdateState(MMC_WRITE_OK);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Failed to Update blob state, ret: " << ret);
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

Result MmcMetaManager::MoveBlob(const std::string& key, const MmcLocation& src, const MmcLocation& dst)
{
    MmcMemObjMetaPtr objMeta;
    if (metaContainer_->Get(key, objMeta) != MMC_OK) {
        MMC_LOG_ERROR("Cannot find MmcMemObjMeta with key : " << key);
        return MMC_UNMATCHED_KEY;
    }
    std::unique_lock<std::mutex> guard(metaItemMtxs_[GetIndex(objMeta)]);
    std::vector<MmcMemBlobDesc> blobsDesc;
    MmcBlobFilterPtr filter = MmcMakeRef<MmcBlobFilter>(src.rank_, src.mediaType_, READABLE);
    if (filter == nullptr) {
        MMC_LOG_ERROR("Fail to malloc filter");
        return MMC_MALLOC_FAILED;
    }
    objMeta->GetBlobsDesc(blobsDesc, filter);
    if (blobsDesc.empty()) {
        MMC_LOG_ERROR("blob for " << src << " to " << dst << " is empty with key : " << key << "," << objMeta);
        return MMC_UNMATCHED_KEY;
    }

    auto ret = CopyBlob(objMeta, blobsDesc[0], dst);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("key: " << key << " copy blob failed, ret " << ret);
        return ret;
    }

    filter = MmcMakeRef<MmcBlobFilter>(src.rank_, src.mediaType_, NONE);
    if (filter == nullptr) {
        MMC_LOG_ERROR("Fail to malloc filter");
        return MMC_MALLOC_FAILED;
    }
    ret = objMeta->FreeBlobs(key, globalAllocator_, filter);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("key: " << key << " free blob failed, ret " << ret);
        return ret;
    }

    MMC_LOG_INFO("move " << key << " from " << src << " to " << dst << " " << blobsDesc[0] << ", " << objMeta);
    return MMC_OK;
}

Result MmcMetaManager::ReplicateBlob(const std::string& key, const MmcLocation& loc)
{
    MmcMemObjMetaPtr objMeta;
    if (metaContainer_->Get(key, objMeta) != MMC_OK || objMeta.Get() == nullptr) {
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

    return CopyBlob(objMeta, blobsDesc[0], loc);
}

EvictResult MmcMetaManager::EvictCallBackFunction(const std::string &key, const MmcMemObjMetaPtr &objMeta)
{
    std::unique_lock<std::mutex> guard(metaItemMtxs_[GetIndex(objMeta)]);

    if (objMeta == nullptr) {
        MMC_LOG_ERROR("objMeta is null");
        return EvictResult::FAIL;
    }
    MediaType dstMedium = objMeta->MoveTo(true);
    MediaType srcMedium = MoveUp(dstMedium);
    if (dstMedium == MEDIA_NONE || srcMedium == MEDIA_NONE) {
        PushRemoveList(key, objMeta);
        return EvictResult::REMOVE;  // 向下淘汰已无可能，直接删除
    }
    MmcLocation src{UINT32_MAX, srcMedium};
    MmcLocation dst{UINT32_MAX, dstMedium};

    uint64_t freeSize = globalAllocator_->GetFreeSpace(dstMedium);
    if (freeSize < objMeta->Size()) {
        MMC_LOG_WARN("key: " << key << " move to " << dst << " no space:" << freeSize
                             << ", need:" << objMeta->Size());
        PushRemoveList(key, objMeta);
        return EvictResult::REMOVE;  // 向下淘汰已无可能，直接删除
    }

    auto future = threadPool_->Enqueue(
        [&](const std::string keyL, const MmcLocation srcL, const MmcLocation dstL) {
            return MoveBlob(keyL, srcL, dstL);
        },
        key, src, dst);
    if (!future.valid()) {
        MMC_LOG_WARN("key: " << key << " move blob from " << src << " to " << dst << " failed");
        PushRemoveList(key, objMeta);
        return EvictResult::REMOVE;  // 向下淘汰失败，直接删除
    }
    return EvictResult::MOVE_DOWN;
}

}  // namespace mmc
}  // namespace ock