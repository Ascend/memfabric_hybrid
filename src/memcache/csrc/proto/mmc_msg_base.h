/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEMFABRIC_MMC_MSG_BASE_H
#define MEMFABRIC_MMC_MSG_BASE_H

#include "mmc_common_includes.h"
#include "mmc_msg_packer.h"

namespace ock {
namespace mmc {
struct MsgBase {
    int16_t msgVer = 0;
    int16_t msgId = -1;
    uint32_t destRankId = 0;

    MsgBase() = default;
    MsgBase(int16_t ver, int16_t op, uint32_t dst) : msgVer(ver), msgId(op), destRankId(dst) {}
    virtual Result Serialize(NetMsgPacker &packer) const = 0;
    virtual Result Deserialize(NetMsgUnpacker &packer) = 0;

    virtual ~MsgBase() = default;
};

enum LOCAL_META_OPCODE_REQ : int16_t {
    ML_PING_REQ = 0,           /* ping request between client/service and service to service */
    ML_ALLOC_REQ = 1,          /* allocate an object by key and size */
    ML_UPDATE_REQ = 2,         /* update an object */
    ML_GET_REQ = 3,            /* get object info by key*/
    ML_REMOVE_REQ = 4,         /* remove object by key */
    ML_BM_REGISTER_REQ = 5,    /* register local bm to meta service */
    LM_PING_REQ = 6,           /* TODO duplicated */
    LM_META_REPLICATE_REQ = 7, /* get replicate list of object by key */
    ML_IS_EXIST_REQ = 8,       /* check if object exists*/
    ML_BATCH_IS_EXIST_REQ = 9, /* check if objects exist in batch */
    ML_BATCH_REMOVE_REQ = 10,  /* remove objects by keys in batch */
    ML_BM_UNREGISTER_REQ = 11, /* unregister local bm to meta service */
    ML_BATCH_GET_REQ = 12,     /* get object info by keys in batch */
    ML_QUERY_REQ = 13,         /* query a key to meta service to get blob info */
    ML_BATCH_QUERY_REQ = 14,   /* query keys to meta service to get blob info */
    ML_BATCH_ALLOC_REQ = 15,   /* allocate batch of objects by key and size */
};

enum LOCAL_META_OPCODE_RESP : int16_t {
    ML_PING_RESP = 0,
    ML_ALLOC_RESP = 1,
    ML_UPDATE_RESP = 2,
    ML_BM_REGISTER_RESP = 3,
    ML_IS_EXIST_RESP = 4, /* unused */
    ML_BATCH_IS_EXIST_RESP = 5,
    ML_BATCH_REMOVE_RESP = 6,
    ML_BM_UNREGISTER_RESP = 7,  /* unused */
    ML_BATCH_ALLOC_RESP = 8,
    ML_QUERY_RESP = 9,
    ML_BATCH_QUERY_RESP = 10,
};
}  // namespace mmc
}  // namespace ock

#endif  // MEMFABRIC_MMC_MSG_BASE_H
