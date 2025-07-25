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
    
    resp.blobs_.resize(req.keys_.size());
    
    for (size_t i = 0; i < req.keys_.size(); ++i) {
        if (allocResults[i] != MMC_OK) {
            MMC_LOG_DEBUG("Allocation failed for key: " << req.keys_[i]
                          << ", error: " << allocResults[i]);
            continue;
        }
        
        ProcessAllocatedObject(i, objMetas[i], req, resp);
    }
    
    return batchRet;
}

void MmcMetaMgrProxyDefault::ProcessAllocatedObject(size_t index, const MmcMemObjMetaPtr& objMeta,
                                                    const BatchAllocRequest &req, BatchAllocResponse &resp)
{
    resp.numBlobs_.push_back(objMeta->NumBlobs());
    resp.prots_.push_back(objMeta->Prot());
    resp.priorities_.push_back(objMeta->Priority());
    resp.leases_.push_back(0);
    
    std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs();
    resp.blobs_[index].resize(blobs.size());
    
    for (size_t j = 0; j < blobs.size(); ++j) {
        const MmcMemBlobDesc& blobDesc = blobs[j]->GetDesc();
        resp.blobs_[index][j] = blobDesc;
        
        if (req.options_[index].preferredRank_ != blobDesc.rank_) {
            HandleBlobReplication(index, j, blobDesc, objMeta, req);
        }
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

Result MmcMetaMgrProxyDefault::UpdateState(const UpdateRequest &req, Response &resp)
{

    // const std::string &key, const MmcLocation &loc, const BlobActionResult &actRet
    MmcLocation loc{req.rank_, static_cast<MediaType>(req.mediaType_)};
    Result ret = metaMangerPtr_->UpdateState(req.key_, loc, req.rank_, req.operateId_, req.actionResult_);
    resp.ret_ = ret;
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("Meta Update State Fail!");
        return MMC_ERROR;
    } else {
        return MMC_OK;
    }
}

Result MmcMetaMgrProxyDefault::Get(const GetRequest &req, AllocResponse &resp)
{

    MmcMemObjMetaPtr objMeta;
    metaMangerPtr_->Get(req.key_, objMeta);
    objMeta->Lock();
    if (objMeta->NumBlobs() == 0) {
        objMeta->Unlock();
        MMC_LOG_ERROR("key " << req.key_ << " already released ");
        return MMC_ERROR;
    }
    resp.numBlobs_ = objMeta->NumBlobs();
    resp.prot_ = objMeta->Prot();
    resp.priority_ = objMeta->Priority();


    MmcBlobFilterPtr filterPtr = MmcMakeRef<MmcBlobFilter>(req.rankId_, MEDIA_NONE, NONE);
    std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs(filterPtr);
    MmcMemBlobPtr blobPtr = nullptr;
    for (auto iter = blobs.begin(); iter != blobs.end(); iter++) {
        if ((*iter)->Type() == MEDIA_HBM) {
            blobPtr = *iter;
        }
    }
    if (blobPtr == nullptr) {
        blobPtr = objMeta->GetBlobs()[0];
    }
    blobPtr->UpdateState(req.rankId_, req.operateId_, MMC_READ_START);
    resp.blobs_.push_back(blobPtr->GetDesc());
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
        if (getResults[i] == MMC_OK) {
            const MmcMemObjMetaPtr &objMeta = objMetas[i];
            objMeta->Lock();
            if (objMeta->NumBlobs() == 0) {
                getResults[i] = MMC_OBJECT_NOT_EXISTS;
                objMeta->Unlock();
                continue;
            }
            resp.numBlobs_[i] = objMeta->NumBlobs();
            resp.prots_[i] = objMeta->Prot();
            resp.priorities_[i] = objMeta->Priority();
            resp.leases_[i] = 0;
            MmcBlobFilterPtr filterPtr = MmcMakeRef<MmcBlobFilter>(req.rankId_, MEDIA_NONE, NONE);
            std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs(filterPtr);
            MmcMemBlobPtr blobPtr = nullptr;
            for (auto iter = blobs.begin(); iter != blobs.end(); iter++) {
                if ((*iter)->Type() == MEDIA_HBM) {
                    blobPtr = *iter;
                }
            }
            if (blobPtr == nullptr) {
                blobPtr = objMeta->GetBlobs()[0];
            }
            blobPtr->UpdateState(req.rankId_, req.operateId_, MMC_READ_START);
            resp.blobs_[i].push_back(blobPtr->GetDesc());
            objMeta->Unlock();
        } else {
            resp.numBlobs_[i] = 0;
            resp.prots_[i] = 0;
            resp.priorities_[i] = 0;
            resp.leases_[i] = 0;
            MMC_LOG_DEBUG("Key " << req.keys_[i] << " not found");
        }
    }
    return MMC_OK;
}

}
}