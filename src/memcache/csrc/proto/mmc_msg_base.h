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
    MsgBase(){}
    MsgBase(int16_t ver, int16_t op, uint32_t dst): msgVer(ver), msgId(op), destRankId(dst){}
    virtual Result Serialize(NetMsgPacker &packer) const = 0;
    virtual Result Deserialize(NetMsgUnpacker &packer) = 0;
};

enum MsgId : int16_t {
    PING_MSG = 0,
};

enum LOCAL_META_OPCODE_REQ : int16_t {
    ML_PING_REQ = 0,
    ML_ALLOC_REQ = 1,
    ML_UPDATE_REQ = 2,
    ML_GET_REQ = 3,
    ML_REMOVE_REQ = 4,
    ML_BM_REGISTER_REQ = 5,
};

enum LOCAL_META_OPCODE_RESP : int16_t {
    ML_PING_RESP = 0,
    ML_ALLOC_RESP = 1,
    ML_UPDATE_RESP = 2,
    ML_BM_REGISTER_RESP = 3,
};
}  // namespace mmc
}  // namespace ock

#endif  // MEMFABRIC_MMC_MSG_BASE_H
