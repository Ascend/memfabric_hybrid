/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc_meta_mgr_proxy_default.h"

namespace ock {
namespace mmc {
Result MmcMetaMgrProxyDefault::Alloc(const AllocRequest &req, AllocResponse &resp)
{
    MmcMemObjMetaPtr objMeta;
    MMC_RETURN_ERROR(metaMangerPtr_->Alloc(req.key_, req.options_, req.operateId_, objMeta),
                     "Meta Alloc Fail, key  " << req.key_);
    resp.numBlobs_ = objMeta->NumBlobs();
    resp.prot_ = objMeta->Prot();
    resp.priority_ = objMeta->Priority();

    std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs();
    for (size_t i = 0; i < blobs.size(); i++) {
        MmcMemBlobDesc blobDesc = blobs[i]->GetDesc();
        resp.blobs_.push_back(blobDesc);
        if (req.options_.preferredRank_ != blobDesc.rank_) {
            MetaReplicateRequest request{req.key_, blobDesc, objMeta->Prot(), objMeta->Priority()};
            Response response;
            netServerPtr_->SyncCall(blobDesc.rank_, request, response, timeOut_);
        }
    }

    // TODO: send a copy of the meta data to local service
    return MMC_OK;
}

Result MmcMetaMgrProxyDefault::BatchAlloc(const BatchAllocRequest &req, BatchAllocResponse &resp)
{
    std::vector<MmcMemObjMetaPtr> objMetas;
    std::vector<Result> allocResults;
    Result batchRet = metaMangerPtr_->BatchAlloc(req.keys_, req.options_, req.operateId_, objMetas, allocResults);
    if (batchRet != MMC_OK) {
        MMC_LOG_ERROR("batch alloc failed, error: " << batchRet);
        return batchRet;
    }

    resp.blobs_.resize(req.keys_.size());
    for (size_t i = 0; i < req.keys_.size(); ++i) {
        if (allocResults[i] != MMC_OK) {
            MMC_LOG_ERROR("Allocation failed for key: " << req.keys_[i] << ", error: " << allocResults[i]);
        }
        ProcessAllocatedObject(i, objMetas[i], req, resp);
    }
    
    return MMC_OK;
}

void MmcMetaMgrProxyDefault::ProcessAllocatedObject(size_t index, const MmcMemObjMetaPtr& objMeta,
                                                    const BatchAllocRequest &req, BatchAllocResponse &resp)
{
    if (objMeta == nullptr) {
        resp.numBlobs_.push_back(0);
        resp.prots_.push_back(0);
        resp.priorities_.push_back(0);
        resp.leases_.push_back(0);
        resp.blobs_[index] = {};
        return;
    }
    resp.numBlobs_.push_back(objMeta->NumBlobs());
    resp.prots_.push_back(objMeta->Prot());
    resp.priorities_.push_back(objMeta->Priority());
    resp.leases_.push_back(0);

    std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs();
    resp.blobs_[index].resize(blobs.size());
    
    for (size_t j = 0; j < blobs.size(); ++j) {
        const MmcMemBlobDesc& blobDesc = blobs[j]->GetDesc();
        resp.blobs_[index][j] = blobDesc;
    }
}

void MmcMetaMgrProxyDefault::HandleBlobReplication(size_t objIndex, size_t blobIndex,
                                                   const MmcMemBlobDesc& blobDesc,
                                                   const MmcMemObjMetaPtr& objMeta,
                                                   const BatchAllocRequest &req)
{
    MetaReplicateRequest replicateReq{
        req.keys_[objIndex],
        blobDesc,
        objMeta->Prot(),
        objMeta->Priority()
    };
    
    Response replicateResp;
    netServerPtr_->SyncCall(blobDesc.rank_, replicateReq, replicateResp, timeOut_);
}

Result MmcMetaMgrProxyDefault::UpdateState(const UpdateRequest& req, Response& resp)
{
    MmcLocation loc{req.rank_, static_cast<MediaType>(req.mediaType_)};
    Result ret = metaMangerPtr_->UpdateState(req.key_, loc, req.actionResult_, req.operateId_);
    resp.ret_ = ret;
    return MMC_OK;
}

Result MmcMetaMgrProxyDefault::BatchUpdateState(const BatchUpdateRequest& req, BatchUpdateResponse& resp)
{
    const size_t keyCount = req.keys_.size();
    if (keyCount != req.ranks_.size() || keyCount != req.mediaTypes_.size()) {
        MMC_LOG_ERROR("BatchUpdateState: Input vectors size mismatch {keyNum:"
                      << req.keys_.size() << ", rankNum:" << req.ranks_.size()
                      << ", mediaNum:" << req.mediaTypes_.size() << "}");
        return MMC_ERROR;
    }

    std::vector<MmcLocation> locs;
    locs.reserve(keyCount);
    for (size_t i = 0; i < keyCount; ++i) {
        locs.push_back({req.ranks_[i], static_cast<MediaType>(req.mediaTypes_[i])});
    }

    Result ret = metaMangerPtr_->BatchUpdateState(req.keys_, locs, req.actionResults_, req.operateId_, resp.results_);
    size_t successCount = std::count(resp.results_.begin(), resp.results_.end(), MMC_OK);
    MMC_LOG_INFO("BatchUpdate completed: " << successCount << "/" << keyCount << " updates succeeded, ret:" << ret);
    return ret;
}

Result MmcMetaMgrProxyDefault::Get(const GetRequest &req, AllocResponse &resp)
{
    MmcMemObjMetaPtr objMeta;
    MMC_RETURN_ERROR(metaMangerPtr_->Get(req.key_, objMeta), "failed to get objMeta for key " << req.key_);
    objMeta->Lock();
    if (objMeta->NumBlobs() == 0) {
        objMeta->Unlock();
        MMC_LOG_ERROR("key " << req.key_ << " already released ");
        return MMC_ERROR;
    }

    MmcBlobFilterPtr filterPtr = MmcMakeRef<MmcBlobFilter>(UINT32_MAX, MEDIA_NONE, READABLE);
    std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs(filterPtr);
    for (auto blob : blobs) {
        auto ret = blob->UpdateState(req.rankId_, req.operateId_, MMC_READ_START);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("update key " << req.key_ << " blob state failed with error: " << ret);
            continue;
        }
        resp.blobs_.push_back(blob->GetDesc());
        break;  // 只需返回一个
    }

    resp.numBlobs_ = resp.blobs_.size();
    if (resp.numBlobs_ == 0) {
        MMC_LOG_ERROR("key " << req.key_ << " blob num is 0");
        objMeta->Unlock();
        return MMC_ERROR;
    }
    resp.prot_ = objMeta->Prot();
    resp.priority_ = objMeta->Priority();

    objMeta->Unlock();
    return MMC_OK;
}

Result MmcMetaMgrProxyDefault::BatchGet(const BatchGetRequest &req, BatchAllocResponse &resp)
{
    std::vector<MmcMemObjMetaPtr> objMetas;
    std::vector<Result> getResults;

    Result ret = metaMangerPtr_->BatchGet(req.keys_, objMetas, getResults);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("BatchGet Fail, error:" << ret);
        return ret;
    }

    if (objMetas.size() != req.keys_.size()) {
        MMC_LOG_ERROR("BatchGet response size mismatch: expected " << req.keys_.size() << ", got " << objMetas.size());
        return MMC_ERROR;
    }

    resp.numBlobs_.resize(req.keys_.size());
    resp.prots_.resize(req.keys_.size());
    resp.priorities_.resize(req.keys_.size());
    resp.leases_.resize(req.keys_.size());
    resp.blobs_.resize(req.keys_.size());

    for (size_t i = 0; i < req.keys_.size(); ++i) {
        if (getResults[i] == MMC_OK && objMetas[i] != nullptr) {
            const MmcMemObjMetaPtr &objMeta = objMetas[i];
            objMeta->Lock();
            if (objMeta->NumBlobs() == 0) {
                getResults[i] = MMC_OBJECT_NOT_EXISTS;
                objMeta->Unlock();
                continue;
            }

            MmcBlobFilterPtr filterPtr = MmcMakeRef<MmcBlobFilter>(UINT32_MAX, MEDIA_NONE, READABLE);
            std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs(filterPtr);
            std::vector<MmcMemBlobDesc> descVec;
            for (auto blob : blobs) {
                auto ret = blob->UpdateState(req.rankId_, req.operateId_, MMC_READ_START);
                if (ret != MMC_OK) {
                    MMC_LOG_ERROR("update key " << req.keys_[i] << " blob state failed with error: " << ret);
                    continue;
                }
                descVec.push_back(blob->GetDesc());
                break;  // 只需返回一个
            }

            resp.numBlobs_[i] = descVec.size();
            if (resp.numBlobs_[i] == 0) {
                getResults[i] = MMC_OBJECT_NOT_EXISTS;
                MMC_LOG_ERROR("key " << req.keys_[i] << " blob num is 0");
            }
            resp.blobs_[i] = descVec;
            resp.prots_[i] = objMeta->Prot();
            resp.priorities_[i] = objMeta->Priority();
            resp.leases_[i] = 0;
            objMeta->Unlock();
        } else {
            getResults[i] = getResults[i] == MMC_OK ? MMC_OBJECT_NOT_EXISTS : getResults[i];
            resp.numBlobs_[i] = 0;
            resp.prots_[i] = 0;
            resp.priorities_[i] = 0;
            resp.leases_[i] = 0;
            MMC_LOG_ERROR("Key " << req.keys_[i] << " not found");
        }
    }
    return MMC_OK;
}

}
}