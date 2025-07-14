/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#ifndef MF_HYBRID_MMC_MSG_CLIENT_META_H
#define MF_HYBRID_MMC_MSG_CLIENT_META_H
#include "mmc_locality_strategy.h"
#include "mmc_mem_blob.h"
#include "mmc_msg_base.h"
#include "mmc_msg_packer.h"

namespace ock {
namespace mmc {
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
    std::string key_;
    AllocOptions options_;

    AllocRequest() : MsgBase{0, ML_ALLOC_REQ, 0} {}
    AllocRequest(const std::string &key, const AllocOptions &prot)
        : MsgBase{0, ML_ALLOC_REQ, 0},
          key_(key),
          options_(prot){};

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(key_);
        packer.Serialize(options_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(key_);
        packer.Deserialize(options_);
        return MMC_OK;
    }
};

struct GetRequest : MsgBase {
    std::string key_;

    GetRequest() : MsgBase{0, ML_GET_REQ, 0} {}
    explicit GetRequest(const std::string &key) : MsgBase{0, ML_GET_REQ, 0}, key_(key){};

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

struct BatchRemoveRequest : public MsgBase {
    std::vector<std::string> keys_;

    BatchRemoveRequest() : MsgBase{0, ML_BATCH_REMOVE_REQ, 0} {}
    explicit BatchRemoveRequest(const std::vector<std::string>& keys) :
        MsgBase{0, ML_BATCH_REMOVE_REQ, 0}, keys_(keys) {}

    Result Serialize(NetMsgPacker& packer) const override {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(keys_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker& packer) override {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(keys_);
        return MMC_OK;
    }
};

struct BatchRemoveResponse : public MsgBase {
    std::vector<Result> results_;

    BatchRemoveResponse() : MsgBase{0, ML_BATCH_REMOVE_RESP, 0}{}
    explicit BatchRemoveResponse(const std::vector<Result>& results) :
        MsgBase{0, ML_BATCH_REMOVE_RESP, 0}, results_(results) {}

    Result Serialize(NetMsgPacker& packer) const override {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(results_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker& packer) override {
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
    uint64_t lease_{0};                 /* lease of the memory object */

    AllocResponse() : MsgBase{0, ML_ALLOC_RESP, 0} {}
    AllocResponse(const uint8_t &numBlobs, const uint16_t &prot, const uint8_t &priority, const uint64_t &lease)
        : MsgBase{0, ML_ALLOC_RESP, 0},
          numBlobs_(numBlobs),
          prot_(prot),
          priority_(priority),
          lease_(lease){};

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(prot_);
        packer.Serialize(priority_);
        packer.Serialize(lease_);
        packer.Serialize(numBlobs_);
        packer.Serialize(blobs_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(prot_);
        packer.Deserialize(priority_);
        packer.Deserialize(lease_);
        packer.Deserialize(numBlobs_);
        packer.Deserialize(blobs_);
        return MMC_OK;
    }
};

struct UpdateRequest : MsgBase {
    BlobActionResult actionResult_{MMC_RESULT_NONE};
    std::string key_;
    uint32_t rank_{UINT32_MAX};
    uint16_t mediaType_{UINT16_MAX};

    UpdateRequest() : MsgBase{0, ML_UPDATE_REQ, 0} {}
    UpdateRequest(const BlobActionResult &result, const std::string &key, const uint32_t &rank,
                  const uint16_t &mediaType)
        : MsgBase{0, ML_UPDATE_REQ, 0},
          actionResult_(result),
          key_(key),
          rank_(rank),
          mediaType_(mediaType){};

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(actionResult_);
        packer.Serialize(key_);
        packer.Serialize(rank_);
        packer.Serialize(mediaType_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(actionResult_);
        packer.Deserialize(key_);
        packer.Deserialize(rank_);
        packer.Deserialize(mediaType_);
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

struct BmRegisterRequest : MsgBase {
    uint32_t rank_{UINT32_MAX};
    uint16_t mediaType_{UINT16_MAX};
    uint64_t addr_{UINT64_MAX};
    uint64_t capacity_{UINT64_MAX};

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
        return MMC_OK;
    }
};

struct BmUnregisterRequest : public MsgBase {
    uint32_t rank_{UINT32_MAX};
    uint16_t mediaType_{UINT16_MAX};

    BmUnregisterRequest() : MsgBase{0, ML_BM_UNREGISTER_REQ, 0}{}
    explicit BmUnregisterRequest(uint32_t rank, uint16_t mediaType) :
        MsgBase{0, ML_BM_UNREGISTER_REQ, 0}, rank_(rank), mediaType_(mediaType) {}

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(rank_);
        packer.Serialize(mediaType_);
        return MMC_OK;
    }
    Result Deserialize(NetMsgUnpacker &packer)
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
    std::string key_;
    MmcMemBlobDesc blob_; /* pointers of blobs */
    uint16_t prot_{0};    /* prot of the mem object, i.e. accessibility */
    uint8_t priority_{0}; /* priority of the memory object, used for eviction */
    uint64_t lease_{0};   /* lease of the memory object */

    MetaReplicateRequest() : MsgBase{0, LM_META_REPLICATE_REQ, 0} {}
    MetaReplicateRequest(const std::string &key, const MmcMemBlobDesc &blobDesc, const uint16_t &prot,
                         const uint8_t &priority, const uint64_t &lease)
        : MsgBase{0, LM_META_REPLICATE_REQ, 0},
          key_(key),
          blob_(blobDesc),
          prot_(prot),
          priority_(priority),
          lease_(lease){};

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(prot_);
        packer.Serialize(priority_);
        packer.Serialize(lease_);
        packer.Serialize(blob_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(prot_);
        packer.Deserialize(priority_);
        packer.Deserialize(lease_);
        packer.Deserialize(blob_);
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
    Result ret_ = 0;
    std::vector<Result> results_;

    BatchIsExistResponse() : MsgBase{0, ML_BATCH_IS_EXIST_RESP, 0} {}
    explicit BatchIsExistResponse(const Result &ret, const std::vector<Result> &results)
        : MsgBase{0, ML_BATCH_IS_EXIST_RESP, 0},
          ret_(ret),
          results_(results)
    {
    }

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(ret_);
        packer.Serialize(results_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer) override
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(ret_);
        packer.Deserialize(results_);
        return MMC_OK;
    }
};
}  // namespace mmc
}  // namespace ock
#endif  // MF_HYBRID_MMC_MSG_CLIENT_META_H
