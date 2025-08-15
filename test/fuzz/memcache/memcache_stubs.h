/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEMCACHE_STUBS_H
#define MEMCACHE_STUBS_H

#include "mmc_bm_proxy.h"
#include "mmc_meta_net_server.h"
#include "mmc_msg_client_meta.h"
#include "acc_tcp_server.h"

using namespace ock::mmc;
using namespace ock::acc;


// smem
void MockSmemBm();

// meta service
Response *GetResp();
AllocResponse *GetAllocResp();
IsExistResponse *GetIsExistResp();
QueryResponse *GetQueryResp();
BatchQueryResponse *GetBatchQueryResp();
BatchIsExistResponse *GetBatchIsExistResp();
BatchRemoveResponse *GetBatchRemoveResp();
BatchAllocResponse *GetBatchAllocResp();
BatchUpdateResponse *GetBatchUpdateResp();

void ResetResp();

void RegisterMockHandles(NetEnginePtr mServer);

// acc tcp server
class MockAccTcpLinkComplex : public AccTcpLinkComplex {
public:
    MockAccTcpLinkComplex(int fd, const std::string &ipPort, uint32_t id);
    ~MockAccTcpLinkComplex() override;
    int32_t NonBlockSend(int16_t msgType, const AccDataBufferPtr &d, const AccDataBufferPtr &cbCtx);
    int32_t NonBlockSend(int16_t msgType, uint32_t seqNo, const AccDataBufferPtr &d, const AccDataBufferPtr &cbCtx);
    int32_t NonBlockSend(int16_t msgType, int16_t opCode, uint32_t seqNo, const AccDataBufferPtr &d, const AccDataBufferPtr &cbCtx);
    int32_t EnqueueAndModifyEpoll(const AccMsgHeader &h, const AccDataBufferPtr &d, const AccDataBufferPtr &cbCtx);
    int32_t BlockSend(void *data, uint32_t len);
    int32_t BlockRecv(void *data, uint32_t demandLen);
    int32_t PollingInput(int32_t timeoutInMs) const;
    int32_t SetSendTimeout(uint32_t timeoutInUs) const;
    int32_t SetReceiveTimeout(uint32_t timeoutInUs) const;
    int32_t EnableNoBlocking() const;
    void Close();
    bool IsConnected() const;

    void UpCtx(uint64_t context);
    uint64_t UpCtx();
};

class MockAccTcpServer : public AccTcpServer {
public:
    int32_t Start(const AccTcpServerOptions &opt);
    int32_t Start(const AccTcpServerOptions &opt, const AccTlsOption &tlsOption) override;
    void Stop() override;
    void StopAfterFork() override;
    int32_t ConnectToPeerServer(const std::string &peerIp, uint16_t port, const AccConnReq &req,
        uint32_t maxRetryTimes, AccTcpLinkComplexPtr &newLink) override;
    int32_t ConnectToPeerServer(const std::string &peerIp, uint16_t port, const AccConnReq &req,
        AccTcpLinkComplexPtr &newLink);
    void RegisterNewRequestHandler(int16_t msgType, const AccNewReqHandler &h) override;
    void RegisterRequestSentHandler(int16_t msgType, const AccReqSentHandler &h) override;
    void RegisterLinkBrokenHandler(const AccLinkBrokenHandler &h) override;
    void RegisterNewLinkHandler(const AccNewLinkHandler &h) override;
    void RegisterDecryptHandler(const AccDecryptHandler &h) override;
    int32_t LoadDynamicLib(const std::string &dynLibPath) override;
    MockAccTcpServer();
    ~MockAccTcpServer() override;
    AccTcpLinkComplexPtr mockAccTcpLinkComplexPtr;
};

#endif // MEMCACHE_STUBS_H