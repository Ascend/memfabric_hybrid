/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef MF_HYBRID_MMC_MSG_CLIENT_META_H
#define MF_HYBRID_MMC_MSG_CLIENT_META_H

#include "mmc_mem_blob.h"
#include "mmc_msg_base.h"
#include "mmc_msg_packer.h"

namespace ock {
namespace mmc {
struct AllocOptions {
    uint64_t blobSize_{0};
    uint32_t numBlobs_{0};
    uint16_t mediaType_{0};
    std::vector<uint32_t> preferredRank_{};
    uint32_t flags_{0};
    AllocOptions() = default;
    AllocOptions(const uint64_t blobSize, const uint32_t numBlobs, const uint16_t mediaType,
                 const std::vector<uint32_t> &preferredRank, const uint32_t flags)
        : blobSize_(blobSize), numBlobs_(numBlobs), mediaType_(mediaType), preferredRank_(preferredRank), flags_(flags)
    {}

    Result Serialize(NetMsgPacker &packer) const
    {
        packer.Serialize(blobSize_);
        packer.Serialize(numBlobs_);
        packer.Serialize(mediaType_);
        packer.Serialize(preferredRank_);
        packer.Serialize(flags_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer)
    {
        packer.Deserialize(blobSize_);
        packer.Deserialize(numBlobs_);
        packer.Deserialize(mediaType_);
        packer.Deserialize(preferredRank_);
        packer.Deserialize(flags_);
        return MMC_OK;
    }

    friend std::ostream &operator<<(std::ostream &os, const AllocOptions &obj)
    {
        os << "blobSize: " << obj.blobSize_ << ", numBlobs: " << obj.numBlobs_ << ", preferredRank: [";
        for (const uint32_t rank : obj.preferredRank_) {
            os << rank << ", ";
        }
        return os << "], ";
    }
};

struct PingMsg : public MsgBase {
    uint64_t num = UINT64_MAX;
    PingMsg() : MsgBase{0, ML_PING_REQ, 0} {}

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(num);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(num);
        return MMC_OK;
    }
};

struct AllocRequest : MsgBase {
    uint64_t operateId_;
    std::string key_;
    AllocOptions options_;

    AllocRequest() : MsgBase{0, ML_ALLOC_REQ, 0}, operateId_{0} {}
    AllocRequest(const std::string& key, const AllocOptions& prot, uint64_t operateId)
        : MsgBase{0, ML_ALLOC_REQ, 0}, operateId_(operateId), key_(key), options_(prot) {};

    Result Serialize(NetMsgPacker& packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(key_);
        options_.Serialize(packer);
        packer.Serialize(operateId_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker& packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(key_);
        options_.Deserialize(packer);
        packer.Deserialize(operateId_);
        return MMC_OK;
    }
};

struct GetRequest : MsgBase {
    uint64_t operateId_;
    uint32_t rankId_;
    std::string key_;
    bool isGet_;

    GetRequest() : MsgBase{0, ML_GET_REQ, 0}, operateId_{0}, rankId_{0}, isGet_{false} {}
    explicit GetRequest(const std::string &key, uint32_t rankId, uint64_t operateId, bool isGet)
        : MsgBase{0, ML_GET_REQ, 0}, key_(key), rankId_(rankId), operateId_(operateId), isGet_(isGet){};

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(operateId_);
        packer.Serialize(rankId_);
        packer.Serialize(isGet_);
        packer.Serialize(key_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(operateId_);
        packer.Deserialize(rankId_);
        packer.Deserialize(isGet_);
        packer.Deserialize(key_);
        return MMC_OK;
    }
};

struct BatchGetRequest : MsgBase {
    uint64_t operateId_;
    uint32_t rankId_;
    std::vector<std::string> keys_;

    BatchGetRequest() : MsgBase{0, ML_BATCH_GET_REQ, 0}, operateId_{0}, rankId_{0} {}
    explicit BatchGetRequest(const std::vector<std::string> &keys, uint32_t rankId, uint64_t operateId)
        : MsgBase{0, ML_BATCH_GET_REQ, 0}, keys_(keys), rankId_(rankId), operateId_(operateId) {}

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(operateId_);
        packer.Serialize(rankId_);
        packer.Serialize(keys_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(operateId_);
        packer.Deserialize(rankId_);
        packer.Deserialize(keys_);
        return MMC_OK;
    }
};

struct BatchUpdateRequest : MsgBase {
    std::vector<BlobActionResult> actionResults_;
    std::vector<std::string> keys_;
    std::vector<uint32_t> ranks_;
    std::vector<uint16_t> mediaTypes_;
    uint64_t operateId_;

    BatchUpdateRequest() : MsgBase{0, ML_BATCH_UPDATE_REQ, 0}, operateId_{0} {}

    BatchUpdateRequest(
        const std::vector<BlobActionResult>& actionResults,
        const std::vector<std::string>& keys,
        const std::vector<uint32_t>& ranks,
        const std::vector<uint16_t>& mediaTypes,
        uint64_t operateId
    ) : MsgBase{0, ML_BATCH_UPDATE_REQ, 0},
        actionResults_(actionResults),
        keys_(keys),
        ranks_(ranks),
        mediaTypes_(mediaTypes),
        operateId_(operateId) {}

    Result Serialize(NetMsgPacker& packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(actionResults_);
        packer.Serialize(keys_);
        packer.Serialize(ranks_);
        packer.Serialize(mediaTypes_);
        packer.Serialize(operateId_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker& packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(actionResults_);
        packer.Deserialize(keys_);
        packer.Deserialize(ranks_);
        packer.Deserialize(mediaTypes_);
        packer.Deserialize(operateId_);
        return MMC_OK;
    }
};

struct BatchAllocRequest : MsgBase {
    std::vector<std::string> keys_;
    std::vector<AllocOptions> options_;
    uint32_t flags_;
    uint64_t operateId_;

    BatchAllocRequest() : MsgBase{0, ML_BATCH_ALLOC_REQ, 0}, flags_{0}, operateId_(0) {}
    
    BatchAllocRequest(const std::vector<std::string>& keys,
                      const std::vector<AllocOptions>& options,
                      uint32_t flags,
                      uint64_t operateId)
        : MsgBase{0, ML_BATCH_ALLOC_REQ, 0},
          keys_(keys),
          options_(options),
          flags_(flags),
          operateId_(operateId) {}

    Result Serialize(NetMsgPacker& packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(keys_);
        MMC_ASSERT_RETURN(keys_.size() == options_.size(), MMC_ERROR);
        for (const auto &option : options_) {
            option.Serialize(packer);
        }
        packer.Serialize(flags_);
        packer.Serialize(operateId_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker& packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(keys_);
        options_.clear();
        options_.assign(keys_.size(), AllocOptions());
        for (auto &option : options_) {
            option.Deserialize(packer);
        }
        packer.Deserialize(flags_);
        packer.Deserialize(operateId_);
        return MMC_OK;
    }
};

struct BatchAllocResponse : MsgBase {
    std::vector<std::vector<MmcMemBlobDesc>> blobs_;
    std::vector<uint8_t> numBlobs_;
    std::vector<uint16_t> prots_;
    std::vector<uint8_t> priorities_;
    std::vector<uint64_t> leases_;
    std::vector<Result> results_;

    BatchAllocResponse() : MsgBase{0, ML_BATCH_ALLOC_RESP, 0} {}
    BatchAllocResponse(
        const std::vector<uint8_t> &numBlobs,
        const std::vector<uint16_t> &prot,
        const std::vector<uint8_t> &priority,
        const std::vector<uint64_t> &lease,
        const std::vector<std::vector<MmcMemBlobDesc>> &blobs,
        const std::vector<Result>& results
    ) : MsgBase{0, ML_BATCH_ALLOC_RESP, 0},
        numBlobs_(numBlobs),
        prots_(prot),
        priorities_(priority),
        leases_(lease),
        blobs_(blobs),
        results_(results) {}

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(prots_);
        packer.Serialize(priorities_);
        packer.Serialize(leases_);
        packer.Serialize(numBlobs_);
        packer.Serialize(blobs_);
        packer.Serialize(results_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(prots_);
        packer.Deserialize(priorities_);
        packer.Deserialize(leases_);
        packer.Deserialize(numBlobs_);
        packer.Deserialize(blobs_);
        packer.Deserialize(results_);
        return MMC_OK;
    }
};

struct RemoveRequest : MsgBase {
    std::string key_;

    RemoveRequest() : MsgBase{0, ML_REMOVE_REQ, 0} {}
    explicit RemoveRequest(const std::string &key) : MsgBase{0, ML_REMOVE_REQ, 0}, key_(key){};

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(key_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(key_);
        return MMC_OK;
    }
};

struct BatchRemoveRequest : MsgBase {
    std::vector<std::string> keys_;

    BatchRemoveRequest() : MsgBase{0, ML_BATCH_REMOVE_REQ, 0} {}
    explicit BatchRemoveRequest(const std::vector<std::string>& keys)
        : MsgBase{0, ML_BATCH_REMOVE_REQ, 0}, keys_(keys) {}

    Result Serialize(NetMsgPacker& packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(keys_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker& packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(keys_);
        return MMC_OK;
    }
};

struct BatchRemoveResponse : MsgBase {
    std::vector<Result> results_;

    BatchRemoveResponse() : MsgBase{0, ML_BATCH_REMOVE_RESP, 0}{}
    explicit BatchRemoveResponse(const std::vector<Result>& results)
        : MsgBase{0, ML_BATCH_REMOVE_RESP, 0}, results_(results) {}

    Result Serialize(NetMsgPacker& packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(results_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker& packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(results_);
        return MMC_OK;
    }
};

struct AllocResponse : MsgBase {
    std::vector<MmcMemBlobDesc> blobs_; /* pointers of blobs */
    uint8_t numBlobs_{0};               /* number of blob that the memory object, i.e. replica count */
    uint16_t prot_{0};                  /* prot of the mem object, i.e. accessibility */
    uint8_t priority_{0};               /* priority of the memory object, used for eviction */
    Result result_{0};                  /* result of the operation */

    AllocResponse() : MsgBase{0, ML_ALLOC_RESP, 0} {}
    AllocResponse(const uint8_t &numBlobs, const uint16_t &prot, const uint8_t &priority, const uint64_t &lease)
        : MsgBase{0, ML_ALLOC_RESP, 0},
          numBlobs_(numBlobs),
          prot_(prot),
          priority_(priority) {}

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(prot_);
        packer.Serialize(priority_);
        packer.Serialize(numBlobs_);
        packer.Serialize(blobs_);
        packer.Serialize(result_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(prot_);
        packer.Deserialize(priority_);
        packer.Deserialize(numBlobs_);
        packer.Deserialize(blobs_);
        packer.Deserialize(result_);
        return MMC_OK;
    }
};

struct UpdateRequest : MsgBase {
    BlobActionResult actionResult_{};
    std::string key_{};
    uint32_t rank_{UINT32_MAX};
    uint16_t mediaType_{UINT16_MAX};
    uint64_t operateId_;

    UpdateRequest() : MsgBase{0, ML_UPDATE_REQ, 0}, operateId_(0) {}
    UpdateRequest(const BlobActionResult& result, const std::string& key, const uint64_t& rank,
                  const uint16_t& mediaType, const uint64_t& operateId)
        : MsgBase{0, ML_UPDATE_REQ, 0}, actionResult_(result), key_(key), rank_(rank), mediaType_(mediaType),
          operateId_(operateId) {};

    Result Serialize(NetMsgPacker& packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(actionResult_);
        packer.Serialize(key_);
        packer.Serialize(rank_);
        packer.Serialize(mediaType_);
        packer.Serialize(operateId_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker& packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(actionResult_);
        packer.Deserialize(key_);
        packer.Deserialize(rank_);
        packer.Deserialize(mediaType_);
        packer.Deserialize(operateId_);
        return MMC_OK;
    }
};

struct Response : MsgBase {
    Result ret_ = 0;

    Response() : MsgBase{0, ML_UPDATE_REQ, 0} {}
    explicit Response(const Result &ret) : MsgBase{0, ML_UPDATE_REQ, 0}, ret_(ret){};

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(ret_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(ret_);
        return MMC_OK;
    }
};

struct BatchUpdateResponse : MsgBase {
    std::vector<Result> results_;

    BatchUpdateResponse() : MsgBase{0, ML_BATCH_UPDATE_RESP, 0} {}

    explicit BatchUpdateResponse(const std::vector<Result>& results)
        : MsgBase{0, ML_BATCH_UPDATE_RESP, 0},
          results_(results) {}

    Result Serialize(NetMsgPacker& packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(results_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker& packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(results_);
        return MMC_OK;
    }
};

struct BmRegisterRequest : MsgBase {
    uint32_t rank_{UINT32_MAX};
    std::vector<uint16_t> mediaType_{};
    std::vector<uint64_t> addr_{};
    std::vector<uint64_t> capacity_{};
    std::map<std::string, MmcMemBlobDesc> blobMap_;

    BmRegisterRequest() : MsgBase{0, ML_BM_REGISTER_REQ, 0} {}

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(rank_);
        packer.Serialize(mediaType_);
        packer.Serialize(addr_);
        packer.Serialize(capacity_);
        packer.Serialize(blobMap_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(rank_);
        packer.Deserialize(mediaType_);
        packer.Deserialize(addr_);
        packer.Deserialize(capacity_);
        packer.Deserialize(blobMap_);
        return MMC_OK;
    }
};

struct BmUnregisterRequest : public MsgBase {
    uint32_t rank_;
    std::vector<uint16_t> mediaType_{UINT16_MAX};

    BmUnregisterRequest() : MsgBase{0, ML_BM_UNREGISTER_REQ, 0}, rank_{0} {}
    explicit BmUnregisterRequest(uint32_t rank, uint16_t mediaType)
        : MsgBase{0, ML_BM_UNREGISTER_REQ, 0}, rank_(rank), mediaType_(mediaType) {}

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(rank_);
        packer.Serialize(mediaType_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(rank_);
        packer.Deserialize(mediaType_);
        return MMC_OK;
    }
};

struct MetaReplicateRequest : public MsgBase {
    std::vector<uint32_t> ops_;
    std::vector<std::string> keys_;
    std::vector<MmcMemBlobDesc> blobs_; /* pointers of blobs */

    MetaReplicateRequest() : MsgBase{0, LM_META_REPLICATE_REQ, 0} {}
    MetaReplicateRequest(const std::vector<uint32_t> &ops,
                         const std::vector<std::string> &keys,
                         const std::vector<MmcMemBlobDesc> &blobs)
        : MsgBase{0, LM_META_REPLICATE_REQ, 0},
          ops_(ops),
          keys_(keys),
          blobs_(blobs) {}

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(ops_);
        packer.Serialize(keys_);
        packer.Serialize(blobs_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(ops_);
        packer.Deserialize(keys_);
        packer.Deserialize(blobs_);
        return MMC_OK;
    }

    std::string KeysString()
    {
        std::stringstream ss;
        ss << "[";
        for (size_t i = 0; i < keys_.size(); ++i) {
            ss << keys_[i];
            if (i != keys_.size() - 1) {
                ss << ", ";
            }
        }
        ss << "]";
        return ss.str();
    }
};

struct BlobCopyRequest : public MsgBase {
    MmcMemBlobDesc srcBlob_;
    MmcMemBlobDesc dstBlob_;

    BlobCopyRequest() : MsgBase{0, LM_BLOB_COPY_REQ, 0} {}
    BlobCopyRequest(const MmcMemBlobDesc& src, const MmcMemBlobDesc& dst)
        : MsgBase{0, LM_BLOB_COPY_REQ, 0}, srcBlob_(src), dstBlob_(dst)
    {}

    Result Serialize(NetMsgPacker& packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(srcBlob_);
        packer.Serialize(dstBlob_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker& packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(srcBlob_);
        packer.Deserialize(dstBlob_);
        return MMC_OK;
    }
};

struct IsExistRequest : MsgBase {
    std::string key_;

    IsExistRequest() : MsgBase{0, ML_IS_EXIST_REQ, 0} {}
    explicit IsExistRequest(std::string key) : MsgBase{0, ML_IS_EXIST_REQ, 0}, key_(std::move(key)) {}

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(key_);
        return MMC_OK;
    }
    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(key_);
        return MMC_OK;
    }
};

struct IsExistResponse : MsgBase {
    Result ret_ = -1;

    IsExistResponse() : MsgBase{0, ML_IS_EXIST_RESP, 0} {}
    explicit IsExistResponse(const Result &ret)
        : MsgBase{0, ML_IS_EXIST_RESP, 0},
          ret_(ret)
    {
    }

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(ret_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(ret_);
        return MMC_OK;
    }
};

struct BatchIsExistRequest : MsgBase {
    std::vector<std::string> keys_;

    BatchIsExistRequest() : MsgBase{0, ML_BATCH_IS_EXIST_REQ, 0} {}
    explicit BatchIsExistRequest(const std::vector<std::string> &keys)
        : MsgBase{0, ML_BATCH_IS_EXIST_REQ, 0},
          keys_(keys)
    {
    }

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(keys_);
        return MMC_OK;
    }
    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(keys_);
        return MMC_OK;
    }
};

struct BatchIsExistResponse : MsgBase {
    std::vector<Result> results_;

    BatchIsExistResponse() : MsgBase{0, ML_BATCH_IS_EXIST_RESP, 0} {}
    explicit BatchIsExistResponse(const std::vector<Result> &results)
        : MsgBase{0, ML_BATCH_IS_EXIST_RESP, 0},
          results_(results)
    {
    }

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(results_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(results_);
        return MMC_OK;
    }
};

struct QueryRequest : MsgBase {
    std::string key_;

    QueryRequest() : MsgBase{0, ML_QUERY_REQ, 0} {}
    explicit QueryRequest(const std::string& key)
        : MsgBase{0, ML_QUERY_REQ, 0},
          key_(key)
    {
    }

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(key_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(key_);
        return MMC_OK;
    }
};

struct QueryResponse : MsgBase {
    MemObjQueryInfo queryInfo_;

    QueryResponse() : MsgBase{0, ML_QUERY_RESP, 0} {}
    explicit QueryResponse(const MemObjQueryInfo &queryInfo)
        : MsgBase{0, ML_QUERY_RESP, 0},
          queryInfo_(queryInfo)
    {
    }

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(queryInfo_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(queryInfo_);
        return MMC_OK;
    }
};

struct BatchQueryRequest : MsgBase {
    std::vector<std::string> keys_;

    BatchQueryRequest() : MsgBase{0, ML_BATCH_QUERY_REQ, 0} {}
    explicit BatchQueryRequest(const std::vector<std::string> &keys)
        : MsgBase{0, ML_BATCH_QUERY_REQ, 0},
          keys_(keys)
    {
    }

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(keys_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(keys_);
        return MMC_OK;
    }
};

struct BatchQueryResponse : MsgBase {
    std::vector<MemObjQueryInfo> batchQueryInfos_;

    BatchQueryResponse() : MsgBase{0, ML_BATCH_QUERY_RESP, 0} {}
    explicit BatchQueryResponse(const std::vector<MemObjQueryInfo> &batchQueryInfos)
        : MsgBase{0, ML_BATCH_QUERY_RESP, 0},
          batchQueryInfos_(batchQueryInfos)
    {
    }

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(batchQueryInfos_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(batchQueryInfos_);
        return MMC_OK;
    }
};
}  // namespace mmc
}  // namespace ock
#endif  // MF_HYBRID_MMC_MSG_CLIENT_META_H