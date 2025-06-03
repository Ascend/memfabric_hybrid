/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MOBS_NET_ENGINE_ACC_LINKS_H
#define MOBS_NET_ENGINE_ACC_LINKS_H

#include "mobs_net_engine.h"

namespace ock {
namespace mobs {
class NetEngineAcc : public NetEngine {
public:
    ~NetEngineAcc() override = default;

    Result Start(const NetEngineOptions &options) override;
    Result Stop() override;

    Result ConnectToPeer(const uint16_t &peerId, const std::string &peerIp, uint16_t port, NetLinkPtr &newLink,
                         bool isForce) override;

    Result Call(const uint16_t &targetId, int32_t timeoutInSecond) override;

    Result RegRequestReceivedHandler(uint16_t opCode, const NetReqReceivedHandler &h) override;
    Result RegRequestSentHandler(uint16_t opCode, const NetReqReceiveHandler &h) override;
    Result RegNewLinkHandler(uint16_t opCode, const NetNewLinkHandler &h) override;
    Result RegLinkBrokenHandler(uint16_t opCode, const NetNewLinkHandler &h) override;
};
}
}

#endif  //MOBS_NET_ENGINE_ACC_LINKS_H
