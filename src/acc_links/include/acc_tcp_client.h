/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef ACC_LINKS_ACC_TCP_CLIENT_H
#define ACC_LINKS_ACC_TCP_CLIENT_H

#include "acc_def.h"
#include "acc_tcp_shared_buf.h"

namespace ock {
namespace acc {
using AccClientReqHandler = std::function<void(uint8_t *data, uint32_t len)>;

class ACC_API AccTcpClient : public AccReferable {
public:
    static AccTcpClientPtr Create(const std::string &serverIp, uint16_t serverPort);

public:
    /**
     * @brief Connect to server with retry times
     *
     * @param connReq           [in] connection info to server
     * @param maxConnRetryTimes [in] retry count
     * @return 0 if successfully
     */
    virtual int32_t Connect(const AccConnReq &connReq, uint32_t maxConnRetryTimes) = 0;

    /**
     * @brief Connect to server with default retry times which is 5
     *
     * @param connReq      [in] connection info to server
     * @return 0 if successfully
     */
    int32_t Connect(const AccConnReq &connReq);

    /**
     * @brief Disconnect with peer
     */
    virtual void Disconnect() = 0;

    /**
     * @brief Set receive timeout with TCP socket option
     *
     * @param timeoutInUs  [in] timeout value in us
     * @return 0 if successfully
     */
    virtual int32_t SetReceiveTimeout(uint32_t timeoutInUs) noexcept = 0;

    /**
     * @brief Set send timeout with TCP socket option
     *
     * @param timeoutInUs  [in] timeout value in us
     * @return 0 if successfully
     */
    virtual int32_t SetSendTimeout(uint32_t timeoutInUs) noexcept = 0;

    /**
     * @brief Send raw data to peer in blocking way
     *
     * @param data         [in] data ptr
     * @param len          [in] length of data
     * @return 0 if successfully
     */
    virtual int32_t SendRaw(uint8_t *data, uint32_t len) noexcept = 0;

    /**
     * @brief Receive raw data from peer in blocking way
     *
     * @param data         [in] target data ptr
     * @param len          [in] len of target data
     * @return 0 if successfully
     */
    virtual int32_t ReceiveRaw(uint8_t *data, uint32_t len) noexcept = 0;

    /**
     * @brief Poll and receive raw data from peer in blocking way
     *
     * @param data         [in] target data ptr
     * @param len          [in] len of target data
     * @return 0 if successfully
     */
    virtual int32_t PollAndReceiveRaw(uint8_t *data, uint32_t len, int32_t timeoutInUs) noexcept = 0;

    /**
     * @brief Send raw data to peer with message type in blocking way
     *
     * @param data         [in] data ptr
     * @param len          [in] length of data
     * @return 0 if successfully
     */
    virtual int32_t Send(int16_t msgType, uint8_t *data, uint32_t len) noexcept = 0;

    /**
     * @brief Receive raw data with message type from peer in blocking way
     *
     * @param data         [in] target data ptr
     * @param len          [in] len of target data
     * @return 0 if successfully
     */
    virtual int32_t Receive(uint8_t *data, uint32_t len, int16_t &msgType, int16_t &result,
                            uint32_t &acLen) noexcept = 0;

    /**
     * @brief Poll and receive raw data from peer in blocking way
     *
     * @param data         [in] target data ptr
     * @param len          [in] len of target data
     * @return 0 if successfully
     */
    virtual int32_t PollAndReceive(uint8_t *data, uint32_t len, int32_t timeoutInUs, int16_t &msgType, int16_t &result,
                                   uint32_t &acLen) noexcept = 0;
    /**
     * @brief Register the handler for handling new request
     * @param msgType      [in] message type of the handler to be handled
     * @param h            [in] handler
     */
    virtual void RegisterNewRequestHandler(int16_t msgType, const AccClientReqHandler &h) = 0;

    /**
     * @brief Get ip and port string
     *
     * @return str
     */
    virtual std::string IpAndPort() const = 0;

    virtual void SetServerIpAndPort(std::string serverIp, uint16_t serverPort) = 0;

    virtual void SetLocalIp(std::string localIp) = 0;

    virtual int32_t ConnectInit(int &fd) = 0;

    virtual void SetSslOption(const AccTlsOption &tlsOption) = 0;

    virtual void SetMaxReconnCnt(uint32_t maxReconnCnt) = 0;

    /**
     * @brief Start epoll thread
     */
    virtual void StartPolling() = 0;

    /**
     * @brief Destroy the client object in async way
     *
     * @param needWait     [in] wait or not
     */
    virtual void Destroy(bool needWait) = 0;

    /**
     * @brief Destroy the client object in sync way
     */
    void Destroy();

    /**
     * @brief Load libraries for security, i.e. openssl
     *
     * @param dynLibPath   [in] path of the libraries
     * @return 0 if successfully
     */
    virtual int32_t LoadDynamicLib(const std::string &dynLibPath) = 0;

    ~AccTcpClient() override = default;
};

inline int32_t AccTcpClient::Connect(const ock::acc::AccConnReq &connReq)
{
    return Connect(connReq, 5L);
}

inline void AccTcpClient::Destroy()
{
    Destroy(true);
}
}  // namespace acc
}  // namespace ock

#endif  // ACC_LINKS_ACC_TCP_CLIENT_H
