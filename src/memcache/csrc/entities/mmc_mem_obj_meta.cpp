/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
 
#include "mmc_mem_obj_meta.h"
#include <chrono>
#include "mmc_global_allocator.h"

namespace ock {
namespace mmc {

static const uint16_t MAX_NUM_BLOB_CHAINS = 5;  // to make sure MmcMemObjMeta <= 64 bytes

Result MmcMemObjMeta::AddBlob(const MmcMemBlobPtr& blob)
{
    if (numBlobs_ != 0 && size_ != blob->Size()) {
        MMC_LOG_ERROR("add blob size:" << blob->Size() << " != meta size:" << size_);
        return MMC_ERROR;
    }
    for (const auto &old : blobs_) {
        if (old == nullptr || blob == nullptr) {
            MMC_LOG_ERROR("null ptr find: " << (old == nullptr));
            return MMC_ERROR;
        }
        if (old->GetDesc() == blob->GetDesc()) {
            MMC_LOG_INFO("find old block: " << blob->GetDesc());
            return MMC_OK;
        }
    }
    blobs_.emplace_back(blob);
    numBlobs_++;
    size_ = blob->Size();
    return MMC_OK;
}

Result MmcMemObjMeta::RemoveBlobs(const MmcBlobFilterPtr& filter, bool revert)
{
    uint8_t oldNumBlobs = numBlobs_;

    for (auto iter = blobs_.begin(); iter != blobs_.end();) {
        auto& blob = *iter;
        if ((blob != nullptr) && (blob->MatchFilter(filter) ^ revert)) {
            iter = blobs_.erase(iter);
            numBlobs_--;
        } else {
            iter++;
        }
    }

    return numBlobs_ < oldNumBlobs ? MMC_OK : MMC_ERROR;
}

Result MmcMemObjMeta::FreeBlobs(const std::string &key, MmcGlobalAllocatorPtr &allocator,
                                const MmcBlobFilterPtr &filter, bool doBackupRemove)
{
    if (NumBlobs() == 0) {
        return MMC_OK;
    }
    std::vector<MmcMemBlobPtr> blobs = GetBlobs(filter);
    RemoveBlobs(filter);
    Result result = MMC_OK;
    for (size_t i = 0; i < blobs.size(); i++) {
        if (doBackupRemove) {
            MMC_RETURN_ERROR(blobs[i]->BackupRemove(key), "memBlob remove backup error");
        }
        auto ret = blobs[i]->UpdateState(key, 0, 0, MMC_REMOVE_START);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("remove op, meta update failed:" << ret);
            result = MMC_ERROR;
        }
        ret = allocator->Free(blobs[i]);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Error in free blobs! failed:" << ret);
            result = MMC_ERROR;
        }
    }
    return result;
}

std::vector<MmcMemBlobPtr> MmcMemObjMeta::GetBlobs(const MmcBlobFilterPtr &filter, bool revert)
{
    std::vector<MmcMemBlobPtr> blobs;
    for (auto blob : blobs_) {
        if ((blob != nullptr) && (blob->MatchFilter(filter) ^ revert)) {
            blobs.emplace_back(blob);
        }
    }
    return blobs;
}

void MmcMemObjMeta::GetBlobsDesc(std::vector<MmcMemBlobDesc>& blobsDesc, const MmcBlobFilterPtr& filter, bool revert)
{
    std::vector<MmcMemBlobPtr> blobs = GetBlobs(filter, revert);
    for (const auto& blob : blobs) {
        blobsDesc.emplace_back(blob->GetDesc());
    }
}

Result MmcMemObjMeta::UpdateBlobsState(const std::string& key, const MmcBlobFilterPtr& filter, uint64_t operateId,
                                       BlobActionResult actRet)
{
    std::vector<MmcMemBlobPtr> blobs = GetBlobs(filter);

    uint32_t opRankId = GetRankIdByOperateId(operateId);
    uint32_t opSeq = GetSequenceByOperateId(operateId);

    Result result = MMC_OK;
    for (auto blob : blobs) {
        auto ret = blob->UpdateState(key, opRankId, opSeq, actRet);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Update rank:" << opRankId << ", seq:" << opSeq << " blob state by " << std::to_string(actRet)
                                         << " Fail!");
            result = MMC_ERROR;
        }
    }
    return result;
}

MediaType MmcMemObjMeta::MoveTo(bool down)
{
    MediaType mediaType = MediaType::MEDIA_NONE;
    for (auto blob : blobs_) {
        if (blob != nullptr) {
            mediaType = static_cast<MediaType>(blob->Type());
            break;
        }
    }

    return down ? MoveDown(mediaType) : MoveUp(mediaType);
}

}  // namespace mmc
}  // namespace ock