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
    MMC_RETURN_ERROR(ock::mmc::MmcOutLogger::Instance().SetLogLevel(static_cast<LogLevel>(config.logLevel)),
                     "failed to set log level " << config.logLevel);
    if (config.logFunc != nullptr) {
        ock::mmc::MmcOutLogger::Instance().SetExternalLogFunction(config.logFunc);
    }
    bmProxy_ = MmcBmProxyFactory::GetInstance("bmProxyDefault");
    if (config.autoRanking == 1) {
        rankId_ = bmProxy_->RankId();
    } else {
        rankId_ = config.rankId;
    }

    auto tmpNetClient  = MetaNetClientFactory::GetInstance(config.discoveryURL, "MetaClientCommon").Get();
    MMC_ASSERT_RETURN(tmpNetClient != nullptr, MMC_NEW_OBJECT_FAILED);
    if (!tmpNetClient->Status()) {
        NetEngineOptions options;
        options.name = name_;
        options.threadCount = 2;
        options.rankId = config.rankId;
        options.startListener = false;
        options.tlsOption = config.tlsConfig;
        options.logLevel = config.logLevel;
        options.logFunc = config.logFunc;
        MMC_RETURN_ERROR(tmpNetClient->Start(options),
                         "Failed to start net server of local service " << name_);
        MMC_RETURN_ERROR(tmpNetClient->Connect(config.discoveryURL),
                         "Failed to connect net server of local service " << name_);
    }

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
        metaNetClient_ = nullptr;
        MMC_LOG_INFO("MetaNetClient stopped.");
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
    if (buf == nullptr || key == nullptr) {
        MMC_LOG_ERROR("Invalid arguments");
        return MMC_ERROR;
    }
    if (bmProxy_ == nullptr) {
        MMC_LOG_ERROR("BmProxy is null");
        return MMC_ERROR;
    }

    options.mediaType = bmProxy_->GetMediaType();

    uint64_t blobSize = buf->dimType == 0 ? buf->oneDim.len : buf->twoDim.width * buf->twoDim.layerNum;
    uint32_t operateId = operateId_++;
    AllocRequest request{key, {blobSize, 1, options.mediaType, RankId(options.policy), flags}, operateId};
    AllocResponse response;
    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, response, rpcTimeOut_),
                     "client " << name_ << " alloc " << key << " failed");

    if (response.numBlobs_ == 0) {
        MMC_LOG_ERROR("client " << name_ << " alloc " << key << " failed");
        return MMC_ERROR;
    }
    
    for (uint8_t i = 0; i < response.numBlobs_; i++) {
        MMC_LOG_INFO("Attempting to put to blob " << i << " at address " << response.blobs_[i].gva_);
        Result ret = bmProxy_->Put(buf, response.blobs_[i].gva_, blobSize);
        if (ret != MMC_OK) {
            UpdateRequest updateRequest{MMC_WRITE_FAIL, key, rankId_, options.mediaType, operateId};
            Response updateResponse;
            metaNetClient_->SyncCall(updateRequest, updateResponse, rpcTimeOut_);
            MMC_LOG_ERROR("client " << name_ << " put " << key << " failed");
            return MMC_ERROR;
        }
    }

    UpdateRequest updateRequest{MMC_WRITE_OK, key, rankId_, options.mediaType, operateId};
    Response updateResponse;
    MMC_RETURN_ERROR(metaNetClient_->SyncCall(updateRequest, updateResponse, rpcTimeOut_),
                     "client " << name_ << " update " << key << " failed");
    MMC_RETURN_ERROR(updateResponse.ret_, "client " << name_ << " update " << key << " failed");

    return MMC_OK;
}

Result MmcClientDefault::ValidateBatchPutInputs(
    const std::vector<std::string>& keys,
    const std::vector<mmc_buffer>& bufs)
{
    if (keys.empty() || bufs.empty() || keys.size() != bufs.size()) {
        MMC_LOG_ERROR("Invalid arguments");
        return MMC_ERROR;
    }
    return MMC_OK;
}

Result MmcClientDefault::AllocateAndPutBlobs(const std::vector<std::string>& keys,
                                             const std::vector<mmc_buffer>& bufs,
                                             const mmc_put_options& options,
                                             uint32_t flags,
                                             uint32_t operateId,
                                             BatchAllocResponse& allocResponse)
{
    if (bmProxy_ == nullptr) {
        MMC_LOG_ERROR("BmProxy is null");
        return MMC_ERROR;
    }
    
    std::vector<AllocOptions> allocOptionsList;
    for (size_t i = 0; i < keys.size(); ++i) {
        const mmc_buffer& buf = bufs[i];
        uint64_t blobSize = buf.dimType == 0 ? buf.oneDim.len : buf.twoDim.width * buf.twoDim.layerNum;
        allocOptionsList.emplace_back(blobSize, 1, options.mediaType, RankId(options.policy), flags);
    }

    BatchAllocRequest request(keys, allocOptionsList, flags, operateId);
    Result allocResult = metaNetClient_->SyncCall(request, allocResponse, rpcTimeOut_);
    if (allocResult != MMC_OK) {
        MMC_LOG_ERROR("client " << name_ << " batch put alloc failed");
        return MMC_ERROR;
    }

    if (keys.size() != allocResponse.blobs_.size() || keys.size() != allocResponse.numBlobs_.size()) {
        MMC_LOG_ERROR("Mismatch in number of keys and allocated blobs");
        return MMC_ERROR;
    }

    for (size_t i = 0; i < keys.size(); ++i) {
        const std::string& key = keys[i];
        const mmc_buffer& buf = bufs[i];
        const auto& blobs = allocResponse.blobs_[i];
        const auto numBlobs = allocResponse.numBlobs_[i];

        if (numBlobs == 0 || blobs.size() != numBlobs) {
            MMC_LOG_ERROR("Invalid number of blobs for key " << key);
            return MMC_ERROR;
        }

        uint64_t blobSize = buf.dimType == 0 ? buf.oneDim.len : buf.twoDim.width * buf.twoDim.layerNum;
        for (uint8_t j = 0; j < numBlobs; ++j) {
            mmc_buffer bufCopy = buf;
            Result putResult = bmProxy_->Put(&bufCopy, blobs[j].gva_, blobSize);
            if (putResult != MMC_OK) {
                MMC_LOG_ERROR("client " << name_ << " batch put " << key << " failed");
                return MMC_ERROR;
            }
        }
    }
    return MMC_OK;
}

Result MmcClientDefault::BatchPut(const std::vector<std::string>& keys,
                                  const std::vector<mmc_buffer>& bufs,
                                  mmc_put_options& options,
                                  uint32_t flags)
{
    Result validationResult = ValidateBatchPutInputs(keys, bufs);
    if (validationResult != MMC_OK) {
        return validationResult;
    }

    uint32_t operateId = operateId_++;
    BatchAllocResponse allocResponse;

    options.mediaType = bmProxy_->GetMediaType();
    
    Result allocationResult = AllocateAndPutBlobs(keys, bufs, options, flags, operateId, allocResponse);
    if (allocationResult != MMC_OK) {
        return allocationResult;
    }

    std::vector<BlobActionResult> actionResults(keys.size(), MMC_WRITE_OK);
    std::vector<uint32_t> ranks(keys.size(), 0);
    std::vector<uint16_t> mediaTypes(keys.size(), options.mediaType);

    BatchUpdateRequest updateRequest{actionResults, keys, ranks, mediaTypes, operateId};
    BatchUpdateResponse updateResponse;
    Result updateResult = metaNetClient_->SyncCall(updateRequest, updateResponse, rpcTimeOut_);
    if (updateResult != MMC_OK) {
        MMC_LOG_ERROR("client " << name_ << " batch put update failed");
        return MMC_ERROR;
    }

    if (updateResponse.results_.size() != keys.size()) {
        MMC_LOG_ERROR("Mismatch in number of update results");
        return MMC_ERROR;
    }

    for (size_t i = 0; i < updateResponse.results_.size(); ++i) {
        if (updateResponse.results_[i] != MMC_OK) {
            MMC_LOG_ERROR("client " << name_ << " batch put update for key " << keys[i] << " failed");
            return MMC_ERROR;
        }
    }

    return MMC_OK;
}

Result MmcClientDefault::Get(const char *key, mmc_buffer *buf, uint32_t flags)
{
    uint32_t operateId = operateId_++;
    GetRequest request{key, rankId_, operateId, true};
    AllocResponse response;
    uint64_t startTime = ock::dagger::Monotonic::TimeUs();
    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, response, rpcTimeOut_),
                     "client " << name_ << " get " << key << " failed");
    if (response.numBlobs_ == 0) {
        MMC_LOG_ERROR("client " << name_ << " get " << key << " failed");
        return MMC_ERROR;
    }

    MMC_RETURN_ERROR(bmProxy_->Get(buf, response.blobs_[0].gva_, response.blobs_[0].size_),
                     "client " << name_ << " get " << key << " failed");
    UpdateRequest updateRequest{MMC_READ_OK, key, rankId_, bmProxy_->GetMediaType(), operateId};
    Response updateResponse;
    if(metaNetClient_->SyncCall(updateRequest, updateResponse, rpcTimeOut_ != MMC_OK)) {
        MMC_LOG_WARN("client" << name_ << " update " << key << " failed");
    } else if(updateResponse.ret_ != MMC_OK) {
        MMC_LOG_WARN("client" << name_ << " update " << key << " failed");
    }
    uint64_t timeMs = (ock::dagger::Monotonic::TimeUs() - startTime) / 1000U;
    MMC_ASSERT_RETURN(timeMs < defaultTtlMs_, MMC_ERROR);

    return MMC_OK;
}

Result MmcClientDefault::BatchGet(const std::vector<std::string> &keys, std::vector<mmc_buffer> &bufs, uint32_t flags)
{
    uint32_t operateId = operateId_++;
    BatchGetRequest request{keys, rankId_, operateId};
    BatchAllocResponse response;
    uint64_t startTime = ock::dagger::Monotonic::TimeUs();
    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, response, rpcTimeOut_),
        "client " << name_ << " batch get failed");

    if (response.blobs_.size() != keys.size() || response.numBlobs_.size() != keys.size()) {
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

            MMC_RETURN_ERROR(bmProxy_->Get(&bufs[i], blobs[0].gva_, blobs[0].size_),
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

mmc_location_t MmcClientDefault::GetLocation(const char *key, uint32_t flags)
{
    uint32_t operateId = operateId_++;
    GetRequest request{key, rankId_, operateId, false};
    AllocResponse response;
    MMC_ASSERT_RETURN(metaNetClient_->SyncCall(request, response, rpcTimeOut_) == MMC_OK, {-1});
    return {};
}

Result MmcClientDefault::Remove(const char *key, uint32_t flags) const
{
    RemoveRequest request{key};
    Response response;
    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, response, rpcTimeOut_),
                     "client " << name_ << " remove " << key << " failed");
    return response.ret_;
}

Result MmcClientDefault::BatchRemove(const std::vector<std::string>& keys,
                                     std::vector<Result>& remove_results, uint32_t flags) const
{
    BatchRemoveRequest request{keys};
    BatchRemoveResponse response;

    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, response, rpcTimeOut_),
        "client " << name_ << " BatchRemove failed");

    if (response.results_.size() != keys.size()) {
        MMC_LOG_ERROR("BatchRemove response size mismatch. Expected: "
                      << keys.size() << ", Got: " << response.results_.size());
        std::fill(remove_results.begin(), remove_results.end(), MMC_ERROR);
        return MMC_ERROR;
    }

    remove_results = response.results_;
    return MMC_OK;
}

Result MmcClientDefault::IsExist(const std::string &key, uint32_t flags) const
{
    if (key.empty()) {
        MMC_LOG_ERROR("Get empty key!");
        return MMC_INVALID_PARAM;
    }

    IsExistRequest request{key};
    Response response;
    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, response, rpcTimeOut_),
                     "client " << name_ << " IsExist " << key << " failed");
    return response.ret_;
}

Result MmcClientDefault::BatchIsExist(const std::vector<std::string> &keys, std::vector<int32_t> &exist_results, uint32_t flags) const
{
    if (keys.empty()) {
        MMC_LOG_ERROR("Get empty keys!");
        return MMC_INVALID_PARAM;
    }

    BatchIsExistRequest request{keys};
    BatchIsExistResponse response;
    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, response, rpcTimeOut_),
                     "client " << name_ << " BatchIsExist failed");

    if (response.results_.size() != keys.size()) {
        MMC_LOG_ERROR("BatchIsExist response size mismatch. Expected: " << keys.size()
                                                                        << ", Got: " << response.results_.size());
        std::fill(exist_results.begin(), exist_results.end(), MMC_ERROR);
        return MMC_ERROR;
    }

    exist_results = response.results_;
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
    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, response, rpcTimeOut_),
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
    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, response, rpcTimeOut_),
                     "client " << name_ << " BatchIsExist failed");

    if (response.batchQueryInfos_.size() != keys.size()) {
        MMC_LOG_ERROR("BatchQuery get a response with mismatched size (" << response.batchQueryInfos_.size()
                      << "), should get size (" << keys.size() << ").");
        MemObjQueryInfo info_fill;
        query_infos.resize(keys.size(), {info_fill.size_, info_fill.prot_, info_fill.numBlobs_, info_fill.valid_});
        return MMC_ERROR;
    }

    for (const auto& info : response.batchQueryInfos_) {
        query_infos.push_back({info.size_, info.prot_, info.numBlobs_, info.valid_});
    }
    return MMC_OK;
}
}
}