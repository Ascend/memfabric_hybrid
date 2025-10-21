/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MF_HYBRID_DYNAMIC_RANKS_QP_DEF_H
#define MF_HYBRID_DYNAMIC_RANKS_QP_DEF_H

#include <netinet/in.h>
#include <cstdint>
#include <mutex>
#include <unordered_set>
#include <unordered_map>

namespace ock {
namespace mf {
namespace transport {
namespace device {
struct TaskStatus {
    bool exist{false};
    int64_t failedTimes{0};
};

// (1)
struct ServerAddWhitelistTask {
    TaskStatus status;
    std::mutex locker;
    std::unordered_map<uint32_t, net_addr_t> remoteIps;

    inline int64_t Failed(const std::unordered_map<uint32_t, net_addr_t> &ips) noexcept
    {
        std::unique_lock<std::mutex> uniqueLock{locker};
        remoteIps = ips;
        status.exist = true;
        return ++status.failedTimes;
    }

    inline void Success() noexcept
    {
        std::unique_lock<std::mutex> uniqueLock{locker};
        status.exist = false;
        status.failedTimes = 0;
    }
};

// (2)
struct ClientConnectSocketTask {
    TaskStatus status;
    std::mutex locker;
    std::unordered_map<uint32_t, mf_sockaddr> remoteAddress;

    inline int64_t Failed(const std::unordered_map<uint32_t, mf_sockaddr> &address) noexcept
    {
        std::unique_lock<std::mutex> uniqueLock{locker};
        status.exist = true;
        remoteAddress = address;
        return ++status.failedTimes;
    }

    inline void Success() noexcept
    {
        std::unique_lock<std::mutex> uniqueLock{locker};
        status.exist = false;
        status.failedTimes = 0;
    }
};

// (3)
struct QueryConnectionStateTask {
    TaskStatus status;
    std::unordered_map<net_addr_t, uint32_t> ip2rank;
    inline int64_t Failed(const std::unordered_map<net_addr_t, uint32_t> &p2r) noexcept
    {
        ip2rank = p2r;
        status.exist = true;
        return ++status.failedTimes;
    }
};

// (4)
struct ConnectQpTask {
    TaskStatus status;
    std::unordered_set<uint32_t> ranks;
    inline int64_t Failed(const std::unordered_set<uint32_t> &rks) noexcept
    {
        ranks = rks;
        status.exist = true;
        return ++status.failedTimes;
    }
};

// (5)
struct QueryQpStateTask {
    TaskStatus status;
    std::unordered_set<uint32_t> ranks;
    inline int64_t Failed(const std::unordered_set<uint32_t> &rks) noexcept
    {
        ranks = rks;
        status.exist = true;
        return ++status.failedTimes;
    }
};

// (6)
struct UpdateLocalMrTask {
    TaskStatus status;
    std::mutex locker;
    inline int64_t Failed() noexcept
    {
        std::unique_lock<std::mutex> uniqueLock{locker};
        status.exist = true;
        return ++status.failedTimes;
    }

    inline void Success() noexcept
    {
        std::unique_lock<std::mutex> uniqueLock{locker};
        status.exist = false;
        status.failedTimes = 0;
    }
};
// (7)
struct UpdateRemoteMrTask {
    TaskStatus status;
    std::mutex locker;
    std::unordered_set<uint32_t> addedMrRanks;
    inline int64_t Failed() noexcept
    {
        std::unique_lock<std::mutex> uniqueLock{locker};
        status.exist = true;
        return ++status.failedTimes;
    }

    inline void Success() noexcept
    {
        std::unique_lock<std::mutex> uniqueLock{locker};
        status.exist = false;
        status.failedTimes = 0;
    }
};

struct ConnectionTasks {
    ServerAddWhitelistTask whitelistTask;
    ClientConnectSocketTask clientConnectTask;
    QueryConnectionStateTask queryConnectTask;
    ConnectQpTask connectQpTask;
    QueryQpStateTask queryQpStateTask;
    UpdateLocalMrTask updateMrTask;
    UpdateRemoteMrTask updateRemoteMrTask;
};
}
}
}
}

#endif  // MF_HYBRID_DYNAMIC_RANKS_QP_DEF_H
