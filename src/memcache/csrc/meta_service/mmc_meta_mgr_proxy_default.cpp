/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "mmc_meta_mgr_proxy_default.h"

namespace ock {
namespace mmc {
Result MmcMetaMgrProxyDefault::Alloc(const AllocRequest &req, AllocResponse &resp)
{
    metaMangerPtr_->CheckAndEvict();
    MmcMemMetaDesc objMeta;
    auto ret = metaMangerPtr_->Alloc(req.key_, req.options_, req.operateId_, objMeta);
    if (ret != MMC_OK) {
        if (ret != MMC_DUPLICATED_OBJECT) {
            MMC_RETURN_ERROR(ret, "Meta Alloc Fail, key  " << req.key_);
        } else {
            return ret;
        }
    }
    resp.numBlobs_ = objMeta.numBlobs_;
    resp.prot_ = objMeta.prot_;
    resp.priority_ = objMeta.priority_;
    resp.blobs_ = objMeta.blobs_;
    return MMC_OK;
}

Result MmcMetaMgrProxyDefault::BatchAlloc(const BatchAllocRequest &req, BatchAllocResponse &resp)
{
    metaMangerPtr_->CheckAndEvict();
    resp.results_.resize(req.keys_.size());
    resp.blobs_.resize(req.keys_.size());
    MMC_ASSERT_RETURN(req.keys_.size() == req.options_.size(), MMC_ERROR);
    for (size_t i = 0; i < req.keys_.size(); ++i) {
        MmcMemMetaDesc objMeta{};
        Result ret = metaMangerPtr_->Alloc(req.keys_[i], req.options_[i], req.operateId_, objMeta);
        if (ret != MMC_OK) {
            if (ret != MMC_DUPLICATED_OBJECT) {
                MMC_LOG_ERROR("Allocation failed for key: " << req.keys_[i] << ", error: " << ret);
            }
            resp.numBlobs_.push_back(0);
            resp.prots_.push_back(0);
            resp.priorities_.push_back(0);
            resp.leases_.push_back(0);
            resp.blobs_[i] = {};
        } else {
            resp.numBlobs_.push_back(objMeta.numBlobs_);
            resp.prots_.push_back(objMeta.prot_);
            resp.priorities_.push_back(objMeta.priority_);
            resp.leases_.push_back(0);
            resp.blobs_[i] = objMeta.blobs_;
        }
        resp.results_[i] = ret;
    }
    return MMC_OK;
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

    for (size_t i = 0; i < keyCount; ++i) {
        MmcLocation loc{req.ranks_[i], static_cast<MediaType>(req.mediaTypes_[i])};
        Result ret = metaMangerPtr_->UpdateState(req.keys_[i], loc, req.actionResults_[i], req.operateId_);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("update for key: " << req.keys_[i] << " failed, error: " << ret);
        }
        resp.results_.push_back(ret);
    }
    return MMC_OK;
}

Result MmcMetaMgrProxyDefault::Get(const GetRequest& req, AllocResponse& resp)
{
    MmcMemMetaDesc objMeta;
    MmcBlobFilterPtr filterPtr = MmcMakeRef<MmcBlobFilter>(UINT32_MAX, MEDIA_NONE, READABLE);
    MMC_RETURN_ERROR(metaMangerPtr_->Get(req.key_, req.operateId_, filterPtr, objMeta),
                     "failed to get objMeta for key " << req.key_);
    if (objMeta.numBlobs_ == 0 || objMeta.blobs_.empty()) {
        MMC_LOG_ERROR("key " << req.key_ << " already released ");
        return MMC_ERROR;
    }

    resp.blobs_ = objMeta.blobs_;
    resp.numBlobs_ = objMeta.blobs_.size();
    resp.prot_ = objMeta.prot_;
    resp.priority_ = objMeta.priority_;
    return MMC_OK;
}

Result MmcMetaMgrProxyDefault::BatchGet(const BatchGetRequest& req, BatchAllocResponse& resp)
{
    resp.numBlobs_.resize(req.keys_.size(), 0);
    resp.prots_.resize(req.keys_.size(), 0);
    resp.priorities_.resize(req.keys_.size(), 0);
    resp.leases_.resize(req.keys_.size(), 0);
    resp.blobs_.resize(req.keys_.size());

    MmcBlobFilterPtr filterPtr = MmcMakeRef<MmcBlobFilter>(UINT32_MAX, MEDIA_NONE, READABLE);
    for (size_t i = 0; i < req.keys_.size(); ++i) {
        MmcMemMetaDesc objMeta{};
        auto ret = metaMangerPtr_->Get(req.keys_[i], req.operateId_, filterPtr, objMeta);
        if (ret != MMC_OK || objMeta.blobs_.empty() || objMeta.numBlobs_ != objMeta.blobs_.size()) {
            resp.numBlobs_[i] = 0;
            resp.blobs_[i] = {};
            resp.prots_[i] = 0;
            resp.priorities_[i] = 0;
            MMC_LOG_ERROR("Key " << req.keys_[i] << " not found");
        } else {
            resp.numBlobs_[i] = objMeta.numBlobs_;
            resp.blobs_[i] = objMeta.blobs_;
            resp.prots_[i] = objMeta.prot_;
            resp.priorities_[i] = objMeta.priority_;
        }
        resp.results_.push_back(ret);
    }
    return MMC_OK;
}

Result MmcMetaMgrProxyDefault::BatchExistKey(const BatchIsExistRequest& req, BatchIsExistResponse& resp)
{
    resp.results_.reserve(req.keys_.size());
    for (size_t i = 0; i < req.keys_.size(); ++i) {
        auto ret = metaMangerPtr_->ExistKey(req.keys_[i]);
        if (ret != MMC_OK && ret != MMC_UNMATCHED_KEY) {
            MMC_LOG_ERROR("get key: " << req.keys_[i] << " unexpected result: " << ret);
        }
        resp.results_.emplace_back(ret);
    }
    return MMC_OK;
}

}  // namespace mmc
}  // namespace ock