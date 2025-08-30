/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_DYNAMIC_RANKS_QP_MANAGER_H
#define MF_HYBRID_DYNAMIC_RANKS_QP_MANAGER_H
#include <atomic>
#include <thread>
#include <list>
#include <mutex>
#include <condition_variable>
#include <unordered_set>
#include "dl_hccp_def.h"
#include "device_qp_manager.h"
#include "dynamic_ranks_qp_def.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {

class DynamicRanksQpManager : public DeviceQpManager {
public:
    DynamicRanksQpManager(uint32_t deviceId, uint32_t rankId, uint32_t rankCount, sockaddr_in devNet,
                          bool server) noexcept;
    ~DynamicRanksQpManager() noexcept override;

    int SetRemoteRankInfo(const std::unordered_map<uint32_t, ConnectRankInfo> &ranks) noexcept override;
    int SetLocalMemories(const MemoryRegionMap &mrs) noexcept override;
    int Startup(void *rdma) noexcept override;
    void Shutdown() noexcept override;
    void *GetQpHandleWithRankId(uint32_t rankId) const noexcept override;

private:
    void BackgroundProcess() noexcept;
    int ProcessServerAddWhitelistTask() noexcept;
    int ProcessClientConnectSocketTask() noexcept;
    int ProcessQueryConnectionStateTask() noexcept;
    int ProcessConnectQpTask() noexcept;
    int ProcessQueryQpStateTask() noexcept;
    void ProcessUpdateLocalMrTask() noexcept;
    void ProcessUpdateRemoteMrTask() noexcept;
    void CloseServices() noexcept;

    std::vector<lite_mr_info> GenerateLocalLiteMrs() noexcept;
    std::vector<lite_mr_info> GenerateRemoteLiteMrs(uint32_t rankId) noexcept;
    void GenDiffInfoChangeRanks(const std::unordered_map<uint32_t, ConnectRankInfo> &last,
                                std::unordered_map<uint32_t, sockaddr_in> &addedRanks,
                                std::unordered_set<uint32_t> &addMrRanks) noexcept;
    void GenTaskFromChangeRanks(const std::unordered_map<uint32_t, sockaddr_in> &addedRanks,
                                const std::unordered_set<uint32_t> &addMrRanks) noexcept;
    static void SetQpHandleRegisterMr(void *qpHandle, const std::vector<lite_mr_info> &mrs, bool local) noexcept;

private:
    struct ConnectionChannel {
        in_addr remoteIp;
        void *socketHandle;
        void *socketFd{nullptr};
        void *qpHandle{nullptr};
        bool qpConnectCalled{false};
        int qpStatus{-1};

        explicit ConnectionChannel(const in_addr ip) : ConnectionChannel{ip, nullptr} {}
        ConnectionChannel(in_addr ip, void *sock) : remoteIp{ip}, socketHandle{sock} {}
    };

    void *rdmaHandle_{nullptr};
    std::unordered_map<uint32_t, ConnectRankInfo> currentRanksInfo_;
    MemoryRegionMap currentLocalMrs_;
    std::atomic<bool> managerRunning_{false};
    std::mutex mutex_;
    std::condition_variable cond_;
    std::shared_ptr<std::thread> backGroundThread_;
    ConnectionTasks connectionTasks_;
    std::unordered_map<uint32_t, ConnectionChannel> connections_;
    std::vector<ConnectionChannel *> connectionView_;
};
}
}
}
}

#endif  // MF_HYBRID_DYNAMIC_RANKS_QP_MANAGER_H
