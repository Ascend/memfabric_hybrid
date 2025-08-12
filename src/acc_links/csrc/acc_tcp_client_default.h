/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef ACC_LINKS_ACC_TCP_CLIENT_DEFAULT_H
#define ACC_LINKS_ACC_TCP_CLIENT_DEFAULT_H

#include "acc_includes.h"
#include "acc_tcp_common.h"
#include "acc_tcp_ssl_helper.h"

namespace ock {
namespace acc {
class AccTcpClientDefault : public AccTcpClient {
public:
    explicit AccTcpClientDefault(std::string serverIp = "", uint16_t serverPort = 0)
        : serverIp_(std::move(serverIp)),
          serverPort_(serverPort)
    {
    }
    ~AccTcpClientDefault() override;

    Result Connect(const AccConnReq &connReq, uint32_t maxConnRetryTimes) override;

    void Disconnect() override;

    Result SetReceiveTimeout(uint32_t timeoutInUs) noexcept override;

    Result SetSendTimeout(uint32_t timeoutInUs) noexcept override;

    Result SendRaw(uint8_t* data, uint32_t len) noexcept override;

    Result ReceiveRaw(uint8_t* data, uint32_t len) noexcept override;

    Result PollAndReceiveRaw(uint8_t* data, uint32_t len, int32_t timeoutInUs) noexcept override;

    Result Send(int16_t msgType, uint8_t* data, uint32_t len) noexcept override;

    Result Receive(uint8_t* data, uint32_t len, int16_t &msgType, int16_t &result, uint32_t &acLen) noexcept override;

    Result PollAndReceive(uint8_t* data, uint32_t len, int32_t timeoutInUs, int16_t &msgType, int16_t &result,
                          uint32_t &acLen) noexcept override;

    void RegisterNewRequestHandler(int16_t msgType, const AccClientReqHandler &h) override;

    void RegisterDecryptHandler(const AccDecryptHandler &h) override;

    std::string IpAndPort() const override;

    void SetServerIpAndPort(std::string serverIp, uint16_t serverPort) override;

    void SetLocalIp(std::string localIp) override;

    Result ConnectInit(int &fd) override;

    void SetSslOption(const AccTlsOption &tlsOption) override;

    Result LoadDynamicLib(const std::string &dynLibPath) override;

    void SetMaxReconnCnt(uint32_t maxReconnCnt) override;

    void StartPolling() override;

    void Destroy(bool needWait) override;

protected:
    Result GenerateSslCtx();

    Result Handshake(int &fd, const AccConnReq &connReq);

    void PollingThread();

    bool ReconnectCtrl(Result callResult) noexcept;

    void AllocRecvDataBuffer(uint8_t* &data, uint32_t &dataSize, uint32_t demandSize);

protected:
    AccClientReqHandler newRequestHandle_[UNO_48]{};
    AccDecryptHandler decryptHandler_ = nullptr;
    AccTcpLinkPtr link_ = nullptr;
    AccConnReq reqforReConn_{};
    uint32_t seqNo_ = 0;
    std::mutex writeMutex_;
    std::mutex readMutex_;
    std::mutex reConnectMutex_;

    std::atomic<bool> needStop_{false}; /* if the client need to destroy */
    std::thread pollThread_; /* thread */
    std::atomic<bool> pollingStarted_{false};  // polling thread start
    std::atomic<bool> isConnected_{false};

    AccTcpSslHelperPtr sslHelper_ = nullptr;
    SSL_CTX* sslCtx_ = nullptr;
    AccTlsOption tlsOption_{};
    std::string serverIp_ = "";
    std::string localIp_ = "";
    uint64_t connRanks = 0;
    uint16_t serverPort_ = 0;
    uint32_t maxReconnCnt_ = 1;
};
using AccTcpClientDefaultPtr = AccRef<AccTcpClientDefault>;

inline Result AccTcpClientDefault::SetReceiveTimeout(uint32_t timeoutInUs) noexcept
{
    std::lock_guard<std::mutex> guard(writeMutex_);
    ASSERT_RETURN(link_.Get(), ACC_CONNECTION_NOT_READY);
    return link_->SetReceiveTimeout(timeoutInUs);
}

inline Result AccTcpClientDefault::SetSendTimeout(uint32_t timeoutInUs) noexcept
{
    std::lock_guard<std::mutex> guard(writeMutex_);
    ASSERT_RETURN(link_.Get(), ACC_CONNECTION_NOT_READY);
    return link_->SetSendTimeout(timeoutInUs);
}

inline Result AccTcpClientDefault::SendRaw(uint8_t* data, uint32_t len) noexcept
{
    std::lock_guard<std::mutex> guard(writeMutex_);
    ASSERT_RETURN(link_.Get(), ACC_CONNECTION_NOT_READY);
    return link_->BlockSend(data, len);
}

inline Result AccTcpClientDefault::ReceiveRaw(uint8_t* data, uint32_t len) noexcept
{
    std::lock_guard<std::mutex> guard(readMutex_);
    ASSERT_RETURN(link_.Get(), ACC_CONNECTION_NOT_READY);
    return link_->BlockRecv(data, len);
}

inline Result AccTcpClientDefault::PollAndReceiveRaw(uint8_t* data, uint32_t len, int32_t timeoutInUs) noexcept
{
    std::lock_guard<std::mutex> guard(readMutex_);
    ASSERT_RETURN(link_.Get(), ACC_CONNECTION_NOT_READY);
    auto result = link_->PollingInput(timeoutInUs);
    if (result != ACC_OK) {
        return result;
    }

    return link_->BlockRecv(data, len);
}

inline Result AccTcpClientDefault::PollAndReceive(uint8_t* data, uint32_t len, int32_t timeoutInUs, int16_t &msgType,
                                                  int16_t &result, uint32_t &acLen) noexcept
{
    std::lock_guard<std::mutex> guard(readMutex_);
    ASSERT_RETURN(link_.Get(), ACC_CONNECTION_NOT_READY);
    auto callResult = link_->PollingInput(timeoutInUs);
    if (callResult != ACC_OK) {
        return callResult;
    }
    return this->Receive(data, len, msgType, result, acLen);
}

inline std::string AccTcpClientDefault::IpAndPort() const
{
    return serverIp_;
}

inline void AccTcpClientDefault::RegisterNewRequestHandler(int16_t msgType, const AccClientReqHandler &h)
{
    ASSERT_RET_VOID(msgType >= MIN_MSG_TYPE && msgType < MAX_MSG_TYPE);
    ASSERT_RET_VOID(h != nullptr);
    ASSERT_RET_VOID(newRequestHandle_[msgType] == nullptr);
    newRequestHandle_[msgType] = h;
}

inline void AccTcpClientDefault::RegisterDecryptHandler(const AccDecryptHandler &h)
{
    ASSERT_RET_VOID(h != nullptr);
    ASSERT_RET_VOID(decryptHandler_ == nullptr);
    decryptHandler_ = h;
}
}  // namespace acc
}  // namespace ock
#endif  // ACC_LINKS_ACC_TCP_CLIENT_DEFAULT_H
