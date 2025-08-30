/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef ACC_LINKS_ACC_TCP_SERVER_H
#define ACC_LINKS_ACC_TCP_SERVER_H

#include "acc_def.h"
#include "acc_tcp_link.h"
#include "acc_tcp_request_context.h"
#include "acc_tcp_shared_buf.h"

namespace ock {
namespace acc {
/**
 * @brief Callback function of new connection accepted, see @AccTcpServer::RegisterNewLinkHandler
 *
 * @param req              [in] connection request information @see AccConnReq
 * @param link             [in] link created by server
 */
using AccNewLinkHandler = std::function<int(const AccConnReq &req, const AccTcpLinkComplexPtr &link)>;

/**
 * @brief Callback function of new message from peer, see @AccTcpServer::RegisterNewRequestHandler
 *
 * @param context          [in] message context see @AccTcpRequestContext
 */
using AccNewReqHandler = std::function<int32_t(const AccTcpRequestContext &context)>;

/**
 * @brief Callback function of message sent to peer, see @AccTcpServer::RegisterRequestSentHandler
 *
 * @param result           [in] send result see @AccMsgSentResult, could be sent, broken etc
 * @param header           [in] header of message send from peer
 * @param cbCtx            [in] context, specified when sending message by blocking functions
 */
using AccReqSentHandler =
    std::function<int32_t(AccMsgSentResult result, const AccMsgHeader &header, const AccDataBufferPtr &cbCtx)>;

/**
 * @brief Callback function of link broken detected, see @AccTcpServer::RegisterLinkBrokenHandler
 *
 * @param link             [in] the broken link detected
 */
using AccLinkBrokenHandler = std::function<int32_t(const AccTcpLinkComplexPtr &link)>;

/**
 * @brief Callback function of private key password decryptor, see @AccTcpServer::RegisterDecryptHandler
 *
 * @param cipherText       [in] the encrypted text(private password)
 * @param plainText        [out] the decrypted text(private password)
 * @param plaintextLen     [in] the length of plainText
 */
using AccDecryptHandler = std::function<int(const std::string &cipherText, char *plainText, size_t &plainTextLen)>;

/**
 * @brief Tcp Server for p2p communication, can be one mode:
 * 1) AccTcpServer <-> AccTcpServer
 *
 * A typical AccTcpServer major contains 3 internal parts:
 * a) socket listener, accepting connection from peer, listener can be disabled as well
 * b) workers, one worker is one thread doing event polling, callback invoking, message sending
 * c) connection manager
 */
class ACC_API AccTcpServer : public AccReferable {
public:
    /**
     * @brief Create a server
     */
    static AccTcpServerPtr Create();

public:
    /**
     * @brief Start Tcp Server with TLS enabled
     *
     * @param opt          [in] options of the TCP server
     * @return 0 if started successfully
     */
    int32_t Start(const AccTcpServerOptions &opt);

    /**
     * @brief Start Tcp Server
     *
     * @param opt          [in] options of the TCP server
     * @return 0 if started successfully
     */
    virtual int32_t Start(const AccTcpServerOptions &opt, const AccTlsOption &tlsOption) = 0;

    /**
     * @brief Stop the Tcp Server
     */
    virtual void Stop() = 0;

    /**
     * @brief Stop the Tcp Server after fork, not wait thread
     */
    virtual void StopAfterFork() = 0;

    /**
     * @brief Connect to another Tcp Server which started listener
     *
     * @param peerIp        [in] ip of peer tcp server
     * @param port          [in] port of peer tcp server listened at
     * @param req           [in] connection info
     * @param maxRetryTimes [in] max retry times
     * @param newLink       [out] connected link
     * @return 0 if successfully
     */
    virtual int32_t ConnectToPeerServer(const std::string &peerIp, uint16_t port, const AccConnReq &req,
        uint32_t maxRetryTimes, AccTcpLinkComplexPtr &newLink) = 0;

    /**
     * @brief Connect to another Tcp Server which started listener
     *
     * @param peerIp        [in] ip of peer tcp server
     * @param port          [in] port of peer tcp server listened at
     * @param req           [in] connection info
     * @param newLink       [out] connected link
     * @return 0 if successfully
     */
    int32_t ConnectToPeerServer(const std::string &peerIp, uint16_t port, const AccConnReq &req,
        AccTcpLinkComplexPtr &newLink);

    /**
     * @brief Register the handler for handling new request
     * @param msgType      [in] message type of the handler to be handled
     * @param h            [in] handler
     */
    virtual void RegisterNewRequestHandler(int16_t msgType, const AccNewReqHandler &h) = 0;

    /**
     * @brief Register the handler for handling the message sent event
     *
     * @param msgType      [in] message type of handler to be handled
     * @param h            [in] handler
     */
    virtual void RegisterRequestSentHandler(int16_t msgType, const AccReqSentHandler &h) = 0;

    /**
     * @brief Register the handler for handling link broken
     *
     * @param h            [in] handler
     */
    virtual void RegisterLinkBrokenHandler(const AccLinkBrokenHandler &h) = 0;

    /**
     * @brief Register the handler for new link connected
     *
     * @param h            [in] handler
     */
    virtual void RegisterNewLinkHandler(const AccNewLinkHandler &h) = 0;
    
    /**
     * @brief Register the handler for decryption of private key password.
     * If the private key is encrypted, this hanlder is needed to be set.
     *
     * @param h            [in] handler
     */
    virtual void RegisterDecryptHandler(const AccDecryptHandler &h) = 0;

    /**
     * @brief Load libraries for security, i.e. openssl
     *
     * @param dynLibPath   [in] path of the libraries
     * @return 0 if successfully
     */
    virtual int32_t LoadDynamicLib(const std::string &dynLibPath) = 0;

    ~AccTcpServer() override = default;
};

inline int32_t AccTcpServer::Start(const ock::acc::AccTcpServerOptions &opt)
{
    return Start(opt, AccTlsOption());
}

inline int32_t AccTcpServer::ConnectToPeerServer(const std::string &peerIp, uint16_t port, const AccConnReq &req,
    AccTcpLinkComplexPtr &newLink)
{
    return ConnectToPeerServer(peerIp, port, req, 30U, newLink);
}
} // namespace acc
} // namespace ock

#endif // ACC_LINKS_ACC_TCP_SERVER_H
