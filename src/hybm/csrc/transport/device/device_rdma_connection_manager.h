/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_DEVICE_RDMA_CONNECTION_MANAGER_H
#define MF_HYBRID_DEVICE_RDMA_CONNECTION_MANAGER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <set>
#include <vector>
#include <thread>
#include <unordered_set>
#include <unordered_map>

#include "hybm_types.h"
#include "hybm_define.h"
#include "hybm_transport_common.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
struct ConnectRankInfo {
    sockaddr_in network;
    std::vector<TransportMemoryKey> memories;

    ConnectRankInfo(sockaddr_in nw, TransportMemoryKey mk) : network{std::move(nw)}, memories{std::move(mk)} {}
    ConnectRankInfo(sockaddr_in nw, std::vector<TransportMemoryKey> mks)
        : network{std::move(nw)},
          memories{std::move(mks)}
    {
    }
};

struct ConnectionChannel {
    in_addr remoteIp;
    void *socketHandle;
    void *socketFd{nullptr};
    void *qpHandle{nullptr};
    HccpAiQpInfo aiQpInfo{};
    int qpStatus{-1};

    explicit ConnectionChannel(const in_addr ip) : ConnectionChannel{ip, nullptr} {}
    ConnectionChannel(in_addr ip, void *sock) : remoteIp{ip}, socketHandle{sock} {}
};

class RdmaConnectionManager {
public:
    RdmaConnectionManager(uint32_t deviceId, uint32_t rankId, uint32_t rankCount, sockaddr_in devNet);
    ~RdmaConnectionManager();
    Result PrepareConnection(void *rmda);
    Result StartServerListen(const std::unordered_map<uint32_t, ConnectRankInfo> &ranks);
    void StopServerListen();
    Result ClientConnectServers();
    void CloseClientConnections(bool closeAll = true);
    Result WaitServerSideConnected();
    Result UpdateRanks(const std::unordered_map<uint32_t, ConnectRankInfo> &ranks);
    const AiQpRMAQueueInfo *GetAiQpRMAQueueInfo() const;

private:
    void GenerateNewAdds(const std::unordered_map<uint32_t, ConnectRankInfo> &ranks);
    int StartServerCommon();
    int GenerateWhiteList();
    void *CreateLocalSocket();
    int CreateServerSocket();
    int WaitConnectionsReady(std::unordered_map<std::string, ConnectionChannel> &connections);
    int CreateQpWaitingReady(std::unordered_map<std::string, ConnectionChannel> &connections);
    bool ReserveQpInfoSpace();
    void ReleaseQpInfoSpace();
    int FillQpInfo();
    void CopyAiWQInfo(struct AiQpRMAWQ &dest, const struct ai_data_plane_wq &source, DBMode dbMode, uint32_t sl);
    void CopyAiCQInfo(struct AiQpRMACQ &dest, const ai_data_plane_cq &source, DBMode dbMode);

private:
    const uint32_t deviceId_;
    const uint32_t rankId_;
    const uint32_t rankCount_;
    const sockaddr_in deviceAddress_;

    volatile int serverConnectResult{-1};
    std::shared_ptr<std::thread> serverConnectThread_;
    std::set<uint32_t> addedRanks_;
    std::unordered_set<std::string> addedNetworks_;
    std::unordered_map<uint32_t, ConnectRankInfo> currentRanksInfo_;
    uint32_t qpInfoSize_{0};
    AiQpRMAQueueInfo *qpInfo_{nullptr};
    void *rdmaHandle_{nullptr};
    void *serverSocketHandle_{nullptr};
    std::unordered_map<std::string, ConnectionChannel> clientConnections_;
    std::unordered_map<std::string, ConnectionChannel> serverConnections_;
};
}
}
}
}

#endif // MF_HYBRID_DEVICE_RDMA_CONNECTION_MANAGER_H
