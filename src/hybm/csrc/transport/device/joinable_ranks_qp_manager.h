/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef MF_HYBRID_JOINABLE_RANKS_QP_MANAGER_H
#define MF_HYBRID_JOINABLE_RANKS_QP_MANAGER_H

#include <set>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <vector>
#include "device_qp_manager.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
class JoinableRanksQpManager : public DeviceQpManager {
public:
    JoinableRanksQpManager(uint32_t userDeviceId, uint32_t deviceId, uint32_t rankId, uint32_t rankCount,
                           sockaddr_in devNet) noexcept;
    ~JoinableRanksQpManager() noexcept override;

    int SetRemoteRankInfo(const std::unordered_map<uint32_t, ConnectRankInfo> &ranks) noexcept override;
    int RemoveRanks(const std::unordered_set<uint32_t> &ranks) noexcept override;
    int Startup(void *rdma) noexcept override;
    void Shutdown() noexcept override;
    UserQpInfo *GetQpHandleWithRankId(uint32_t rankId) noexcept override;
    void PutQpHandle(UserQpInfo *qp) const noexcept override;
    bool CheckQpReady(const std::vector<uint32_t> &rankIds) const noexcept override;

private:
    void CloseServices() noexcept;
    int StartServerSide() noexcept;
    int StartClientSide() noexcept;
    void ServerSideRunLoop() noexcept;
    void ClientSideRunLoop() noexcept;
    void ServerSideHandleNewClients(const std::set<uint32_t> &newRanks) noexcept;
    int WaitSocketConnections(const std::set<uint32_t> &newRanks) noexcept;
    void MakeQpConnections(const std::set<uint32_t> &newRanks) noexcept;
    void WaitQpConnections(const std::set<uint32_t> &newRanks) noexcept;
    int GenerateWhiteList(const std::set<uint32_t> &newClients) noexcept;
    int CreateConnectionToServers(const std::set<uint32_t> &newServers) noexcept;
    void RemoveRanksProcess(const std::set<uint32_t> &ranks) noexcept;

private:
    std::atomic<bool> started_{false};
    std::atomic<bool> running_{true};
    std::shared_ptr<std::thread> clientConnectThread_;
    std::shared_ptr<std::thread> serverConnectThread_;
    void *rdmaHandle_{nullptr};
    MemoryRegionMap currentLocalMrs_;
    std::vector<UserQpInfo *> qpArray_;
    ReadWriteLock qpLock_;
    std::vector<ConnectionChannel> connections_; // connections_操作由mutex_保证不并发
    std::mutex mutex_;
    std::condition_variable cond_;
    std::set<uint32_t> newClients_;
    std::set<uint32_t> newServers_;
    std::set<uint32_t> removedClientRanks_;
    std::set<uint32_t> removedServerRanks_;
    uint32_t userDeviceId_{0};
};

}  // namespace device
}  // namespace transport
}  // namespace mf
}  // namespace ock
#endif  // MF_HYBRID_JOINABLE_RANKS_QP_MANAGER_H
