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
#ifndef ACC_LINKS_ACC_TCP_LISTENER_H
#define ACC_LINKS_ACC_TCP_LISTENER_H

#include "mf_net.h"
#include "acc_includes.h"
#include "acc_tcp_common.h"
#include "acc_tcp_link.h"
#include "acc_tcp_link_complex_default.h"

namespace ock {
namespace acc {
using NewConnHandlerInner = std::function<int(const AccConnReq &reg, const AccTcpLinkComplexDefaultPtr &)>;

class AccTcpListener : public AccReferable {
public:
    AccTcpListener(std::string ip, uint16_t port, bool reusePort, bool enableTls = false, SSL_CTX *sslCtx = nullptr)
        : listenIp_(std::move(ip)), listenPort_(port), reusePort_(reusePort), enableTls_(enableTls), sslCtx_(sslCtx)
    {}

    ~AccTcpListener() override = default;

    void RegisterNewConnectionHandler(const NewConnHandlerInner &h);

    Result Start() noexcept;
    void Stop(bool afterFork = false) noexcept;

private:
    void RunInThread() noexcept;
    void ProcessNewConnection(int fd, struct sockaddr_in addressIn) noexcept;
    Result StartAcceptThread() noexcept;

    inline std::string NameAndPort() const noexcept;

private:
    int listenFd_ = -1;                         /* listen fd */
    volatile bool needStop_ = false;            /* stop thread flag */
    NewConnHandlerInner connHandler_ = nullptr; /* new connection handler */
    std::thread acceptThread_;                  /* accept thread */
    bool started_ = false;                      /* listener started or not */
    std::atomic<bool> threadStarted_{false};    /* flag to ensure thread started */
    const std::string listenIp_;                /* listen ip */
    const uint16_t listenPort_;                 /* listen port */
    const bool reusePort_;                      /* reuse listen port or not */
    const bool enableTls_;                      /* enable tls */
    SSL_CTX *sslCtx_ = nullptr;                 /* ssl ctx */
};
using AccTcpListenerPtr = AccRef<AccTcpListener>;

inline void AccTcpListener::RegisterNewConnectionHandler(const NewConnHandlerInner &h)
{
    ASSERT_RET_VOID(h != nullptr);
    ASSERT_RET_VOID(connHandler_ == nullptr);
    connHandler_ = h;
}

inline std::string AccTcpListener::NameAndPort() const noexcept
{
    return listenIp_ + ":" + std::to_string(listenPort_);
}
} // namespace acc
} // namespace ock

#endif // ACC_LINKS_ACC_TCP_LISTENER_H