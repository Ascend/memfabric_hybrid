/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_HYBM_RDMA_TRANS_MANAGER_H
#define MF_HYBRID_HYBM_RDMA_TRANS_MANAGER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <atomic>
#include <string>
#include <mutex>
#include <condition_variable>
#include <unordered_map>

#include "hybm_trans_conn_prdma.h"
#include "hybm_transport_manager.h"

namespace ock {
namespace mf {

struct ChannelConnection {
    in_addr remoteIp;
    void *socketHandle;
    void *socketFd{nullptr};
    void *qpHandle{nullptr};
    HccpAiQpInfo aiQpInfo{};
    int qpStatus{-1};

    explicit ChannelConnection(const in_addr ip) : ChannelConnection{ip, nullptr} {}
    ChannelConnection(in_addr ip, void *sock) : remoteIp{ip}, socketHandle{sock} {}
};

/**
 * @brief RdmaTransportManager running state
 * @details
 *   client side:
 *     IDLE -> INIT -> SOCKET_CONNECTING -> SOCKET_CONNECTED -> QP_CONNECTING -> READY -> EXITING
 *   server side:
 *     IDLE -> INIT -> SOCKET_LISTENING -> SOCKET_ACCEPTING -> SOCKET_CONNECTED -> QP_CONNECTING -> READY -> EXITING
 */
enum RdmaManagerState {
    RDMA_IDLE = 0,
    RDMA_INIT,
    RDMA_SOCKET_CONNECTING,
    RDMA_SOCKET_LISTENING,
    RDMA_SOCKET_ACCEPTING,
    RDMA_SOCKET_CONNECTED,
    RDMA_QP_CONNECTING,
    RDMA_READY,
    RDMA_EXITING,
    RDMA_STATE_BUTT
};

class RdmaTransportManager : public TransportManager {
public:
    RdmaTransportManager(uint32_t deviceId, uint32_t port);
    TransHandlePtr OpenDevice(const TransDeviceOptions &options) override;
    void CloseDevice(const TransHandlePtr &h) override;
    uint64_t GetTransportId() const override;
    void SetTransportIds(const std::vector<uint64_t> transports) override;
    Result RegMemToDevice(const TransHandlePtr &h, const TransMemRegInput &in, TransMemRegOutput &out) override;
    Result UnRegMemFromDevice(const TransMemRegOutput &out) override;
    Result SetGlobalRegisterMrInfo(const std::vector<RdmaMemRegionInfo> &mrs) override;
    Result PrepareDataConn(const TransPrepareOptions &options) override;
    void UnPrepareDataConn() override;
    Result CreateDataConn(const TransDataConnOptions &options) override;
    void CloseAllDataConn() override;
    bool IsReady() override;
    Result WaitingReady(int64_t timeoutNs) override;
    std::vector<TransDataConnPtr> GetDataConn() override;
    TransDataConnAddressInfo GetDataConnAddrInfo() override;

private:
    bool OpenTsd();
    bool RaInit();
    bool RetireDeviceIp();
    bool RaRdevInit();
    void SetClientState(RdmaManagerState state);
    void SetServerState(RdmaManagerState state);
    int CreateQpWaitingReady(std::unordered_map<std::string, ChannelConnection> &connections);
    int FillQpInfo();
    void CopyAiWQInfo(struct AiQpRMAWQ& dest, const struct ai_data_plane_wq& source, DBMode dbMode, uint32_t sl);
    void CopyAiCQInfo(struct AiQpRMACQ& dest, const ai_data_plane_cq& source, DBMode dbMode);
    int WaitConnectionsReady(std::unordered_map<std::string, ChannelConnection> &connections);

private:
    const uint32_t listenPort_;
    const uint32_t deviceId_;
    uint32_t localRankId_{0};
    uint32_t totalRankCount_{1};

    std::vector<uint64_t> clusterTransports_;
    std::vector<RdmaMemRegionInfo> rdmaMemRegionInfos_;
    std::mutex stateMutex_;
    std::condition_variable stateCond_;
    RdmaManagerState clientState_{RDMA_IDLE};
    RdmaManagerState serverState_{RDMA_IDLE};
    in_addr deviceIp_;
    void *rdmaHandle_{nullptr};
    void *serverSocketHandle_{nullptr};

    size_t qpInfoSize_{0};
    AiQpRMAQueueInfo *qpInfo_{nullptr};
    std::unordered_map<std::string, ChannelConnection> clientConnections_;
    std::unordered_map<std::string, ChannelConnection> serverConnections_;
};
}
}

#endif  // MF_HYBRID_HYBM_RDMA_TRANS_MANAGER_H
