/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <net/if.h>
#include <sys/time.h>
#include "acc_common_util.h"
#include "acc_tcp_listener.h"

namespace ock {
namespace acc {
    
void AccTcpListener::PrepareSockAddr(mf_sockaddr& addr) noexcept
{
    if (addr.type == IpV4) {
        addr.ip.ipv4.sin_family = AF_INET;
        addr.ip.ipv4.sin_addr.s_addr = inet_addr(listenIp_.c_str());
        addr.ip.ipv4.sin_port = htons(listenPort_);
    } else if (addr.type == IpV6) {
        addr.ip.ipv6.sin6_family = AF_INET6;
        addr.ip.ipv6.sin6_port = htons(listenPort_);
        inet_pton(AF_INET6, listenIp_.c_str(), &addr.ip.ipv6.sin6_addr);
    }
}

Result AccTcpListener::CreateSocketForStrat(mf_sockaddr &addr, int &tmpFD) noexcept
{
    if (listenIp_.find(':') != std::string::npos) {
        tmpFD = ::socket(AF_INET6, SOCK_STREAM, 0);
        addr.type = IpV6;
    } else {
        tmpFD = ::socket(AF_INET, SOCK_STREAM, 0);
        addr.type = IpV4;
    }
    if (tmpFD < 0) {
        LOG_ERROR("Failed to create listen socket, error " << strerror(errno) <<
            ", please check if running of fd limit");
        return ACC_ERROR;
    }
    ipType_ = addr.type;
    return ACC_OK;
}

Result AccTcpListener::BindAndListenSocket(int tmpFD, mf_sockaddr &addr) noexcept
{
    int result_bind = -1;
    if (addr.type == IpV4) {
        result_bind = ::bind(tmpFD, reinterpret_cast<struct sockaddr *>(&addr.ip.ipv4), sizeof(addr.ip.ipv4));
    } else if (addr.type == IpV6) {
        result_bind = ::bind(tmpFD, reinterpret_cast<struct sockaddr *>(&addr.ip.ipv6), sizeof(addr.ip.ipv6));
    }
    if (result_bind < 0 || ::listen(tmpFD, 200L) < 0) {
        auto errorNum = errno;
        SafeCloseFd(tmpFD);
        if (errorNum == EADDRINUSE) {
            LOG_INFO("address in use for bind listen on " << NameAndPort());
            return ACC_LINK_ADDRESS_IN_USE;
        }
        LOG_ERROR("Failed to bind or listen on " << NameAndPort() << " as errno " << strerror(errorNum));
        return ACC_ERROR;
    }
    return ACC_OK;
}

Result AccTcpListener::Start() noexcept
{
    if (started_) {
        LOG_INFO("AccTcpListener at " << NameAndPort() << " already started");
        return ACC_OK;
    }

    if (connHandler_ == nullptr) {
        LOG_ERROR("Invalid connection handler");
        return ACC_INVALID_PARAM;
    }

    /* create socket */
    mf_sockaddr addr {};
    auto tmpFD {-1};
    if (CreateSocketForStrat(addr, tmpFD) != ACC_OK) {
        return ACC_ERROR;
    }
    /* assign address */
    PrepareSockAddr(addr);

    /* set option, bind and listen */
    if (reusePort_) {
        int flags = 1;
        if (::setsockopt(tmpFD, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<void *>(&flags), sizeof(flags)) < 0) {
            SafeCloseFd(tmpFD);
            LOG_ERROR("Failed to set reuse port of " << NameAndPort() << " as " << strerror(errno));
            return ACC_ERROR;
        }
    }

    auto isListen = BindAndListenSocket(tmpFD, addr);
    if (isListen != ACC_OK) {
        return isListen;
    }

    auto ret = StartAcceptThread();
    if (ret != ACC_OK) {
        SafeCloseFd(tmpFD);
        return ret;
    }

    listenFd_ = tmpFD;

    int retry_times = 10000;
    while (!threadStarted_.load()) {
        usleep(100L);
        retry_times -= 1;
        if (retry_times == 0) {
            LOG_ERROR("Internal thread start timeout.");
            return ACC_ERROR;
        }
    }

    started_ = true;
    return ACC_OK;
}

Result AccTcpListener::StartAcceptThread() noexcept
{
    threadStarted_.store(false);

    try {
        acceptThread_ = std::thread([this]() {
            this->RunInThread();
        });
    } catch (const std::system_error& e) {
        LOG_ERROR("Failed to create accept thread: " << e.what());
        return ACC_ERROR;
    } catch (...) {
        LOG_ERROR("Unknown error creating accept thread");
        return ACC_ERROR;
    }

    std::string thrName = "AccListener";
    if (pthread_setname_np(acceptThread_.native_handle(), thrName.c_str()) != 0) {
        LOG_WARN("Failed to set thread name of oob tcp server");
    }

    return ACC_OK;
}

void AccTcpListener::Stop(bool afterFork) noexcept
{
    if (!started_) {
        return;
    }

    needStop_ = true;
    if (acceptThread_.joinable()) {
        if (afterFork) {
            acceptThread_.detach();
        } else {
            acceptThread_.join();
        }
    }

    SafeCloseFd(listenFd_, !afterFork);

    started_ = false;
}

void AccTcpListener::RunInThread() noexcept
{
    LOG_INFO("Acc listener accept thread for " << NameAndPort() << " start ...");
    threadStarted_.store(true);

    while (!needStop_) {
        try {
            struct pollfd pollEventFd = {};
            pollEventFd.fd = listenFd_;
            pollEventFd.events = POLLIN;
            pollEventFd.revents = 0;

            int rc = poll(&pollEventFd, 1, 500L);
            if (rc < 0 && errno != EINTR) {
                LOG_ERROR("Get poll event failed  , errno " << strerror(errno));
                break;
            } else if (needStop_) {
                LOG_WARN("Acc listener accept thread get stop signal, will exit...");
                break;
            } else if (rc == 0) {
                continue;
            }

            mf_sockaddr addressIn {};
            auto fd {-1};
            if (ipType_ == IpV6) {
                socklen_t len = sizeof(sockaddr_in6);
                fd = ::accept(listenFd_, reinterpret_cast<struct sockaddr *>(&addressIn.ip.ipv6), &len);
            } else if (ipType_ == IpV4) {
                socklen_t len = sizeof(sockaddr_in);
                fd = ::accept(listenFd_, reinterpret_cast<struct sockaddr *>(&addressIn.ip.ipv4), &len);
            }
            if (fd < 0) {
                LOG_WARN("Failed to accept on new socket with " << strerror(errno) << ", ignore and continue");
                continue;
            }

            int flags = 1;
            setsockopt(fd, SOL_TCP, TCP_NODELAY, &flags, sizeof(flags));

            struct timeval timeout = {ACC_LINK_RECV_TIMEOUT, 0};
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

            ProcessNewConnection(fd, addressIn);
        } catch (std::exception &ex) {
            LOG_WARN("Got exception in AccTcpListener::RunInThread, exception " << ex.what() <<
                ", ignore and continue");
        } catch (...) {
            LOG_WARN("Got unknown error in AccTcpListener::RunInThread, ignore and continue");
        }
    }

    LOG_INFO("Working thread for AccTcpStore listener at " << NameAndPort() << " exiting");
}

void AccTcpListener::FormatIPAddressAndPort(mf_sockaddr addressIn, std::string &ipPort) noexcept
{
    if (ipType_ == IpV6) {
        char ipStr[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &(addressIn.ip.ipv6.sin6_addr), ipStr, INET6_ADDRSTRLEN);
        ipPort = ipStr;
        ipPort += ":";
        ipPort += std::to_string(ntohs(addressIn.ip.ipv6.sin6_port));
    } else if (ipType_ == IpV4) {
        ipPort = inet_ntoa(addressIn.ip.ipv4.sin_addr);
        ipPort += ":";
        ipPort += std::to_string(ntohs(addressIn.ip.ipv4.sin_port));
    }
}

void AccTcpListener::ProcessNewConnection(int fd, mf_sockaddr addressIn) noexcept
{
    std::string ipPort {};
    FormatIPAddressAndPort(addressIn, ipPort);

    /* receive header */
    AccConnReq req;
    auto received = ::recv(fd, &req, sizeof(req), 0);
    if (received != sizeof(req)) {
        LOG_ERROR("Failed to read header from the socket connected from " << ipPort);
        SafeCloseFd(fd);
        return;
    }

    SSL *ssl = nullptr;
    if (enableTls_) {
        auto ret = AccTcpSslHelper::NewSslLink(true, fd, sslCtx_, ssl);
        if (ret != ACC_OK) {
            LOG_ERROR("Failed to new connection ssl link");
            SafeCloseFd(fd);
            return ;
        }
    }

    LOG_INFO("Connected from " << ipPort << " successfully, ssl " << (enableTls_ ? "enable" : "disable"));
    auto newLink = AccMakeRef<AccTcpLinkComplexDefault>(fd, ipPort, AccTcpLinkDefault::NewId(), ssl);
    if (newLink == nullptr) {
        LOG_ERROR("Failed to create listener tcp link object, probably out of memory");
        if (ssl != nullptr) {
            if (AccCommonUtil::SslShutdownHelper(ssl) != ACC_OK) {
                LOG_ERROR("shut down ssl failed!");
            }
            OpenSslApiWrapper::SslFree(ssl);
            ssl = nullptr;
        }
        SafeCloseFd(fd);
        return;
    }

    // tmpLink作为智能指针 异常分支返回时会自动析构释放资源
    auto result = connHandler_(req, newLink.Get());
    if (result != ACC_OK) {
        return;
    }

    AccConnResp resp;
    resp.result = 0;
    auto sent = newLink->BlockSend(reinterpret_cast<void *>(&resp), sizeof(resp));
    if (sent != ACC_OK) {
        LOG_WARN("Failed to connect response to " << ipPort);
    }
}
}
}