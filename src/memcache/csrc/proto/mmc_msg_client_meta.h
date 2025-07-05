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
    uint64_t num;
    PingMsg() : MsgBase{0, ML_PING_REQ, 0}{}
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

struct AllocRequest : public MsgBase {

    std::string key_;
    AllocOptions options_;

    AllocRequest() : MsgBase{0, ML_ALLOC_REQ, 0}{}
    AllocRequest(const std::string &key, const AllocOptions &prot)
        : MsgBase{0, ML_ALLOC_REQ, 0}, key_(key), options_(prot) {};

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

struct GetRequest : public MsgBase {

    std::string key_;

    GetRequest() : MsgBase{0, ML_GET_REQ, 0}{}
    explicit GetRequest(const std::string &key) : MsgBase{0, ML_GET_REQ, 0}, key_(key) {};

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


struct RemoveRequest : public MsgBase {

    std::string key_;

    RemoveRequest() : MsgBase{0, ML_REMOVE_REQ, 0}{}
    explicit RemoveRequest(const std::string &key) : MsgBase{0, ML_REMOVE_REQ, 0}, key_(key) {};

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


struct AllocResponse : public MsgBase {

    std::vector<MmcMemBlobDesc> blobs_; /* pointers of blobs */
    uint8_t numBlobs_{0};               /* number of blob that the memory object, i.e. replica count */
    uint16_t prot_{0};                  /* prot of the mem object, i.e. accessibility */
    uint8_t priority_{0};               /* priority of the memory object, used for eviction */
    uint64_t lease_{0};                 /* lease of the memory object */

    AllocResponse() : MsgBase{0, ML_ALLOC_RESP, 0}{}
    AllocResponse(const uint8_t &numBlobs, const uint16_t &prot, const uint8_t &priority, const uint64_t &lease)
        : MsgBase{0, ML_ALLOC_RESP, 0}, numBlobs_(numBlobs), prot_(prot), priority_(priority), lease_(lease) {};

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(prot_);
        packer.Serialize(priority_);
        packer.Serialize(lease_);
        packer.Serialize(numBlobs_);

        for (auto iter = blobs_.begin(); iter != blobs_.end(); iter++) {
            MmcMemBlobDesc blobDesc = *iter;
            packer.Serialize(blobDesc.rank_);
            packer.Serialize(blobDesc.gva_);
            packer.Serialize(blobDesc.size_);
            packer.Serialize(blobDesc.mediaType_);
            packer.Serialize(blobDesc.state_);
            packer.Serialize(blobDesc.prot_);
        }
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
        uint32_t rank;
        uint64_t gva;
        uint32_t size;
        uint16_t mediaType;
        BlobState state;
        uint16_t prot;
        for (uint32_t i = 0; i < numBlobs_; i++) {
            packer.Deserialize(rank);
            packer.Deserialize(gva);
            packer.Deserialize(size);
            packer.Deserialize(mediaType);
            packer.Deserialize(state);
            packer.Deserialize(prot);

            blobs_.push_back(MmcMemBlobDesc(rank, gva, size, mediaType, state, prot));
        }
        return MMC_OK;
    }
};

struct UpdateRequest : public MsgBase {

    BlobActionResult actionResult_{MMC_RESULT_NONE};
    std::string key_{""};
    uint32_t rank_{UINT32_MAX};
    uint16_t mediaType_{UINT16_MAX};

    UpdateRequest() : MsgBase{0, ML_UPDATE_REQ, 0}{}
    UpdateRequest(const BlobActionResult &result, const std::string &key, const uint32_t &rank,
                  const uint16_t &mediaType)
        : MsgBase{0, ML_UPDATE_REQ, 0}, actionResult_(result), key_(key), rank_(rank), mediaType_(mediaType) {};

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

struct Response : public MsgBase {

    Result ret_;

    Response() : MsgBase{0, ML_UPDATE_REQ, 0}{}
    Response(const Result &ret) : MsgBase{0, ML_UPDATE_REQ, 0}, ret_(ret) {};

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

struct BmRegisterRequest : public MsgBase {
    uint32_t rank_;
    uint16_t mediaType_;
    uint64_t bm_;
    uint64_t capacity_;

    BmRegisterRequest() : MsgBase{0, ML_BM_REGISTER_REQ, 0}{}

    Result Serialize(NetMsgPacker &packer) const override
    {
        packer.Serialize(msgVer);
        packer.Serialize(msgId);
        packer.Serialize(destRankId);
        packer.Serialize(rank_);
        packer.Serialize(mediaType_);
        packer.Serialize(bm_);
        packer.Serialize(capacity_);
        return MMC_OK;
    }
    Result Deserialize(NetMsgUnpacker &packer)
    {
        packer.Deserialize(msgVer);
        packer.Deserialize(msgId);
        packer.Deserialize(destRankId);
        packer.Deserialize(rank_);
        packer.Deserialize(mediaType_);
        packer.Deserialize(bm_);
        packer.Deserialize(capacity_);
        return MMC_OK;
    }
};
}  // namespace mmc
}  // namespace ock
#endif  // MF_HYBRID_MMC_MSG_CLIENT_META_H
