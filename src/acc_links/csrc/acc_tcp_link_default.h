/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef ACC_LINKS_ACC_TCP_LINK_DEFAULT_H
#define ACC_LINKS_ACC_TCP_LINK_DEFAULT_H

#include <sys/uio.h>

#include "acc_includes.h"
#include "acc_common_util.h"
#include "acc_tcp_common.h"
#include "acc_tcp_shared_buf.h"
#include "acc_tcp_ssl_helper.h"
#include "openssl_api_wrapper.h"

namespace ock {
namespace acc {

constexpr long TIME_UNIT_INTERVAL = 1000L;
constexpr int32_t SSL_ERROR_SSL = 1;
constexpr int32_t SSL_ERROR_SYSCALL = 5;
constexpr int32_t SSL_ERROR_ZERO_RETURN = 6;
const std::set<int> g_errnoToReconn = {EPERM,   EINTR,    EBADF,     EAGAIN,   EPIPE,       ECONNRESET,
                                       EISCONN, ENOTCONN, ETIMEDOUT, ENETDOWN, ENETUNREACH, ECONNREFUSED};
const std::set<int> g_errnoToReconnSsl = {SSL_ERROR_SSL, SSL_ERROR_ZERO_RETURN};

struct AccLinkReceiveState {
    uint16_t headerLen = sizeof(AccMsgHeader);
    uint16_t headerToBeReceived = headerLen;
    ssize_t bodyToBeReceived = -1;

    inline bool ShouldReceiveHeader() const
    {
        return headerToBeReceived > 0;
    }

    inline uint16_t ReceivedHeaderLen() const
    {
        return headerLen > headerToBeReceived ? (headerLen - headerToBeReceived) : 0;
    }

    inline void ResetHeader()
    {
        headerToBeReceived = sizeof(AccMsgHeader);
        bodyToBeReceived = -1;
    }

    inline bool BodySatisfied(ssize_t newReceivedSize)
    {
        bodyToBeReceived = (bodyToBeReceived > newReceivedSize) ? (bodyToBeReceived - newReceivedSize) : 0;
        return bodyToBeReceived == 0;
    }

    inline bool HeaderSatisfied(uint16_t newReceivedHeader)
    {
        headerToBeReceived = (headerToBeReceived > newReceivedHeader) ? (headerToBeReceived - newReceivedHeader) : 0;
        return headerToBeReceived == 0;
    }
} __attribute__((packed));

/**
 * @brief AccTcpLinkDefault which is a tcp connection for data transmit
 */
class AccTcpLinkDefault : public AccTcpLinkComplex {
public:
    static uint32_t NewId()
    {
        static std::atomic<uint32_t> gIdGen(0);
        return gIdGen++;
    }

public:
    AccTcpLinkDefault(int fd, const std::string &ipPort, uint32_t id, SSL *ssl = nullptr)
        : AccTcpLinkComplex(fd, ipPort, id),
          ssl_(ssl)
    {
    }

    ~AccTcpLinkDefault() override
    {
        if (ssl_ != nullptr) {
            if (AccCommonUtil::SslShutdownHelper(ssl_) != ACC_OK) {
                LOG_ERROR("shut down ssl failed!");
            }
            OpenSslApiWrapper::SslFree(ssl_);
            ssl_ = nullptr;
        }
        SafeCloseFd(fd_);
    }

    Result BlockSend(void *data, uint32_t len) override
    {
        ASSERT_RETURN(fd_ != -1, ACC_CONNECTION_NOT_READY);
        ASSERT_RETURN(data != nullptr, ACC_INVALID_PARAM);
        ASSERT_RETURN(len != 0, ACC_INVALID_PARAM);

        uint32_t remain = len;
        ssize_t result = 0;
        while (remain > 0) {
            if (LIKELY(ssl_ == nullptr)) {
                result = ::send(fd_, data, remain, 0);
                if (UNLIKELY(result < 0)) {
                    auto errorNumber = errno;
                    if (errorNumber == EINTR) { /* interrupted */
                        continue;
                    }
                    LOG_WARN("Failed to send data to " << ipPort_ << ", errno " << errorNumber);
                    if (g_errnoToReconn.count(-errorNumber) > 0) {
                        return ACC_LINK_NEED_RECONN;
                    }
                    return -errorNumber;
                }
            } else {
                result = OpenSslApiWrapper::SslWrite(ssl_, data, remain);
                if (UNLIKELY(result <= 0)) {
                    const auto errorNumber = errno;
                    int sslErr = OpenSslApiWrapper::SslGetError(ssl_, result);
                    LOG_ERROR("Failed to ssl write data to " << ipPort_ << ", sslErr " << sslErr);
                    if (g_errnoToReconnSsl.count(sslErr) > 0) {
                        return ACC_LINK_NEED_RECONN;
                    }
                    if (sslErr == SSL_ERROR_SYSCALL && g_errnoToReconn.count(-errorNumber) > 0) {
                        return ACC_LINK_NEED_RECONN;
                    }
                    return ACC_LINK_MSG_INVALID;
                }
            }
            data = reinterpret_cast<uint8_t *>(data) + result;
            if (UNLIKELY(static_cast<ssize_t>(remain) < result)) {
                remain = 0;
            } else {
                remain -= static_cast<uint32_t>(result);
            }
        }
        return ACC_OK;
    }

#ifdef ENABLE_IOV
    Result BlockSendIOV(struct iovec *iov, int32_t len, int32_t totalDataLen) override
    {
        ASSERT_RETURN(fd_ != -1, ACC_CONNECTION_NOT_READY);
        ASSERT_RETURN(iov != nullptr, ACC_INVALID_PARAM);
        ASSERT_RETURN(len > 0, ACC_INVALID_PARAM);

        for (int32_t i = 0; i < len; i++) {
            ASSERT_RETURN(iov[i].iov_base != nullptr, ACC_INVALID_PARAM);
            ASSERT_RETURN(iov[i].iov_len > 0, ACC_INVALID_PARAM);
        }

        ssize_t result = 0;
        if (LIKELY(ssl_ == nullptr)) {
            result = ::writev(fd_, iov, len);
            if (LIKELY(result == totalDataLen)) {
                return ACC_OK;
            }
            auto errorNumber = errno;
            LOG_WARN("Failed to send data to " << ipPort_ << ", errno " << errorNumber);
            return -errorNumber;
        } else {
            for (int32_t i = 0; i < len; i++) {
                auto callResult = BlockSend(iov[i].iov_base, iov[i].iov_len);
                if (callResult != ACC_OK) {
                    LOG_ERROR("Failed to ssl writev to " << ipPort_ << ", len " << iov[i].iov_len);
                    return callResult;
                }
            }
            return ACC_OK;
        }
    }
#endif

    Result BlockRecv(void *data, uint32_t demandLen) override
    {
        ASSERT_RETURN(fd_ != -1, ACC_CONNECTION_NOT_READY);
        ASSERT_RETURN(data != nullptr, ACC_INVALID_PARAM);
        ASSERT_RETURN(demandLen != 0, ACC_INVALID_PARAM);

        uint32_t remain = demandLen;
        ssize_t result = 0;
        while (remain > 0) {
            if (LIKELY(ssl_ == nullptr)) {
                result = ::recv(fd_, data, remain, 0);
                if (UNLIKELY(result < 0)) {
                    auto errorNumber = errno;
                    if (errorNumber == EINTR) { /* interrupted */
                        continue;
                    }
                    LOG_WARN("Failed to read data from " << ipPort_ << ", errno " << errorNumber);
                    if (g_errnoToReconn.count(-errorNumber) > 0) {
                        return ACC_LINK_NEED_RECONN;
                    }
                    return -errorNumber;
                }
                if (result == 0) {  // link down
                    return ACC_LINK_ERROR;
                }
            } else {
                result = OpenSslApiWrapper::SslRead(ssl_, data, remain);
                if (UNLIKELY(result <= 0)) {
                    const auto errorNumber = errno;
                    int sslErr = OpenSslApiWrapper::SslGetError(ssl_, result);
                    LOG_ERROR("Failed to ssl read data from " << ipPort_ << ", sslErr " << sslErr);
                    if (g_errnoToReconnSsl.count(sslErr) > 0) {
                        return ACC_LINK_NEED_RECONN;
                    }
                    if (sslErr == SSL_ERROR_SYSCALL && g_errnoToReconn.count(-errorNumber) > 0) {
                        return ACC_LINK_NEED_RECONN;
                    }
                    return ACC_LINK_MSG_INVALID;
                }
            }

            data = reinterpret_cast<uint8_t *>(data) + result;
            if (UNLIKELY(static_cast<ssize_t>(remain) < result)) {
                remain = 0;
            } else {
                remain -= static_cast<uint32_t>(result);
            }
        }
        return ACC_OK;
    }

#ifdef ENABLE_IOV
    Result BlockRecvIOV(struct iovec *iov, int32_t len, int32_t totalDataLen) override
    {
        ASSERT_RETURN(fd_ != -1, ACC_CONNECTION_NOT_READY);
        ASSERT_RETURN(iov != nullptr, ACC_INVALID_PARAM);
        ASSERT_RETURN(len > 0, ACC_INVALID_PARAM);

        for (int32_t i = 0; i < len; i++) {
            ASSERT_RETURN(iov[i].iov_base != nullptr, ACC_INVALID_PARAM);
            ASSERT_RETURN(iov[i].iov_len > 0, ACC_INVALID_PARAM);
        }

        ssize_t result = 0;
        if (LIKELY(ssl_ == nullptr)) {
            result = ::readv(fd_, iov, len);
            if (LIKELY(result == totalDataLen)) {
                return ACC_OK;
            }
            auto errorNumber = errno;
            LOG_WARN("Failed to receive data from " << ipPort_ << ", errno " << errorNumber);
            return -errorNumber;
        } else {
            for (int32_t i = 0; i < len; i++) {
                auto callResult = BlockRecv(iov[i].iov_base, iov[i].iov_len);
                if (callResult != ACC_OK) {
                    LOG_ERROR("Failed to ssl read from " << ipPort_ << ", len " << iov[i].iov_len);
                    return callResult;
                }
            }
            return ACC_OK;
        }
    }
#endif

    Result PollingInput(int32_t timeoutInMs) const override
    {
        ASSERT_RETURN(fd_ != -1, ACC_CONNECTION_NOT_READY);

        ::pollfd pfd{};
        pfd.fd = fd_;
        pfd.events = POLLIN;

        while (true) {
            auto result = ::poll(&pfd, 1, timeoutInMs);
            if (result < 0 && errno == EINTR) { /* interrupted */
                continue;
            } else if (result == 0) { /* timeout */
                return ACC_TIMEOUT;
            } else if (result > 0) { /* poll active fd */
                return ACC_OK;
            } else { /* error */
                return ACC_ERROR;
            }
        }
    }

    Result SetSendTimeout(uint32_t timeoutInUs) const override
    {
        ASSERT_RETURN(fd_ != -1, ACC_CONNECTION_NOT_READY);

        struct timeval timeoutTV {};
        timeoutTV.tv_sec = static_cast<int64_t>(timeoutInUs) / (TIME_UNIT_INTERVAL * TIME_UNIT_INTERVAL);
        timeoutTV.tv_usec = static_cast<int64_t>(timeoutInUs) % (TIME_UNIT_INTERVAL * TIME_UNIT_INTERVAL);

        return ::setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char *>(&timeoutTV), sizeof(timeoutTV)) < 0 ?
            ACC_ERROR :
            ACC_OK;
    }

    Result SetReceiveTimeout(uint32_t timeoutInUs) const override
    {
        ASSERT_RETURN(fd_ != -1, ACC_CONNECTION_NOT_READY);

        struct timeval timeoutTV {};
        timeoutTV.tv_sec = static_cast<int64_t>(timeoutInUs) / (TIME_UNIT_INTERVAL * TIME_UNIT_INTERVAL);
        timeoutTV.tv_usec = static_cast<int64_t>(timeoutInUs) % (TIME_UNIT_INTERVAL * TIME_UNIT_INTERVAL);

        return ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&timeoutTV), sizeof(timeoutTV)) < 0 ?
                   ACC_ERROR :
                   ACC_OK;
    }

    Result EnableNoBlocking() const override
    {
        int32_t value = UNO_1;
        /* set blocking, fcntl result is 0 or -1 */
        if ((value = fcntl(fd_, F_GETFL, 0)) == -1) {
            LOG_ERROR("Failed to get control value of link " << ShortName() << ", errno:" << errno);
            return ACC_LINK_OPTION_ERROR;
        }

        if ((value = fcntl(fd_, F_SETFL, static_cast<uint32_t>(value)  &~O_NONBLOCK)) == -1) {
            LOG_ERROR("Failed to set control value of link " << ShortName() << ", errno:" << errno);
            return ACC_LINK_OPTION_ERROR;
        }

        return ACC_OK;
    }

    void Close() override
    {
        __sync_bool_compare_and_swap(&established_, 1, 0);
        if (ssl_ != nullptr) {
            if (AccCommonUtil::SslShutdownHelper(ssl_) != ACC_OK) {
                LOG_ERROR("shut down ssl failed!");
            }
            OpenSslApiWrapper::SslFree(ssl_);
            ssl_ = nullptr;
        }
        SafeCloseFd(fd_);
    }

    bool IsConnected() const override
    {
        tcp_info info;
        if (fd_ == -1) {
            return false;
        }
        int infoLen = sizeof(info);
        getsockopt(fd_, IPPROTO_TCP, TCP_INFO, &info, reinterpret_cast<socklen_t*>(&infoLen));
        return (info.tcpi_state == TCP_ESTABLISHED);
    }

    Result NonBlockSend(int16_t msgType, const AccDataBufferPtr &d, const AccDataBufferPtr &cbCtx) override
    {
        LOG_DEBUG("Not support non-blocking send, msgType " << msgType);
        return ACC_ERROR;
    }

    Result NonBlockSend(int16_t msgType, uint32_t seqNo, const AccDataBufferPtr &d,
                        const AccDataBufferPtr &cbCtx) override
    {
        LOG_DEBUG("Not support non-blocking send, msgType " << msgType << ", seqNo" << seqNo);
        return ACC_ERROR;
    }

    Result NonBlockSend(int16_t msgType, int16_t opCode, uint32_t seqNo,
                        const AccDataBufferPtr &d, const AccDataBufferPtr &cbCtx) override
    {
        LOG_DEBUG("Not support non-blocking send, msgType " << msgType << ", opCode " << opCode<<", seqNo" << seqNo);
        return ACC_ERROR;
    }

    Result EnqueueAndModifyEpoll(const AccMsgHeader &h, const AccDataBufferPtr &d,
                                 const AccDataBufferPtr &cbCtx) override
    {
        LOG_DEBUG("Not support non-blocking send, header " << h.ToString());
        return ACC_ERROR;
    }

protected:
    SSL* ssl_ = nullptr; /* ssl link ptr */

    friend class AccTcpWorker;
};
using AccTcpLinkDefaultPtr = AccRef<AccTcpLinkDefault>;
}  // namespace acc
}  // namespace ock

#endif  // ACC_LINKS_ACC_TCP_LINK_DEFAULT_H
