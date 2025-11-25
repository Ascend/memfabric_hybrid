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

#ifndef MF_HYBRID_FIXED_RANKS_QP_MANAGER_H
#define MF_HYBRID_FIXED_RANKS_QP_MANAGER_H

#include <atomic>
#include <thread>
#include "device_qp_manager.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {

class FixedRanksQpManager : public DeviceQpManager {
public:
    FixedRanksQpManager(uint32_t deviceId, uint32_t rankId, uint32_t rankCount, sockaddr_in devNet) noexcept;
    ~FixedRanksQpManager() noexcept override;

    int SetRemoteRankInfo(const std::unordered_map<uint32_t, ConnectRankInfo> &ranks) noexcept override;
    int Startup(void *rdma) noexcept override;
    void Shutdown() noexcept override;
    int WaitingConnectionReady() noexcept override;
    const void *GetQpInfoAddress() const noexcept override;
    UserQpInfo *GetQpHandleWithRankId(uint32_t rankId) noexcept override;
    void PutQpHandle(UserQpInfo *qp) const noexcept override;

private:
    struct AiCoreConnChannel {
        in_addr remoteIp;
        void *socketHandle;
        void *socketFd{nullptr};
        void *qpHandle{};
        HccpAiQpInfo aiQpInfo{};
        int qpStatus{-1};

        explicit AiCoreConnChannel(const in_addr ip) : AiCoreConnChannel{ip, nullptr} {}
        AiCoreConnChannel(in_addr ip, void *sock) : remoteIp{ip}, socketHandle{sock} {}
    };

    bool ReserveQpInfoSpace() noexcept;
    void ReleaseQpInfoSpace() noexcept;
    int StartServerSide() noexcept;
    int StartClientSide() noexcept;
    int GenerateWhiteList() noexcept;
    int WaitConnectionsReady(std::unordered_map<uint32_t, AiCoreConnChannel> &connections) noexcept;
    int CheckReadyConnection(std::unordered_map<uint32_t, AiCoreConnChannel> &connections,
                             const std::unordered_map<in_addr_t, uint32_t> addr2index,
                             const HccpSocketInfo &socketInfo) noexcept;
    int CreateQpWaitingReady(std::unordered_map<uint32_t, AiCoreConnChannel> &connections) noexcept;
    int CreateOneQp(AiCoreConnChannel &channel) noexcept;
    int FillQpInfo() noexcept;
    void CopyAiWQInfo(struct AiQpRMAWQ &dest, const struct ai_data_plane_wq &src, DBMode dbMode, uint32_t sl) noexcept;
    void CopyAiCQInfo(struct AiQpRMACQ &dest, const ai_data_plane_cq &source, DBMode dbMode) noexcept;
    void CloseServices() noexcept;
    void CloseClientConnections() noexcept;
    void CloseServerConnections() noexcept;
    void CloseConnections(std::unordered_map<uint32_t, AiCoreConnChannel> &connections) noexcept;

private:
    std::atomic<bool> started_{false};
    std::atomic<int> serverConnectResult{-1};
    std::atomic<int> clientConnectResult{-1};
    uint32_t qpInfoSize_{0};
    void *rdmaHandle_{nullptr};
    std::unordered_map<uint32_t, ConnectRankInfo> currentRanksInfo_;
    MemoryRegionMap currentLocalMrs_;
    AiQpRMAQueueInfo *qpInfo_{nullptr};
    std::shared_ptr<std::thread> clientConnectThread_;
    std::shared_ptr<std::thread> serverConnectThread_;
    std::unordered_map<uint32_t, AiCoreConnChannel> clientConnections_;
    std::unordered_map<uint32_t, AiCoreConnChannel> serverConnections_;
};
}
}
}
}

#endif  // MF_HYBRID_FIXED_RANKS_QP_MANAGER_H
