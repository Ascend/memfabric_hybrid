/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <sys/time.h>
#include "acc_common_util.h"
#include "acc_tcp_listener.h"

namespace ock {
namespace acc {
    
void AccTcpListener::PrepareSockAddr(struct sockaddr_in& addr) noexcept
{
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(listenIp_.c_str());
    addr.sin_port = htons(listenPort_);
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
    auto tmpFD = ::socket(AF_INET, SOCK_STREAM, 0);
    if (tmpFD < 0) {
        LOG_ERROR("Failed to create listen socket, error " << strerror(errno) <<
            ", please check if running of fd limit");
        return ACC_ERROR;
    }

    /* assign address */
    struct sockaddr_in addr {};
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

    if (::bind(tmpFD, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0 || ::listen(tmpFD, 200L) < 0) {
        auto errorNum = errno;
        SafeCloseFd(tmpFD);
        if (errorNum == EADDRINUSE) {
            LOG_INFO("address in use for bind listen on " << NameAndPort());
            return ACC_LINK_ADDRESS_IN_USE;
        }
        LOG_ERROR("Failed to bind or listen on " << NameAndPort() << " as errno " << strerror(errorNum));
        return ACC_ERROR;
    }

    auto ret = StartAcceptThread();
    if (ret != ACC_OK) {
        SafeCloseFd(tmpFD);
        return ret;
    }

    listenFd_ = tmpFD;

    while (!threadStarted_.load()) {
        usleep(100L);
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

            struct sockaddr_in addressIn {};
            socklen_t len = sizeof(addressIn);
            auto fd = ::accept(listenFd_, reinterpret_cast<struct sockaddr *>(&addressIn), &len);
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

void AccTcpListener::ProcessNewConnection(int fd, struct sockaddr_in addressIn) noexcept
{
    std::string ipPort = inet_ntoa(addressIn.sin_addr);
    ipPort += ":";
    ipPort += std::to_string(ntohs(addressIn.sin_port));

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