/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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
    FixedRanksQpManager(uint32_t deviceId, uint32_t rankId, uint32_t rankCount, mf_sockaddr devNet) noexcept;
    ~FixedRanksQpManager() noexcept override;

    int SetRemoteRankInfo(const std::unordered_map<uint32_t, ConnectRankInfo> &ranks) noexcept override;
    int SetLocalMemories(const MemoryRegionMap &mrs) noexcept override;
    int Startup(void *rdma) noexcept override;
    void Shutdown() noexcept override;
    int WaitingConnectionReady() noexcept override;
    const void *GetQpInfoAddress() const noexcept override;
    void *GetQpHandleWithRankId(uint32_t rankId) const noexcept override;

private:
    enum ConnQpType : uint32_t {
        CONN_QP_AI_CORE,  // AI core使用的QP
        CONN_QP_STARS,    // Host侧使用STARS驱动的QP
        CONN_QP_COUNT
    };

    struct ConnectionChannel {
        net_addr_t remoteIp;
        void *socketHandle;
        void *socketFd{nullptr};
        void *qpHandles[CONN_QP_COUNT]{};
        HccpAiQpInfo aiQpInfo{};
        int qpStatus{-1};

        explicit ConnectionChannel(const net_addr_t ip) : ConnectionChannel{ip, nullptr} {}
        ConnectionChannel(net_addr_t ip, void *sock) : remoteIp{ip}, socketHandle{sock} {}
    };

    bool ReserveQpInfoSpace() noexcept;
    int StartServerSide() noexcept;
    int StartClientSide() noexcept;
    int GenerateWhiteList() noexcept;
    int WaitConnectionsReady(std::unordered_map<uint32_t, ConnectionChannel> &connections) noexcept;
    int CreateQpWaitingReady(std::unordered_map<uint32_t, ConnectionChannel> &connections, ConnQpType qpType) noexcept;
    int CreateOneQp(ConnQpType qpType, ConnectionChannel &channel) noexcept;
    int FillQpInfo(ConnQpType qpType) noexcept;
    void CopyAiWQInfo(struct AiQpRMAWQ &dest, const struct ai_data_plane_wq &src, DBMode dbMode, uint32_t sl) noexcept;
    void CopyAiCQInfo(struct AiQpRMACQ &dest, const ai_data_plane_cq &source, DBMode dbMode) noexcept;
    void CloseServices() noexcept;
    void CloseClientConnections() noexcept;
    void CloseServerConnections() noexcept;
    void CloseConnections(std::unordered_map<uint32_t, ConnectionChannel> &connections) noexcept;
    int CheckConnectionSuccessCount(std::unordered_map<uint32_t, ConnectionChannel> &connections,
        std::vector<HccpSocketInfo> &socketInfos, std::unordered_map<net_addr_t, uint32_t> &addr2index,
        uint32_t &succCnt, IpType type);
    void InitClientConnectThread();
    void FillQpPreSettingCopyInfo(AiQpRMAQueueInfo *&copyInfo);
    void FillQpPostSettingCopyInfo(AiQpRMAQueueInfo *&copyInfo);

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
    std::unordered_map<uint32_t, ConnectionChannel> clientConnections_;
    std::unordered_map<uint32_t, ConnectionChannel> serverConnections_;
};
}
}
}
}

#endif  // MF_HYBRID_FIXED_RANKS_QP_MANAGER_H
