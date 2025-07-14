/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc_meta_mgr_proxy_default.h"

namespace ock {
namespace mmc {
Result MmcMetaMgrProxyDefault::Alloc(const AllocRequest &req, AllocResponse &resp)
{
    MmcMemObjMetaPtr objMeta;
    Result ret = metaMangerPtr_->Alloc(req.key_, req.options_, objMeta);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("Meta Alloc Fail!");
        return MMC_ERROR;
    }
    resp.numBlobs_ = objMeta->NumBlobs();
    resp.prot_ = objMeta->Prot();
    resp.priority_ = objMeta->Priority();
    resp.lease_ = objMeta->Lease();

    std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs();
    for (size_t i = 0; i < blobs.size(); i++) {
        MmcMemBlobDesc blobDesc = blobs[i]->GetDesc();
        resp.blobs_.push_back(blobDesc);
        if (req.options_.preferredRank_ != blobDesc.rank_) {
            MetaReplicateRequest request{req.key_, blobDesc, objMeta->Prot(), objMeta->Priority(),
                                         objMeta->Lease()};
            Response response;
            netServerPtr_->SyncCall(blobDesc.rank_, request, response, timeOut_);
        }
    }

    // TODO: send a copy of the meta data to local service
    return MMC_OK;
}

Result MmcMetaMgrProxyDefault::UpdateState(const UpdateRequest &req, Response &resp)
{

    // const std::string &key, const MmcLocation &loc, const BlobActionResult &actRet
    MmcLocation loc{req.rank_, req.mediaType_};
    Result ret = metaMangerPtr_->UpdateState(req.key_, loc, req.actionResult_);
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
    resp.numBlobs_ = objMeta->NumBlobs();
    resp.prot_ = objMeta->Prot();
    resp.priority_ = objMeta->Priority();
    resp.lease_ = objMeta->Lease();

    std::vector<MmcMemBlobPtr> blobs = objMeta->GetBlobs();
    for (size_t i = 0; i < blobs.size(); i++) {
        resp.blobs_.push_back(blobs[i]->GetDesc());
    }
    return MMC_OK;
}

Result MmcMetaMgrProxyDefault::Remove(const RemoveRequest &req, Response &resp)
{

    MmcMemObjMetaPtr objMeta;
    Result ret = metaMangerPtr_->Remove(req.key_);
    resp.ret_ = ret;
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("Remove failed for key: " << req.key_ << " error: " << ret);
        return MMC_ERROR;
    } else {
        return MMC_OK;
    }
}

Result MmcMetaMgrProxyDefault::BatchRemove(const BatchRemoveRequest &req, BatchRemoveResponse &resp)
{
    std::vector<Result> results;
    results.reserve(req.keys_.size());

    Result ret = metaMangerPtr_->BatchRemove(req.keys_, results);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("BatchRemove failed");
        return ret;
    }

    for (size_t i = 0; i < req.keys_.size(); ++i) {
        if (results[i] != MMC_OK) {
            MMC_LOG_ERROR("BatchRemove failed for key: " << req.keys_[i] << " error: " << results[i]);
        }
    }

    resp.results_ = results;
    return MMC_OK;
}

}
}