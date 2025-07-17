/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc_client_default.h"
#include "mmc_msg_client_meta.h"
#include "mmc_mem_obj_meta.h"
#include "mmc_bm_proxy.h"
#include "mmc_montotonic.h"

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
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetClient_->SyncCall(request, response, rpcTimeOut_),
                                    "client " << name_ << " alloc " << key << " failed");

    for (uint8_t i = 0; i < response.numBlobs_; i++) {
        MMC_LOG_ERROR_AND_RETURN_NOT_OK(bmProxy_->Put(buf, response.blobs_[i].gva_, blobSize),
                                        "client " << name_ << " put " << key << " failed");
    }
    UpdateRequest updateRequest{MMC_WRITE_OK, key, 0, 0};
    Response updateResponse;
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetClient_->SyncCall(updateRequest, updateResponse, rpcTimeOut_),
                                    "client " << name_ << " update " << key << " failed");
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(updateResponse.ret_, "client " << name_ << " update " << key << " failed");

    return MMC_OK;
}

Result MmcClientDefault::Get(const char *key, mmc_buffer *buf, uint32_t flags) const
{
    GetRequest request{key};
    AllocResponse response;
    uint64_t startTime = ock::dagger::Monotonic::TimeUs();
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetClient_->SyncCall(request, response, rpcTimeOut_),
                                    "client " << name_ << " get " << key << " failed");
    if (response.numBlobs_ > 0) {
        MMC_LOG_ERROR_AND_RETURN_NOT_OK(bmProxy_->Get(buf, response.blobs_[0].gva_),
                                        "client " << name_ << " get " << key << " failed");
        uint64_t timeMs = (ock::dagger::Monotonic::TimeUs() - startTime) / 1000U;
        MMC_ASSERT_RETURN(timeMs < defaultTtlMs_, MMC_ERROR);
    } else {
        MMC_LOG_ERROR("client " << name_ << " get " << key << " failed");
        return MMC_ERROR;
    }
    return MMC_OK;
}

Result MmcClientDefault::BatchGet(const std::vector<std::string> &keys,
                                  std::vector<mmc_buffer> &bufs, uint32_t flags) const
{
    BatchGetRequest request{keys};
    BatchAllocResponse response;
    uint64_t startTime = ock::dagger::Monotonic::TimeUs();
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(
        metaNetClient_->SyncCall(request, response, rpcTimeOut_),
        "client " << name_ << " batch get failed"
    );

    if (response.blobs_.size() != keys.size()) {
        MMC_LOG_ERROR("client " << name_ << " batch get response size mismatch: expected " << keys.size() << ", got "
                       << response.blobs_.size());
        return MMC_ERROR;
    };

    for (size_t i = 0; i < keys.size(); ++i) {
        const auto &blobs = response.blobs_[i];
        uint8_t numBlobs = response.numBlobs_[i];

        if (numBlobs > 0) {
            if (i >= bufs.size()) {
                MMC_LOG_ERROR("client " << name_ << " batch get buffer size mismatch for key " << keys[i]);
                return MMC_ERROR;
            }

            MMC_LOG_ERROR_AND_RETURN_NOT_OK(bmProxy_->Get(&bufs[i], blobs[0].gva_),
                "client " << name_ << " batch get failed for key " << keys[i]);

        } else {
            MMC_LOG_ERROR("client " << name_ << " batch get failed for key " << keys[i]);
            return MMC_ERROR;
        }
    }
    uint64_t timeMs = (ock::dagger::Monotonic::TimeUs() - startTime) / 1000U;
    MMC_ASSERT_RETURN(timeMs < (defaultTtlMs_ * keys.size()), MMC_ERROR);
    return MMC_OK;
}

mmc_location_t MmcClientDefault::GetLocation(const char *key, uint32_t flags) const
{
    GetRequest request{key};
    AllocResponse response;
    MMC_ASSERT_RETURN(metaNetClient_->SyncCall(request, response, rpcTimeOut_) == MMC_OK, {-1});
    return {};
}

Result MmcClientDefault::Remove(const char *key, uint32_t flags) const
{
    RemoveRequest request{key};
    Response response;
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetClient_->SyncCall(request, response, rpcTimeOut_),
                                    "client " << name_ << " remove " << key << " failed");
    return 0;
}

Result MmcClientDefault::BatchRemove(const std::vector<std::string>& keys,
                                     std::vector<Result>& remove_results, uint32_t flags) const
{
    BatchRemoveRequest request{keys};
    BatchRemoveResponse response;

    MMC_LOG_ERROR_AND_RETURN_NOT_OK(
        metaNetClient_->SyncCall(request, response, rpcTimeOut_),
        "client " << name_ << " BatchRemove failed"
    );

    if (response.results_.size() != keys.size()) {
        MMC_LOG_ERROR("BatchRemove response size mismatch. Expected: "
                      << keys.size() << ", Got: " << response.results_.size());
        std::fill(remove_results.begin(), remove_results.end(), MMC_ERROR);
        return MMC_ERROR;
    }

    remove_results = response.results_;
    return MMC_OK;
}

Result MmcClientDefault::IsExist(const std::string &key, bool &result, uint32_t flags) const
{
    if (key.empty()) {
        MMC_LOG_ERROR("Get empty key!");
        return MMC_INVALID_PARAM;
    }

    IsExistRequest request{key};
    Response response;
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetClient_->SyncCall(request, response, rpcTimeOut_),
                                    "client " << name_ << " IsExist " << key << " failed");
    result = response.ret_ == MMC_OK;
    return MMC_OK;
}

Result MmcClientDefault::BatchIsExist(const std::vector<std::string> &keys, std::vector<int32_t> &exist_results, bool &result, uint32_t flags) const
{
    if (keys.empty()) {
        MMC_LOG_ERROR("Get empty keys!");
        return MMC_INVALID_PARAM;
    }

    BatchIsExistRequest request{keys};
    BatchIsExistResponse response;
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetClient_->SyncCall(request, response, rpcTimeOut_),
                                    "client " << name_ << " BatchIsExist failed");

    if (response.results_.size() != keys.size()) {
        MMC_LOG_ERROR("BatchIsExist response size mismatch. Expected: " << keys.size()
                                                                        << ", Got: " << response.results_.size());
        std::fill(exist_results.begin(), exist_results.end(), MMC_ERROR);
        return MMC_ERROR;
    }

    exist_results = response.results_;
    result = response.ret_ == MMC_OK;
    return MMC_OK;
}

Result MmcClientDefault::Query(const std::string &key, mmc_data_info &query_info, uint32_t flags) const
{
    if (key.empty()) {
        MMC_LOG_ERROR("Get empty key!");
        return MMC_INVALID_PARAM;
    }

    QueryRequest request{key};
    QueryResponse response;
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetClient_->SyncCall(request, response, rpcTimeOut_),
                                    "client " << name_ << " Query " << key << " failed");
    query_info.size = response.queryInfo_.size_;
    query_info.prot = response.queryInfo_.prot_;
    query_info.numBlobs = response.queryInfo_.numBlobs_;
    query_info.valid = response.queryInfo_.valid_;
    return MMC_OK;
}

Result MmcClientDefault::BatchQuery(const std::vector<std::string> &keys, std::vector<mmc_data_info> &query_infos, uint32_t flags) const
{
    if (keys.empty()) {
        MMC_LOG_ERROR("Get empty keys!");
        return MMC_INVALID_PARAM;
    }

    BatchQueryRequest request{keys};
    BatchQueryResponse response;
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(metaNetClient_->SyncCall(request, response, rpcTimeOut_),
                                    "client " << name_ << " BatchIsExist failed");

    if (response.batchQueryInfos_.empty()) {
        MMC_LOG_WARN("BatchQuery get empty response.");
    }

    for (const auto& info : response.batchQueryInfos_) {
        query_infos.push_back({info.size_, info.prot_, info.numBlobs_, info.valid_});
    }
    return MMC_OK;
}
}
}