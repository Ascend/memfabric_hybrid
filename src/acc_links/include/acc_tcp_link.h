/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/
#ifndef ACC_LINKS_ACC_TCP_LINK_H
#define ACC_LINKS_ACC_TCP_LINK_H

#include "acc_def.h"

namespace ock {
namespace acc {
/**
 * @brief A link is a socket connection, link is created by server or client
 * when client connected to server successfully or server accepted a connection
 * from client or peer server.
 *
 * There are two types of link:
 * 1) AccTcpLink, which provides blocking data operation functions, created by client
 * 2) AccTcpLinkComplex, which provides non-block data operation functions, created by server
 *
 */
class ACC_API AccTcpLink : public AccReferable {
public:
    /**
     * @brief Set up context which associated with the link
     *
     * @param context        [in] context value
     */
    void UpCtx(uint64_t context);

    /**
     * @brief Get the up context value which associated with the link
     *
     * @return context value
     */
    uint64_t UpCtx() const;

    /**
     * @brief Short name of this link
     *
     * @return name content
     */
    std::string ShortName() const;

    /**
     * @brief Get link remote ip
     *
     * @return ip:port
     */
    const std::string &GetLinkRemoteIpPort() const;

    /**
     * @brief Get id of the link
     *
     * @return id of this link
     */
    uint32_t Id() const;

    /**
     * @brief Check if the link is established
     *
     * @return true if established
     */
    bool Established() const;

    /**
     * @brief CAS to un-established state
     *
     * @return true if set to un-established successfully, false mean it is already broken called
     */
    bool Break();

    /**
     * @brief Send data to peer in blocking way
     *
     * @param data         [in] the data to be sent
     * @param len          [in] length of data
     * @return 0 if successfully
     */
    virtual int32_t BlockSend(void *data, uint32_t len) = 0;

#ifdef ENABLE_IOV
    /* *
     * @brief Send data array to peer in blocking way
     *
     * @param iov          [in] io vector
     * @param len          [in] length
     * @return
     */
    virtual int32_t BlockSendIOV(struct iovec *iov, int32_t len, int32_t totalDataLen) = 0;
#endif
    /**
     * @brief Receive data from peer in blocking way
     *
     * @param data         [in] target buffer to be received in
     * @param demandLen    [in] demand length of data
     * @return 0 if successfully
     */
    virtual int32_t BlockRecv(void *data, uint32_t demandLen) = 0;

#ifdef ENABLE_IOV
    /**
     * @brief Receive iov from peer in blocking way
     *
     * @param data         [in] target buffer to be received in
     * @param demandLen    [in] demand length of data
     * @return 0 if successfully
     */
    virtual int32_t BlockRecvIOV(struct iovec *iov, int32_t len, int32_t totalDataLen) = 0;
#endif

    /**
     * @brief Check if there is any input data
     *
     * @param timeoutInMs
     * @return
     */
    virtual int32_t PollingInput(int32_t timeoutInMs) const = 0;

    /**
     * @brief Set socket send timeout option
     *
     * @param timeoutInUs  [in] timeout in us
     * @return 0 if successfully
     */
    virtual int32_t SetSendTimeout(uint32_t timeoutInUs) const = 0;

    /**
     * @brief Set socket receive timeout option
     *
     * @param timeoutInUs  [in] timeout in us
     * @return 0 if successfully
     */
    virtual int32_t SetReceiveTimeout(uint32_t timeoutInUs) const = 0;

    /**
     * @brief Enable the link to non-blocking mode
     * @return 0 if successful
     */
    virtual int32_t EnableNoBlocking() const = 0;

    /**
     * @brief Close the fd
     */
    virtual void Close() = 0;

    virtual bool IsConnected() const = 0;

    ~AccTcpLink() override = default;

protected:
    AccTcpLink(int fd, const std::string &ipPort, uint32_t id) : fd_(fd), id_(id), ipPort_(ipPort) {}

protected:
    int established_ = 0;      /* if the connection is ok */
    int fd_ = -1;              /* fd of the link */
    uint64_t upCtx_ = 0;       /* up context */
    const uint32_t id_;        /* id */
    const std::string ipPort_; /* peer ip and port */

    friend class AccTcpWorker;
};

class ACC_API AccTcpLinkComplex : public AccTcpLink {
public:
    ~AccTcpLinkComplex() override = default;

    /**
     * @brief Put the data to be sent into queue and return
     *
     * @param msgType      [in] type of message
     * @param d            [in] data to be sent
     * @param cbCtx        [in] context data for sent callback function, it passed back by sent handle callback function
     * @return 0 if successfully
     */
    virtual int32_t NonBlockSend(int16_t msgType, const AccDataBufferPtr &d, const AccDataBufferPtr &cbCtx) = 0;

    /**
     * @brief Put the data to be sent into queue and return
     *
     * @param msgType      [in] type of message
     * @param seqNo        [in] seq no of this message
     * @param d            [in] data to be sent
     * @param cbCtx        [in] context data for sent callback function, it passed back by sent handle callback function
     * @return 0 if successfully
     */
    virtual int32_t NonBlockSend(int16_t msgType, uint32_t seqNo, const AccDataBufferPtr &d,
                                 const AccDataBufferPtr &cbCtx) = 0;

    /**
 * @brief Put the data to be sent into queue and return
 *
 * @param msgType      [in] type of message
 * @param opCode      [in] opCode of message
 * @param seqNo        [in] seq no of this message
 * @param d            [in] data to be sent
 * @param cbCtx        [in] context data for sent callback function, it passed back by sent handle callback function
 * @return 0 if successfully
 */
    virtual int32_t NonBlockSend(int16_t msgType, int16_t opCode, uint32_t seqNo, const AccDataBufferPtr &d,
                                 const AccDataBufferPtr &cbCtx) = 0;

    /**
     * @brief Put the data to be sent into queue and return
     *
     * @param h            [in] message header
     * @param d            [in] data to be sent
     * @param cbCtx        [in] context data for sent callback function, it passed back by sent handle callback function
     * @return 0 if successfully
     */
    virtual int32_t EnqueueAndModifyEpoll(const AccMsgHeader &h, const AccDataBufferPtr &d,
                                          const AccDataBufferPtr &cbCtx) = 0;

protected:
    AccTcpLinkComplex(int fd, const std::string &ipPort, uint32_t id) : AccTcpLink(fd, ipPort, id) {}
};

/* inline functions of AccTcpLink */
inline void AccTcpLink::UpCtx(uint64_t context)
{
    upCtx_ = context;
}

inline uint64_t AccTcpLink::UpCtx() const
{
    return upCtx_;
}

inline std::string AccTcpLink::ShortName() const
{
    return "link [id:" + std::to_string(id_) + ",fd:" + std::to_string(fd_) + "," + ipPort_ + "]";
}

inline const std::string &AccTcpLink::GetLinkRemoteIpPort() const
{
    return ipPort_;
}

inline uint32_t AccTcpLink::Id() const
{
    return id_;
}

inline bool AccTcpLink::Established() const
{
    return established_ == 1;
}

inline bool AccTcpLink::Break()
{
    return __sync_bool_compare_and_swap(&established_, 1, 0);
}
}  // namespace acc
}  // namespace ock

#endif  // ACC_LINKS_ACC_TCP_LINK_H
