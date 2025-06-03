/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mobs_net_engine_acc.h"

namespace ock {
namespace mobs {
Result NetEngineAcc::Start(const NetEngineOptions &options)
{
    return MO_OK;
}

Result NetEngineAcc::Stop()
{
    return MO_OK;
}

Result NetEngineAcc::ConnectToPeer(const uint16_t &peerId, const std::string &peerIp, uint16_t port,
                                   NetLinkPtr &newLink, bool isForce)
{
    return MO_OK;
}

Result NetEngineAcc::Call(const uint16_t &targetId, int32_t timeoutInSecond)
{
    return MO_OK;
}

Result NetEngineAcc::RegRequestReceivedHandler(uint16_t opCode, const NetReqReceivedHandler &h)
{
    return MO_OK;
}
Result NetEngineAcc::RegRequestSentHandler(uint16_t opCode, const NetReqSentHandler &h)
{
    return MO_OK;
}
Result NetEngineAcc::RegNewLinkHandler(uint16_t opCode, const NetNewLinkHandler &h)
{
    return MO_OK;
}
Result NetEngineAcc::RegLinkBrokenHandler(uint16_t opCode, const NetLinkBrokenHandler &h)
{
    return MO_OK;
}
}
}
