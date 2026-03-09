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
#ifndef ACC_LINKS_ACC_TCP_COMMON_H
#define ACC_LINKS_ACC_TCP_COMMON_H

#include <fcntl.h>
#include <functional>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unordered_map>
#include <utility>

#include "acc_includes.h"

namespace ock {
namespace acc {
/**
 * @brief Options of worker
 */
struct AccTcpWorkerOptions {
    uint16_t pollingTimeoutMs = UNO_500; /* poll/epoll timeout */
    uint16_t index = 0;                  /* index of the worker */
    int16_t cpuId = -1;                  /* cpu id for bounding */
    int16_t threadPriority = -1;         /* thread nice */
    std::string name_ = "AccWrk";        /* worker name */

    inline std::string ToString() const
    {
        std::ostringstream oss;
        oss << "name " << name_ << ", index " << index << ", cpu " << cpuId << ", thread-priority " << threadPriority
            << ", poll-timeout-ms " << pollingTimeoutMs;
        return oss.str();
    }

    inline std::string Name() const
    {
        return name_ + ":" + std::to_string(index);
    }
};

/**
 * @brief Close fd in safe way, to avoid double close
 *
 * @param fd               [in] fd to be closed
 */
void SafeCloseFd(int &fd, bool needShutdown = true);

inline ssize_t SocketSendNoSignal(int fd, const void *buf, size_t len, int flags = 0)
{
#ifdef MSG_NOSIGNAL
    return ::send(fd, buf, len, flags | MSG_NOSIGNAL);
#else
    return ::send(fd, buf, len, flags);
#endif
}

inline ssize_t SocketSendVectorNoSignal(int fd, const struct iovec *iov, int iovcnt)
{
#ifdef MSG_NOSIGNAL
    struct msghdr msg {};
    msg.msg_iov = const_cast<struct iovec *>(iov);
    msg.msg_iovlen = iovcnt;
    return ::sendmsg(fd, &msg, MSG_NOSIGNAL);
#else
    return ::writev(fd, iov, iovcnt);
#endif
}

constexpr int16_t MIN_MSG_TYPE = 0;
constexpr int16_t MAX_MSG_TYPE = UNO_48;
constexpr uint32_t ACC_LINK_RECV_TIMEOUT = 1800;
} // namespace acc
} // namespace ock

#endif // ACC_LINKS_ACC_TCP_COMMON_H
