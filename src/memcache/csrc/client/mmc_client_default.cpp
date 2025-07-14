/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc_client_default.h"
#include "mmc_msg_client_meta.h"
#include "mmc_mem_obj_meta.h"
#include "mmc_bm_proxy.h"

namespace ock {
namespace mmc {
Result MmcClientDefault::Start(const mmc_client_config_t &config)
{
    MMC_LOG_INFO("Starting client " << name_);
    std::lock_guard<std::mutex> guard(mutex_);
    if (started_) {
        MMC_LOG_INFO("MetaService " << name_ << " already started");
        return MMC_OK;
    }

    auto tmpNetClient  = MetaNetClientFactory::GetInstance(config.discoveryURL, "MetaClientCommon").Get();
    MMC_ASSERT_RETURN(tmpNetClient != nullptr, MMC_NEW_OBJECT_FAILED);
    if (!tmpNetClient->Status()) {
        MMC_LOG_ERROR_AND_RETURN_NOT_OK(tmpNetClient->Start(config.rankId),
                                        "Failed to start net server of local service " << name_);
        MMC_LOG_ERROR_AND_RETURN_NOT_OK(tmpNetClient->Connect(config.discoveryURL),
                                        "Failed to connect net server of local service " << name_);
    }

    randId_ = config.rankId;
    bmProxy_ = MmcBmProxyFactory::GetInstance("bmProxyDefault");

    metaNetClient_ = tmpNetClient;
    started_ = true;
    return MMC_OK;
}

void MmcClientDefault::Stop()
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!started_) {
        MMC_LOG_WARN("MmcClientDefault has not been started");
        return;
    }

    if (metaNetClient_ != nullptr) {
        metaNetClient_->Stop();
    }
    started_ = false;
}

const std::string &MmcClientDefault::Name() const
{
    return name_;
}

Result MmcClientDefault::Put(const char *key, mmc_buffer *buf, mmc_put_options &options, uint32_t flags)
{
    // todo 参数校验
    uint64_t blobSize = buf->type == 0 ? buf->dram.len : buf->hbm.width * buf->hbm.layerNum;
    AllocRequest request{key, {blobSize, 1, options.mediaType, RankId(options.policy), flags}};
    AllocResponse response;
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetClient_->SyncCall(request, response, timeOut_),
                                    "client " << name_ << " alloc " << key << " failed");

    for (uint8_t i = 0; i < response.numBlobs_; i++) {
        MMC_LOG_ERROR_AND_RETURN_NOT_OK(bmProxy_->Put(buf, response.blobs_[i].gva_, blobSize),
                                        "client " << name_ << " put " << key << " failed");
    }
    UpdateRequest updateRequest{MMC_WRITE_OK, key, 0, 0};
    Response updateResponse;
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetClient_->SyncCall(updateRequest, updateResponse, timeOut_),
                                    "client " << name_ << " update " << key << " failed");
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(updateResponse.ret_, "client " << name_ << " update " << key << " failed");

    return MMC_OK;
}

Result MmcClientDefault::Get(const char *key, mmc_buffer *buf, uint32_t flags) const
{
    GetRequest request{key};
    AllocResponse response;
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetClient_->SyncCall(request, response, timeOut_),
                                    "client " << name_ << " get " << key << " failed");
    if (response.numBlobs_ > 0) {
        MMC_LOG_ERROR_AND_RETURN_NOT_OK(bmProxy_->Get(buf, response.blobs_[0].gva_),
                                        "client " << name_ << " put " << key << " failed");
    } else {
        MMC_LOG_ERROR("client " << name_ << " get " << key << " failed");
        return MMC_ERROR;
    }
    return MMC_OK;
}

mmc_location_t MmcClientDefault::GetLocation(const char *key, uint32_t flags) const
{
    GetRequest request{key};
    AllocResponse response;
    MMC_ASSERT_RETURN(metaNetClient_->SyncCall(request, response, timeOut_) == MMC_OK, {-1});
    return {};
}

Result MmcClientDefault::Remove(const char *key, uint32_t flags) const
{
    RemoveRequest request{key};
    Response response;
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetClient_->SyncCall(request, response, timeOut_),
                                    "client " << name_ << " remove " << key << " failed");
    return 0;
}

Result MmcClientDefault::BatchRemove(const std::vector<std::string>& keys, std::vector<Result>& remove_results, uint32_t flags) {
    BatchRemoveRequest request{keys};
    BatchRemoveResponse response;
    
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(
        metaNetClient_->SyncCall(request, response, timeOut_),
        "client " << name_ << " BatchRemove failed"
    );

    if (response.results_.size() != keys.size()) {
        MMC_LOG_ERROR("BatchRemove response size mismatch. Expected: " << keys.size() << ", Got: " << response.results_.size());
        std::fill(remove_results.begin(), remove_results.end(), MMC_ERROR);
        return MMC_ERROR;
    }

    remove_results = response.results_;
    return MMC_OK;
}

Result MmcClientDefault::IsExist(const std::string &key, uint32_t flags) const
{
    IsExistRequest request{key};
    Response response;
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetClient_->SyncCall(request, response, timeOut_),
                                    "client " << name_ << " IsExist " << key << " failed");
    return response.ret_;
}

Result MmcClientDefault::BatchIsExist(const std::vector<std::string> &keys, std::vector<Result> &exist_results,
                                      uint32_t flags) const
{
    BatchIsExistRequest request{keys};
    BatchIsExistResponse response;
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetClient_->SyncCall(request, response, timeOut_),
                                    "client " << name_ << " BatchIsExist failed");

    if (response.results_.size() != keys.size()) {
        MMC_LOG_ERROR("BatchIsExist response size mismatch. Expected: " << keys.size()
                                                                        << ", Got: " << response.results_.size());
        std::fill(exist_results.begin(), exist_results.end(), MMC_ERROR);
        return MMC_ERROR;
    }

    exist_results.resize(keys.size());
    exist_results = response.results_;

    return response.ret_;
}
}
}