/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_NET_ENGINE_ACC_LINKS_H
#define MEM_FABRIC_MMC_NET_ENGINE_ACC_LINKS_H

#include "mmc_net_engine.h"
#include "mmc_net_common_acc.h"
#include "mmc_net_link_acc.h"

namespace ock {
namespace mmc {
class NetEngineAcc final : public NetEngine {
public:
    ~NetEngineAcc() override;

    Result Start(const NetEngineOptions &options) override;
    void Stop() override;

    Result ConnectToPeer(uint32_t peerId, const std::string &peerIp, uint16_t port, NetLinkPtr &newLink,
                         bool isForce) override;

    Result Call(uint32_t targetId, const char *reqData, uint32_t reqDataLen, char *respData,
                        uint32_t &respDataLen, int32_t timeoutInSecond) override;

private:
    static Result VerifyOptions(const NetEngineOptions &options);
    Result Initialize(const NetEngineOptions &options);
    void UnInitialize();
    Result StartInner();
    Result StopInner();

    /* callback function of tcp server */
    Result RegisterTcpServerHandler();
    Result HandleNewLink(const TcpConnReq &req, const TcpLinkPtr &link) const;
    Result HandleNeqRequest(const TcpReqContext &context);
    Result HandleMsgSent(TcpMsgSentResult result, const TcpMsgHeader &header, const TcpDataBufPtr &cbCtx);
    Result HandleLinkBroken(const TcpLinkPtr &link) const;

private:
    /* hot used variables */
    TcpServerPtr server_;
    NetLinkMapAccPtr peerLinkMap_;
    NetContextStorePtr ctxStore_;

    /* not hot used variables */
    NetEngineOptions options_;
    bool started_ = false;
    bool inited_ = false;
    std::mutex connectMutex_;
    int32_t timeoutInSecond_ = 0;
};
}
}

#endif  //MEM_FABRIC_MMC_NET_ENGINE_ACC_LINKS_H
