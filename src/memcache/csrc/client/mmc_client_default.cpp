/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "mmc_client_default.h"
#include "mmc_msg_client_meta.h"
#include "mmc_mem_obj_meta.h"
#include "mmc_bm_proxy.h"
#include "mmc_montotonic.h"
#include "mmc_ptracer.h"

namespace ock {
namespace mmc {
constexpr int CLIENT_THREAD_COUNT = 2;
constexpr uint32_t MMC_REGISTER_SET_MARK_BIT = 1U;
constexpr uint32_t MMC_REGISTER_SET_LEFT_MARK = 1U;
constexpr int32_t MMC_BATCH_TRANSPORT = 1U;
constexpr int32_t MMC_ASYNC_TRANSPORT = 2U;
constexpr uint32_t KEY_MAX_LENTH = 256U;
MmcClientDefault* MmcClientDefault::gClientHandler = nullptr;
std::mutex MmcClientDefault::gClientHandlerMtx;

Result MmcClientDefault::Start(const mmc_client_config_t &config)
{
    MMC_LOG_INFO("Starting client " << name_);
    std::lock_guard<std::mutex> guard(mutex_);
    if (started_) {
        MMC_LOG_INFO("MetaService " << name_ << " already started");
        return MMC_OK;
    }
    bmProxy_ = MmcBmProxyFactory::GetInstance("bmProxyDefault");
    MMC_ASSERT_RETURN(bmProxy_ != nullptr, MMC_MALLOC_FAILED);
    rankId_ = bmProxy_->RankId();

    threadPool_ = MmcMakeRef<MmcThreadPool>("client_pool", 1);
    MMC_ASSERT_RETURN(threadPool_ != nullptr, MMC_MALLOC_FAILED);
    MMC_RETURN_ERROR(threadPool_->Start(), "thread pool start failed");
    MMC_ASSERT_RETURN(memchr(config.discoveryURL, '\0', DISCOVERY_URL_SIZE) != nullptr, MMC_INVALID_PARAM);
    auto tmpNetClient  = MetaNetClientFactory::GetInstance(config.discoveryURL, "MetaClientCommon").Get();
    MMC_ASSERT_RETURN(tmpNetClient != nullptr, MMC_NEW_OBJECT_FAILED);
    if (!tmpNetClient->Status()) {
        NetEngineOptions options;
        options.name = name_;
        options.threadCount = CLIENT_THREAD_COUNT;
        options.rankId = rankId_;
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
    rpcRetryTimeOut_ = config.rpcRetryTimeOut;
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
    if (threadPool_ != nullptr) {
        threadPool_->Shutdown();
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

Result MmcClientDefault::Put(const char* key, mmc_buffer* buf, mmc_put_options& options, uint32_t flags)
{
    MMC_VALIDATE_RETURN(bmProxy_ != nullptr, "BmProxy is null", MMC_CLIENT_NOT_INIT);
    MMC_VALIDATE_RETURN(metaNetClient_ != nullptr, "MetaNetClient is null", MMC_CLIENT_NOT_INIT);

    if (buf == nullptr || key == nullptr || key[0] == '\0' ||
        strnlen(key, KEY_MAX_LENTH + 1) == KEY_MAX_LENTH + 1) {
        MMC_LOG_ERROR("Invalid arguments");
        return MMC_ERROR;
    }

    MmcBufferArray buffArr{};
    buffArr.AddBuffer(*buf);
    return Put(key, buffArr, options, flags);
}

Result MmcClientDefault::PrePutHandle(const MmcBufferArray &bufArr, mmc_put_options &options, AllocRequest &request,
                                      uint32_t flags)
{
    MMC_VALIDATE_RETURN(bmProxy_ != nullptr, "BmProxy is null", MMC_CLIENT_NOT_INIT);
    MMC_VALIDATE_RETURN(metaNetClient_ != nullptr, "MetaNetClient is null", MMC_CLIENT_NOT_INIT);
    options.mediaType = bmProxy_->GetMediaType();
    AllocOptions prot = {bufArr.TotalSize(), 1, options.mediaType, {RankId(options.policy)}, flags};
    if (options.replicaNum > 0) {
        prot.numBlobs_ = options.replicaNum;
    }
    std::vector<uint32_t> preferredRank;
    std::copy_if(std::begin(options.preferredLocalServiceIDs), std::end(options.preferredLocalServiceIDs),
                 std::back_inserter(preferredRank), [](const int32_t x) { return x >= 0; });
    if (!preferredRank.empty()) {
        prot.flags_ = ALLOC_FORCE_BY_RANK;
        prot.preferredRank_ = preferredRank;
    }
    request.options_ = prot;
    return MMC_OK;
}

Result MmcClientDefault::Put(const std::string &key, const MmcBufferArray &bufArr, mmc_put_options &options,
                             uint32_t flags)
{
    uint64_t operateId = GenerateOperateId(rankId_);
    AllocRequest request{key, {}, operateId};
    MMC_VALIDATE_RETURN(PrePutHandle(bufArr, options, request, flags) == MMC_OK, "put error", MMC_ERROR);
    AllocResponse response;
    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, response, rpcRetryTimeOut_),
                     "client " << name_ << " alloc " << key << " failed");
    if (response.result_ == MMC_DUPLICATED_OBJECT) {
        return response.result_;
    }
    MMC_RETURN_ERROR(response.result_, "client " << name_ << " alloc " << key << " failed");
    if (response.numBlobs_ == 0 || response.numBlobs_ != response.blobs_.size()) {
        MMC_LOG_ERROR("client " << name_ << " alloc " << key << " failed, blobs size:" << response.blobs_.size()
                                << ", numBlob:" << response.numBlobs_);
        return MMC_ERROR;
    }
    MMC_ASSERT_RETURN(!bufArr.Buffers().empty(), MMC_ERROR);
    auto transType = SelectTransportType(bufArr.Buffers()[0]);
    for (uint8_t i = 0; i < response.numBlobs_; i++) {
        auto blob = response.blobs_[i];
        MMC_LOG_DEBUG("Attempting to put to blob " << static_cast<int>(i) << " key " << key);
        auto ret = transType == MMC_ASYNC_TRANSPORT ?
            bmProxy_->Put(bufArr, blob) : bmProxy_->BatchPut(bufArr, blob);
        if (ret == MMC_OK && transType == MMC_ASYNC_TRANSPORT) {
            ret = bmProxy_->CopyWait();
            if (ret != MMC_OK) {
                MMC_LOG_ERROR("Failed to wait copy task, ret: " << ret);
            }
        }
        if (ret != MMC_OK) {
            UpdateRequest updateRequest{MMC_WRITE_FAIL, key, blob.rank_, blob.mediaType_, operateId};
            Response updateResponse;
            metaNetClient_->SyncCall(updateRequest, updateResponse, rpcRetryTimeOut_);
            MMC_LOG_ERROR("client " << name_ << " put " << key << " blob rank:" << blob.rank_
                                    << ", media:" << blob.mediaType_ << " failed");
            return MMC_ERROR;
        } else {
            UpdateRequest updateRequest{MMC_WRITE_OK, key, blob.rank_, blob.mediaType_, operateId};
            Response updateResponse;
            ret = metaNetClient_->SyncCall(updateRequest, updateResponse, rpcRetryTimeOut_);
            if (ret != MMC_OK || updateResponse.ret_ != MMC_OK) {
                MMC_LOG_ERROR("client " << name_ << " update " << key << " blob rank:" << blob.rank_ << ", media:"
                                        << blob.mediaType_ << " failed:" << ret << ", " << updateResponse.ret_);
            }
        }
    }
    return MMC_OK;
}

Result MmcClientDefault::BatchPut(const std::vector<std::string>& keys, const std::vector<mmc_buffer>& bufs,
                                  mmc_put_options& options, uint32_t flags, std::vector<int>& batchResult)
{
    MMC_VALIDATE_RETURN(bmProxy_ != nullptr, "BmProxy is null", MMC_CLIENT_NOT_INIT);
    MMC_VALIDATE_RETURN(metaNetClient_ != nullptr, "MetaNetClient is null", MMC_CLIENT_NOT_INIT);

    if (keys.empty() || bufs.empty() || keys.size() != bufs.size()) {
        MMC_LOG_ERROR("client " << name_ << " batch get failed: keys size:" << keys.size()
                                << ", bufs size:" << bufs.size());
        return MMC_INVALID_PARAM;
    }

    std::vector<MmcBufferArray> bufferArrays{};
    bufferArrays.reserve(bufs.size());
    for (const auto& buf : bufs) {
        MmcBufferArray bufArr{};
        bufArr.AddBuffer(buf);
        bufferArrays.emplace_back(bufArr);
    }

    return BatchPut(keys, bufferArrays, options, flags, batchResult);
}

Result MmcClientDefault::BatchPut(const std::vector<std::string>& keys, const std::vector<MmcBufferArray>& bufArrs,
                                  mmc_put_options& options, uint32_t flags, std::vector<int>& batchResult)
{
    MMC_VALIDATE_RETURN(bmProxy_ != nullptr, "BmProxy is null", MMC_CLIENT_NOT_INIT);
    MMC_VALIDATE_RETURN(metaNetClient_ != nullptr, "MetaNetClient is null", MMC_CLIENT_NOT_INIT);

    if (keys.empty() || bufArrs.empty() || keys.size() != bufArrs.size()) {
        MMC_LOG_ERROR("client " << name_ << " batch get failed: keys size:" << keys.size() << ", bufArrs size:"
                                << bufArrs.size());
        return MMC_INVALID_PARAM;
    }

    options.mediaType = bmProxy_->GetMediaType();
    uint64_t operateId = GenerateOperateId(rankId_);
    batchResult.resize(keys.size(), MMC_ERROR);

    BatchAllocResponse allocResponse;
    MMC_RETURN_ERROR(AllocateAndPutBlobs(keys, bufArrs, options, flags, operateId, batchResult, allocResponse),
        "client " << name_ << " allocate and put blobs failed");

    // update blob state
    std::vector<BlobActionResult> actionResults;
    std::vector<uint32_t> ranks;
    std::vector<uint16_t> mediaTypes;
    std::vector<std::string> updateKeys;
    for (size_t i = 0; i < keys.size(); ++i) {
        for (const auto& blob : allocResponse.blobs_[i]) {
            updateKeys.push_back(keys[i]);
            ranks.push_back(blob.rank_);
            mediaTypes.push_back(blob.mediaType_);
            actionResults.push_back(batchResult[i] == 0 ? MMC_WRITE_OK : MMC_WRITE_FAIL);
        }
    }

    BatchUpdateRequest updateRequest{actionResults, updateKeys, ranks, mediaTypes, operateId};
    BatchUpdateResponse updateResponse;
    Result updateResult = metaNetClient_->SyncCall(updateRequest, updateResponse, rpcRetryTimeOut_);
    if (updateResult != MMC_OK || updateResponse.results_.size() != updateKeys.size()) {
        MMC_LOG_ERROR("client " << name_ << " batch put update failed:" << updateResult << ", key size:"
                                << updateKeys.size() << ", ret size:" << updateResponse.results_.size());
    } else {
        for (size_t i = 0; i < updateKeys.size(); ++i) {
            if (updateResponse.results_[i] != MMC_OK) {
                MMC_LOG_ERROR("client " << name_ << " batch put update for key " << updateKeys[i]
                                        << " failed:" << updateResponse.results_[i]);
            }
        }
    }
    return MMC_OK;
}

Result MmcClientDefault::Get(const char* key, mmc_buffer* buf, uint32_t flags)
{
    if (buf == nullptr || key == nullptr || key[0] == '\0' ||
        strnlen(key, KEY_MAX_LENTH + 1) == KEY_MAX_LENTH + 1) {
        MMC_LOG_ERROR("Invalid arguments");
        return MMC_ERROR;
    }

    MmcBufferArray bufArr{};
    bufArr.AddBuffer(*buf);
    return Get(key, bufArr, flags);
}

Result MmcClientDefault::Get(const std::string &key, const MmcBufferArray& bufArr, uint32_t flags)
{
    MMC_VALIDATE_RETURN(bmProxy_ != nullptr, "BmProxy is null", MMC_CLIENT_NOT_INIT);
    MMC_VALIDATE_RETURN(metaNetClient_ != nullptr, "MetaNetClient is null", MMC_CLIENT_NOT_INIT);

    uint64_t operateId = GenerateOperateId(rankId_);
    GetRequest request{key, rankId_, operateId, true};
    AllocResponse response;
    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, response, rpcRetryTimeOut_),
                     "client " << name_ << " get " << key << " failed");
    if (response.numBlobs_ == 0 || response.blobs_.empty()) {
        MMC_LOG_ERROR("client " << name_ << " get " << key << " failed, numblob is:"
                      << static_cast<uint64_t>(response.numBlobs_));
        return MMC_ERROR;
    }
    auto& blob = response.blobs_[0];
    auto transType = SelectTransportType(bufArr.Buffers()[0]);
    auto ret = transType == MMC_ASYNC_TRANSPORT ? bmProxy_->Get(bufArr, blob) :
        bmProxy_->BatchGet(bufArr, blob);
    if (ret == MMC_OK && transType == MMC_ASYNC_TRANSPORT) {
        ret = bmProxy_->CopyWait();
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("Failed to wait copy task, ret: " << ret);
        }
    }

    auto future = threadPool_->Enqueue(
        [&](const std::string keyL, uint32_t rankL, uint16_t mediaTypeL, uint64_t operateIdL) {
            UpdateRequest updateRequest{MMC_READ_FINISH, keyL, rankL, mediaTypeL, operateIdL};
            Response updateResponse;
            auto updateRet = metaNetClient_->SyncCall(updateRequest, updateResponse, rpcRetryTimeOut_);
            if (updateRet != MMC_OK || updateResponse.ret_ != MMC_OK) {
                MMC_LOG_WARN("client" << name_ << " update " << key << " update state failed, ret:" << updateRet << ", "
                                      << updateResponse.ret_);
            }
        },
        key, blob.rank_, blob.mediaType_, operateId);
    if (!future.valid()) {
        MMC_LOG_WARN("get update enqueue failed");
    }

    if (ret != MMC_OK) {
        MMC_LOG_ERROR("client " << name_ << " get " << key << " read data failed.");
        return ret;
    }
    return MMC_OK;
}

Result MmcClientDefault::BatchGet(const std::vector<std::string>& keys, std::vector<mmc_buffer>& bufs, uint32_t flags,
                                  std::vector<int>& batchResult)
{
    MMC_VALIDATE_RETURN(bmProxy_ != nullptr, "BmProxy is null", MMC_CLIENT_NOT_INIT);
    MMC_VALIDATE_RETURN(metaNetClient_ != nullptr, "MetaNetClient is null", MMC_CLIENT_NOT_INIT);
    if ((keys.empty() || bufs.empty() || keys.size() != bufs.size())) {
        MMC_LOG_ERROR("client " << name_ << " batch get failed: keys size:" << keys.size()
                                << ", bufArrs size:" << bufs.size());
        return MMC_INVALID_PARAM;
    }

    std::vector<MmcBufferArray> bufferArrays{};
    bufferArrays.reserve(bufs.size());
    for (const auto& buf : bufs) {
        MmcBufferArray bufArr{};
        bufArr.AddBuffer(buf);
        bufferArrays.emplace_back(bufArr);
    }
    return BatchGet(keys, bufferArrays, flags, batchResult);
}

Result MmcClientDefault::BatchGet(const std::vector<std::string>& keys, const std::vector<MmcBufferArray>& bufArrs,
                                  uint32_t flags, std::vector<int>& batchResult)
{
    MMC_VALIDATE_RETURN(bmProxy_ != nullptr, "BmProxy is null", MMC_CLIENT_NOT_INIT);
    MMC_VALIDATE_RETURN(metaNetClient_ != nullptr, "MetaNetClient is null", MMC_CLIENT_NOT_INIT);

    if ((keys.empty() || bufArrs.empty() || keys.size() != bufArrs.size())) {
        MMC_LOG_ERROR("client " << name_ << " batch get failed: keys size:" << keys.size() << ", bufArrs size:"
                                << bufArrs.size());
        return MMC_INVALID_PARAM;
    }

    batchResult.resize(keys.size(), MMC_ERROR);

    const uint64_t operateId = GenerateOperateId(rankId_);
    BatchGetRequest request{keys, rankId_, operateId};
    BatchAllocResponse response;
    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, response, rpcRetryTimeOut_),
                     "client " << name_ << " batch get failed");

    if (response.blobs_.size() != keys.size() || response.numBlobs_.size() != keys.size()) {
        MMC_LOG_ERROR("client " << name_ << " batch get response size mismatch: expected " << keys.size() << ", got "
                                << response.blobs_.size());
        return MMC_ERROR;
    }

    std::vector<BlobActionResult> actionResults;
    std::vector<uint32_t> ranks;
    std::vector<uint16_t> mediaTypes;

    std::vector<void *> sources;
    std::vector<void *> destinations;
    std::vector<uint64_t> dataSizes;
    std::vector<uint64_t> indexs;
    MediaType mediaType = MEDIA_NONE;

    MMC_ASSERT_RETURN(!bufArrs[0].Buffers().empty(), MMC_INVALID_PARAM);
    batchResult.resize(keys.size(), MMC_ERROR);
    for (size_t i = 0; i < keys.size(); ++i) {
        const auto &blobs = response.blobs_[i];
        uint8_t numBlobs = response.numBlobs_[i];
        actionResults.push_back(MMC_READ_FINISH);

        if (numBlobs <= 0 || blobs.empty() || blobs.size() != numBlobs) {
            MMC_LOG_ERROR("client " << name_ << " batch get failed for key " << keys[i] << ", blob:" << numBlobs
                                    << ", size:" << blobs.size());
            batchResult[i] = MMC_ERROR;
            ranks.push_back(UINT32_MAX);
            mediaTypes.push_back(MEDIA_NONE);
            continue;
        }
        ranks.push_back(blobs[0].rank_);
        mediaTypes.push_back(blobs[0].mediaType_);

        batchResult[i] = MMC_OK;
        indexs.push_back(i);
        const MmcBufferArray &bufArr = bufArrs[i];
        uint64_t shift = 0;
        uint32_t count = bufArr.Buffers().size();
        for (size_t k = 0; k < count; ++k) {
            auto buf = &bufArr.Buffers()[k];
            if (buf->type == MEDIA_NONE || buf->dimType != 0) {
                MMC_LOG_ERROR("unexcepted buf type:" << buf->type << " or dim:" << buf->dimType);
                return MMC_ERROR;
            }
            if (mediaType == MEDIA_NONE) {
                mediaType = static_cast<MediaType>(buf->type);
            } else if (mediaType != buf->type) {
                MMC_LOG_ERROR("not all data type same as " << mediaType << ", unexcepted buf type:" << buf->type);
                return MMC_ERROR;
            }
            sources.push_back(reinterpret_cast<void *>(blobs[0].gva_ + shift));
            destinations.push_back(reinterpret_cast<void *>(buf->addr + buf->oneDim.offset));
            dataSizes.push_back(buf->oneDim.len);
            shift += MmcBufSize(*buf);
        }
    }
    auto ret = bmProxy_->BatchDataGet(sources, destinations, dataSizes, mediaType);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("Failed to wait copy task ret: " << ret);
        for (auto idx : indexs) {
            batchResult[idx] = ret;
        }
    }

    auto future = threadPool_->Enqueue(
        [&](std::vector<BlobActionResult> actionResultsL, const std::vector<std::string> keysL,
            std::vector<uint32_t> ranksL, std::vector<uint16_t> mediaTypesL, uint64_t operateIdL) {
            BatchUpdateRequest updateRequest{actionResultsL, keysL, ranksL, mediaTypesL, operateIdL};
            BatchUpdateResponse updateResponse;
            Result updateResult = metaNetClient_->SyncCall(updateRequest, updateResponse, rpcRetryTimeOut_);
            if (updateResult != MMC_OK || updateResponse.results_.size() != keysL.size()) {
                MMC_LOG_ERROR("client " << name_ << " batch get update failed:" << updateResult << ", key size:"
                                        << keysL.size() << ", ret size:" << updateResponse.results_.size());
            } else {
                for (size_t i = 0; i < keysL.size(); ++i) {
                    if (updateResponse.results_[i] != MMC_OK) {
                        MMC_LOG_ERROR("client " << name_ << " batch put update for key " << keysL[i]
                                                << " failed:" << updateResponse.results_[i]);
                    }
                }
            }
        },
        actionResults, keys, ranks, mediaTypes, operateId);
    if (!future.valid()) {
        MMC_LOG_WARN("get batch update enqueue failed");
    }
    return ret;
}

mmc_location_t MmcClientDefault::GetLocation(const char *key, uint32_t flags)
{
    MMC_VALIDATE_RETURN(metaNetClient_ != nullptr, "MetaNetClient is null", {});

    uint64_t operateId = GenerateOperateId(rankId_);
    GetRequest request{key, rankId_, operateId, false};
    AllocResponse response;
    MMC_ASSERT_RETURN(metaNetClient_->SyncCall(request, response, rpcRetryTimeOut_) == MMC_OK, {-1});
    return {};
}

Result MmcClientDefault::Remove(const char *key, uint32_t flags) const
{
    MMC_VALIDATE_RETURN(metaNetClient_ != nullptr, "MetaNetClient is null", MMC_CLIENT_NOT_INIT);

    RemoveRequest request{key};
    Response response;
    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, response, rpcRetryTimeOut_),
                     "client " << name_ << " remove " << key << " failed");
    return response.ret_;
}

Result MmcClientDefault::BatchRemove(const std::vector<std::string>& keys,
                                     std::vector<Result>& remove_results, uint32_t flags) const
{
    MMC_VALIDATE_RETURN(metaNetClient_ != nullptr, "MetaNetClient is null", MMC_CLIENT_NOT_INIT);

    BatchRemoveRequest request{keys};
    BatchRemoveResponse response;

    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, response, rpcRetryTimeOut_),
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
    MMC_VALIDATE_RETURN(metaNetClient_ != nullptr, "MetaNetClient is null", MMC_CLIENT_NOT_INIT);

    if (key.empty()) {
        MMC_LOG_ERROR("Get empty key!");
        return MMC_INVALID_PARAM;
    }

    IsExistRequest request{key};
    Response response;
    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, response, rpcRetryTimeOut_),
                     "client " << name_ << " IsExist " << key << " failed");
    return response.ret_;
}

Result MmcClientDefault::BatchIsExist(const std::vector<std::string> &keys, std::vector<int32_t> &exist_results,
                                      uint32_t flags) const
{
    MMC_VALIDATE_RETURN(metaNetClient_ != nullptr, "MetaNetClient is null", MMC_CLIENT_NOT_INIT);

    if (keys.empty()) {
        MMC_LOG_ERROR("Get empty keys!");
        return MMC_INVALID_PARAM;
    }

    BatchIsExistRequest request{keys};
    BatchIsExistResponse response;
    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, response, rpcRetryTimeOut_),
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

Result MmcClientDefault::Query(const std::string& key, mmc_data_info& query_info, uint32_t flags) const
{
    MMC_VALIDATE_RETURN(metaNetClient_ != nullptr, "MetaNetClient is null", MMC_CLIENT_NOT_INIT);

    if (key.empty()) {
        MMC_LOG_ERROR("Get empty key!");
        return MMC_INVALID_PARAM;
    }

    QueryRequest request{key};
    QueryResponse response;
    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, response, rpcRetryTimeOut_),
                     "client " << name_ << " Query " << key << " failed");
    query_info.size = response.queryInfo_.size_;
    query_info.prot = response.queryInfo_.prot_;
    query_info.numBlobs =
        response.queryInfo_.numBlobs_ > MAX_BLOB_COPIES ? MAX_BLOB_COPIES : response.queryInfo_.numBlobs_;
    query_info.valid = response.queryInfo_.valid_;
    for (int i = 0; i < query_info.numBlobs && i < MAX_BLOB_COPIES; i++) {
        query_info.ranks[i] = response.queryInfo_.blobRanks_[i];
        query_info.types[i] = response.queryInfo_.blobTypes_[i];
    }
    return MMC_OK;
}

Result MmcClientDefault::BatchQuery(const std::vector<std::string> &keys, std::vector<mmc_data_info> &query_infos,
                                    uint32_t flags) const
{
    MMC_VALIDATE_RETURN(metaNetClient_ != nullptr, "MetaNetClient is null", MMC_CLIENT_NOT_INIT);

    if (keys.empty()) {
        MMC_LOG_ERROR("Get empty keys!");
        return MMC_INVALID_PARAM;
    }

    BatchQueryRequest request{keys};
    BatchQueryResponse response;
    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, response, rpcRetryTimeOut_),
                     "client " << name_ << " BatchIsExist failed");

    if (response.batchQueryInfos_.size() != keys.size()) {
        MMC_LOG_ERROR("BatchQuery get a response with mismatched size (" << response.batchQueryInfos_.size()
                      << "), should get size (" << keys.size() << ").");
        MemObjQueryInfo info_fill;
        query_infos.resize(keys.size(), {});
        return MMC_ERROR;
    }

    for (const auto& info : response.batchQueryInfos_) {
        mmc_data_info outInfo{};
        outInfo.valid = info.valid_;
        if (!outInfo.valid) {
            query_infos.push_back(outInfo);
            continue;
        }

        for (int i = 0; i < info.numBlobs_ && i < MAX_BLOB_COPIES; i++) {
            outInfo.ranks[i] = info.blobRanks_[i];
            outInfo.types[i] = info.blobTypes_[i];
        }
        outInfo.size = info.size_;
        outInfo.prot = info.prot_;
        outInfo.numBlobs = info.numBlobs_;
        query_infos.push_back(outInfo);
    }
    return MMC_OK;
}

Result MmcClientDefault::AllocateAndPutBlobs(const std::vector<std::string>& keys,
    const std::vector<MmcBufferArray>& bufArrs, const mmc_put_options& options, uint32_t flags, uint64_t operateId,
    std::vector<int>& batchResult, BatchAllocResponse& allocResponse)
{
    std::vector<uint32_t> preferredRank;
    std::copy_if(std::begin(options.preferredLocalServiceIDs), std::end(options.preferredLocalServiceIDs),
                 std::back_inserter(preferredRank), [](const int32_t x) { return x >= 0; });
    std::vector<AllocOptions> allocOptionsList;
    auto flag = !preferredRank.empty() ? static_cast<uint32_t>(ALLOC_FORCE_BY_RANK) : flags;
    uint16_t numBlobs = std::max<uint16_t>(options.replicaNum, 1u);
    for (const auto &bufArr : bufArrs) {
        AllocOptions tmpAllocOptions = {
            bufArr.TotalSize(), numBlobs, options.mediaType, {RankId(options.policy)}, flag};
        if (!preferredRank.empty()) {
            tmpAllocOptions.preferredRank_ = preferredRank;
        }
        allocOptionsList.emplace_back(tmpAllocOptions);
    }
    BatchAllocRequest request(keys, allocOptionsList, flag, operateId);
    MMC_RETURN_ERROR(metaNetClient_->SyncCall(request, allocResponse, rpcRetryTimeOut_), "batch put alloc failed");

    if (keys.size() != allocResponse.blobs_.size() || keys.size() != allocResponse.numBlobs_.size() ||
        keys.size() != allocResponse.results_.size()) {
        MMC_LOG_ERROR("Mismatch in number of keys and allocated blobs");
        return MMC_ERROR;
    }
    MMC_ASSERT_RETURN(!bufArrs.empty() && !bufArrs[0].Buffers().empty(), MMC_INVALID_PARAM);

    std::vector<void *> sources;
    std::vector<void *> destinations;
    std::vector<uint64_t> dataSizes;
    std::vector<uint64_t> indexs;
    MediaType mediaType = MEDIA_NONE;

    for (size_t i = 0; i < keys.size(); ++i) {
        const std::string& key = keys[i];
        const MmcBufferArray& bufArr = bufArrs[i];
        const auto& blobs = allocResponse.blobs_[i];
        const auto numBlobs = allocResponse.numBlobs_[i];

        if (allocResponse.results_[i] != MMC_OK) {
            // alloc has error, reserve alloc error code
            if (allocResponse.results_[i] != MMC_DUPLICATED_OBJECT) {
                MMC_LOG_ERROR("Alloc blob failed for key " << key << ", error code=" << allocResponse.results_[i]);
            }
            batchResult[i] = allocResponse.results_[i];
            continue;
        } else if (numBlobs == 0 || blobs.size() != numBlobs) {
            MMC_LOG_ERROR("Invalid number of blobs for key " << key);
            batchResult[i] = MMC_ERROR;
            continue;
        }

        batchResult[i] = MMC_OK;
        indexs.push_back(i);
        for (uint8_t j = 0; j < numBlobs; ++j) {
            uint64_t shift = 0;
            uint32_t count = bufArr.Buffers().size();
            for (size_t k = 0; k < count; ++k) {
                auto buf = &bufArr.Buffers()[k];
                if (buf->type == MEDIA_NONE || buf->dimType != 0) {
                    MMC_LOG_ERROR("unexcepted buf type:" << buf->type << " or dim:" << buf->dimType);
                    return MMC_ERROR;
                }
                if (mediaType == MEDIA_NONE) {
                    mediaType = static_cast<MediaType>(buf->type);
                } else if (mediaType != buf->type) {
                    MMC_LOG_ERROR("not all data type same as " << mediaType << ", unexcepted buf type:" << buf->type);
                    return MMC_ERROR;
                }
                sources.push_back(reinterpret_cast<void *>(buf->addr + buf->oneDim.offset));
                destinations.push_back(reinterpret_cast<void *>(blobs[j].gva_ + shift));
                dataSizes.push_back(buf->oneDim.len);
                shift += MmcBufSize(*buf);
            }
        }
    }
    auto ret = bmProxy_->BatchDataPut(sources, destinations, dataSizes, mediaType);
    if (ret != MMC_OK) {
        MMC_LOG_ERROR("Failed to wait copy task ret: " << ret);
        for (auto idx : indexs) {
            batchResult[idx] = ret;
        }
    }
    return ret;
}

Result MmcClientDefault::RegisterBuffer(uint64_t addr, uint64_t size)
{
    auto ret = bmProxy_->RegisterBuffer(addr, size);
    if (ret == MMC_OK) {
        UpdateRegisterMap(addr, size);
    }
    return ret;
}

Result MmcClientDefault::RegisterLayerBuffer(const uint64_t *addrs, const uint64_t *sizes, uint64_t layer, uint64_t num)
{
    auto ret = bmProxy_->RegisterLayerBuffer(addrs, sizes, layer, num);
    if (ret == MMC_OK) {
        for (uint64_t i = 0; i < layer; i++) {
            UpdateRegisterMap(addrs[i], sizes[i]);
        }
    }
    return ret;
}

void MmcClientDefault::UpdateRegisterMap(uint64_t va, uint64_t size)
{
    if (va > std::numeric_limits<uint64_t>::max() - size) {
        return;
    }
    registerSet_.insert((va << MMC_REGISTER_SET_MARK_BIT) | MMC_REGISTER_SET_LEFT_MARK);
    registerSet_.insert((va + size) << MMC_REGISTER_SET_MARK_BIT);
}

bool MmcClientDefault::QueryInRegisterMap(uint64_t va, uint64_t size)
{
    if (va > std::numeric_limits<uint64_t>::max() / 2u) {
        MMC_LOG_ERROR("va is " << va);
        return false;
    }
    auto it = registerSet_.upper_bound((va << MMC_REGISTER_SET_MARK_BIT) | MMC_REGISTER_SET_LEFT_MARK);
    return (it != registerSet_.end() && !((*it) & MMC_REGISTER_SET_LEFT_MARK) &&
            (va + size) <= ((*it) >> MMC_REGISTER_SET_MARK_BIT));
}

bool MmcClientDefault::QueryInRegisterMap(const mmc_buffer &buf)
{
    uint64_t addr = buf.addr;
    if (buf.dimType == 0) {
        return QueryInRegisterMap(addr, buf.oneDim.len);
    } else if (buf.dimType == 1) {
        return QueryInRegisterMap(addr, buf.twoDim.width);
    }
    return false;
}

int32_t MmcClientDefault::SelectTransportType(const mmc_buffer &buf)
{
    if (bmProxy_->GetDataOpType() == "device_rdma") { // device RDMA暂不支持异步
        return MMC_BATCH_TRANSPORT;
    }

    if (QueryInRegisterMap(buf) || buf.dimType == 1) {
        return MMC_ASYNC_TRANSPORT;
    }
    return MMC_BATCH_TRANSPORT;
}

}
}