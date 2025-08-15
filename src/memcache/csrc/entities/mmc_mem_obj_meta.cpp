/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
 
#include "mmc_mem_obj_meta.h"
#include "mmc_global_allocator.h"
#include <algorithm>
#include <chrono>

namespace ock {
namespace mmc {

// TODO: 此处去重成本比较高，先不做
Result MmcMemObjMeta::AddBlob(const MmcMemBlobPtr &blob)
{
    if (numBlobs_ != 0 && size_ != blob->Size()) {
        MMC_LOG_ERROR("add blob size:" << blob->Size() << " != meta size:" << size_);
        return MMC_ERROR;
    }

    if (numBlobs_ < MAX_NUM_BLOB_CHAINS) {
        for (size_t i = 0; i < MAX_NUM_BLOB_CHAINS; ++i) {
            if (blobs_[i] == nullptr) {
                blobs_[i] = blob;
                numBlobs_++;
                size_ = blob->Size();
                break;
            }
        }
    } else {
        MmcMemBlobPtr last = blobs_[MAX_NUM_BLOB_CHAINS - 1];
        while (last->Next() != nullptr) {
            last = last->Next();
        }
        last->Next(blob);
        numBlobs_++;
    }
    return MMC_OK;
}

Result MmcMemObjMeta::RemoveBlobs(const MmcBlobFilterPtr &filter, bool revert)
{
    uint8_t oldNumBlobs = numBlobs_;
    for (size_t i = 0; i < MAX_NUM_BLOB_CHAINS - 1; ++i) {
        if (blobs_[i] != nullptr && blobs_[i]->MatchFilter(filter) ^ revert) {
            blobs_[i] = nullptr;
            numBlobs_--;
        }
    }

    MmcMemBlobPtr pre = blobs_[MAX_NUM_BLOB_CHAINS - 1];
    while (pre != nullptr && pre->Next() != nullptr) {
        MmcMemBlobPtr cur = pre->Next();
        if (cur->MatchFilter(filter) ^ revert) {
            pre->Next() = cur->Next();
            cur = nullptr;
            numBlobs_--;
        }
        pre = pre->Next();
    }

    MmcMemBlobPtr head = blobs_[MAX_NUM_BLOB_CHAINS - 1];
    if (head != nullptr && head->MatchFilter(filter) ^ revert) {
        blobs_[MAX_NUM_BLOB_CHAINS - 1] = head->Next();
        head = nullptr;
        numBlobs_--;
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
    MmcMemBlobPtr curBlob;
    for (size_t i = 0; i < MAX_NUM_BLOB_CHAINS; ++i) {
        curBlob = blobs_[i];
        if (curBlob != nullptr && (curBlob->MatchFilter(filter) ^ revert)) {
            blobs.push_back(curBlob);
        }
    }

    while (curBlob != nullptr) {
        curBlob = curBlob->Next();
        if (curBlob != nullptr && (curBlob->MatchFilter(filter) ^ revert)) {
            blobs.push_back(curBlob);
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
}  // namespace mmc
}  // namespace ock