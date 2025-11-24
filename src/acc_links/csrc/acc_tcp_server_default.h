/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef ACC_LINKS_ACC_TCP_SERVER_DEFAULT_H
#define ACC_LINKS_ACC_TCP_SERVER_DEFAULT_H

#include "acc_includes.h"
#include "acc_tcp_link_delay_cleanup.h"
#include "acc_tcp_listener.h"
#include "acc_tcp_ssl_helper.h"
#include "acc_tcp_worker.h"

namespace ock {
namespace acc {
class AccTcpServerDefault : public AccTcpServer {
public:
    AccTcpServerDefault() = default;
    ~AccTcpServerDefault() override;

    Result Start(const AccTcpServerOptions &opt, const AccTlsOption &tlsOption) override;

    Result LoadDynamicLib(const std::string &dynLibPath) override;

    void Stop() override;

    void StopAfterFork() override;

    Result ConnectToPeerServer(const std::string &peerIp, uint16_t port, const AccConnReq &req,
                               uint32_t maxRetryTimes, AccTcpLinkComplexPtr &newLink) override;

    void RegisterNewRequestHandler(int16_t msgType, const AccNewReqHandler &h) override;

    void RegisterRequestSentHandler(int16_t msgType, const AccReqSentHandler &h) override;

    void RegisterLinkBrokenHandler(const AccLinkBrokenHandler &h) override;

    void RegisterNewLinkHandler(const AccNewLinkHandler &h) override;

    void RegisterDecryptHandler(const AccDecryptHandler &h) override;

private:
    Result ValidateOptions() const;
    Result ValidateHandler() const;
    Result StartDelayCleanup();
    Result StartWorkers();
    Result StartListener();

    void StopAndCleanDelayCleanup(bool afterFork = false);
    void StopAndCleanListener(bool afterFork = false);
    void StopAndCleanWorkers(bool afterFork = false);
    void StopAndCleanSSLHelper(bool afterFork = false);

    Result GenerateSslCtx();
    Result CreateSSLLink(SSL* &ssl, int &tmpFD);
    void ValidateSSLLink(SSL* &ssl, int &tmpFD);
    Result LinkReceive(AccRef<AccTcpLinkComplexDefault>& tmpLink, const std::string &ipAndPort);

    Result Handshake(int &fd, const AccConnReq &connReq, const std::string &ipAndPort, AccTcpLinkComplexPtr &newLink);

    /* listener callback */
    Result HandleNewConnection(const AccConnReq &req, const AccTcpLinkComplexDefaultPtr &newLink);
    bool WorkerLinkLimitCheck(uint32_t workerIdx);
    void WorkerLinkCntUpdate(uint32_t workerIdx);
    Result WorkerSelect();

    /* worker callbacks */
    Result HandleNewRequest(const AccTcpRequestContext &context);
    Result HandleRequestSent(AccMsgSentResult msgResult, const AccMsgHeader &header, const AccDataBufferPtr &cbCtx);
    Result HandleLinkBroken(const AccTcpLinkComplexDefaultPtr &link);

private:
    AccNewReqHandler newRequestHandle_[UNO_48]{};
    AccReqSentHandler requestSentHandle_[UNO_48]{};
    AccLinkBrokenHandler linkBrokenHandle_ = nullptr;
    AccDecryptHandler decryptHandler_ = nullptr;
    std::vector<AccTcpWorkerPtr> workers_;
    AccTcpListenerPtr listener_;
    std::atomic<uint32_t> nextWorkerIndex_{0};
    std::unordered_map<uint32_t, AccTcpLinkComplexDefaultPtr> connectedLinks_;
    AccNewLinkHandler newLinkHandle_ = nullptr;
    AccTcpLinkDelayCleanupPtr delayCleanup_{nullptr};
    std::mutex linkCntMutex;
    std::unordered_map<uint32_t, uint32_t> workerLinkCnt_;
    uint32_t maxWorkerLinkeCnt_ = UNO_1024;

    std::mutex mutex_;
    std::atomic<bool> started_{false};
    AccTcpServerOptions options_;
    AccTcpSslHelperPtr sslHelper_ = nullptr;
    SSL_CTX* sslCtx_ = nullptr;
    AccTlsOption tlsOption_{};
};
using AccTcpServerDefaultPtr = AccRef<AccTcpServerDefault>;

inline void AccTcpServerDefault::RegisterNewRequestHandler(int16_t msgType, const AccNewReqHandler &h)
{
    ASSERT_RET_VOID(msgType >= MIN_MSG_TYPE && msgType < MAX_MSG_TYPE);
    ASSERT_RET_VOID(h != nullptr);
    ASSERT_RET_VOID(newRequestHandle_[msgType] == nullptr);
    newRequestHandle_[msgType] = h;
}

inline void AccTcpServerDefault::RegisterRequestSentHandler(int16_t msgType, const AccReqSentHandler &h)
{
    ASSERT_RET_VOID(msgType >= MIN_MSG_TYPE && msgType < MAX_MSG_TYPE);
    ASSERT_RET_VOID(h != nullptr);
    ASSERT_RET_VOID(requestSentHandle_[msgType] == nullptr);
    requestSentHandle_[msgType] = h;
}

inline void AccTcpServerDefault::RegisterLinkBrokenHandler(const AccLinkBrokenHandler &h)
{
    ASSERT_RET_VOID(h != nullptr);
    ASSERT_RET_VOID(linkBrokenHandle_ == nullptr);
    linkBrokenHandle_ = h;
}

inline void AccTcpServerDefault::RegisterNewLinkHandler(const AccNewLinkHandler &h)
{
    ASSERT_RET_VOID(h != nullptr);
    ASSERT_RET_VOID(newLinkHandle_ == nullptr);
    newLinkHandle_ = h;
}

inline void AccTcpServerDefault::RegisterDecryptHandler(const AccDecryptHandler &h)
{
    ASSERT_RET_VOID(h != nullptr);
    ASSERT_RET_VOID(decryptHandler_ == nullptr);
    decryptHandler_ = h;
}

inline Result AccTcpServerDefault::HandleNewRequest(const AccTcpRequestContext &context)
{
    auto msgType = context.MsgType();
    ASSERT_RETURN(msgType >= MIN_MSG_TYPE && msgType < MAX_MSG_TYPE, ACC_LINK_MSG_INVALID);
    auto &handler = newRequestHandle_[msgType];
    if (UNLIKELY(handler == nullptr)) {
        LOG_ERROR("NewRequestHandler is not register for msg type " << msgType << ", msg dropped");
        return ACC_LINK_MSG_INVALID;
    }

    return handler(context);
}

inline Result AccTcpServerDefault::HandleRequestSent(AccMsgSentResult msgResult, const AccMsgHeader &header,
                                                     const AccDataBufferPtr &cbCtx)
{
    auto msgType = header.type;
    ASSERT_RETURN(msgType >= MIN_MSG_TYPE && msgType < MAX_MSG_TYPE, ACC_LINK_MSG_INVALID);
    auto &handler = requestSentHandle_[msgType];
    if (handler == nullptr) {
        LOG_TRACE("RequestSentHandler is not register for msg type " << msgType << ", msg dropped");
        return ACC_LINK_MSG_INVALID;
    }

    return handler(msgResult, header, cbCtx);
}

inline Result AccTcpServerDefault::HandleLinkBroken(const AccTcpLinkComplexDefaultPtr &link)
{
    ASSERT_RETURN(link.Get() != nullptr, ACC_INVALID_PARAM);

    auto breakByMe = link->Break();
    if (!breakByMe) {
        return ACC_OK;
    }

    /* get un-sent messages and call upper */
    AccLinkedMessageNode* node = link->TakeAwayMessages();
    while (node != nullptr) {
        auto nextNode = node->next;
        HandleRequestSent(MSG_LINK_BROKEN, node->header, node->cbCtx);
        delete node;
        node = nextNode;
    }

    /* call to user define handler */
    linkBrokenHandle_(link.Get());

    /* clean up things */
    {
        /* check and add new link into map */
        std::lock_guard<std::mutex> guard(mutex_);
        if (!started_) {
            return ACC_OK;
        }
        auto iter = connectedLinks_.find(link->Id());
        if (iter == connectedLinks_.end()) {
            LOG_WARN("Failed to find the link " << link->Id());
        }

        /* added to worker */
        workers_[link->workerIndex_]->RemoveLink(link);
        WorkerLinkCntUpdate(link->workerIndex_);

        /* bind worker index */
        link->workerIndex_ = 0;
        /* enqueue this link to delay cleanup manager and erase from connected links map */
        delayCleanup_->Enqueue(link.Get());
        connectedLinks_.erase(link->Id());
    }

    return ACC_OK;
}
}  // namespace acc
}  // namespace ock

#endif  // ACC_LINKS_ACC_TCP_SERVER_DEFAULT_H