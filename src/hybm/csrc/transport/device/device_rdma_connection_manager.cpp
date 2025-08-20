/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <thread>
#include <chrono>
#include <algorithm>

#include "hybm_logger.h"
#include "dl_acl_api.h"
#include "dl_hccp_api.h"
#include "device_rdma_common.h"
#include "device_rdma_connection_manager.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
RdmaConnectionManager::RdmaConnectionManager(uint32_t deviceId, uint32_t rankId, uint32_t rankCount, sockaddr_in devNet)
    : deviceId_{deviceId},
      rankId_{rankId},
      rankCount_{rankCount},
      deviceAddress_(devNet)
{
}

RdmaConnectionManager::~RdmaConnectionManager()
{
    CloseClientConnections();
    StopServerListen();
    ReleaseQpInfoSpace();
}

Result RdmaConnectionManager::PrepareConnection(void *rmda)
{
    if (rmda == nullptr) {
        BM_LOG_ERROR("start server listen with rdma handle is null.");
        return BM_INVALID_PARAM;
    }
    rdmaHandle_ = rmda;

    if (!ReserveQpInfoSpace()) {
        BM_LOG_ERROR("reserve qp info space failed.");
        return BM_ERROR;
    }

    BM_LOG_INFO("PrepareConnection finished.");
    return BM_OK;
}

Result RdmaConnectionManager::StartServerListen(const std::unordered_map<uint32_t, ConnectRankInfo> &ranks)
{
    GenerateNewAdds(ranks);
    if (rankId_ + 1U == rankCount_) {
        BM_LOG_INFO("rank(" << rankId_ << ") largest need not listen.");
        return BM_OK;
    }

    auto ret = StartServerCommon();
    if (ret != BM_OK) {
        BM_LOG_ERROR("start server socket failed: " << ret);
        return ret;
    }

    BM_LOG_INFO("start to listen on port: " << deviceAddress_.sin_port << " success.");
    return BM_OK;
}

void RdmaConnectionManager::StopServerListen()
{
    if (serverSocketHandle_ == nullptr) {
        BM_LOG_INFO("Not Prepared.");
        return;
    }

    HccpSocketListenInfo listenInfo;
    listenInfo.handle = serverSocketHandle_;
    listenInfo.port = deviceAddress_.sin_port;
    listenInfo.phase = 0;
    listenInfo.err = 0;
    auto ret = DlHccpApi::RaSocketListenStop(&listenInfo, 1);
    if (ret != 0) {
        BM_LOG_WARN("stop socket listen failed: " << ret);
    }

    DlHccpApi::RaSocketDeinit(serverSocketHandle_);
    serverSocketHandle_ = nullptr;
    BM_LOG_INFO("UnPrepareDataConn finished.");
}

Result RdmaConnectionManager::ClientConnectServers()
{
    if (rankId_ == 0U) {
        BM_LOG_INFO("rank 0 need not connect to others.");
        return BM_OK;
    }

    std::vector<HccpSocketConnectInfo> connectInfos;
    for (auto it = currentRanksInfo_.begin(); it != currentRanksInfo_.end(); ++it) {
        if (it->first >= rankId_ || addedRanks_.find(it->first) == addedRanks_.end()) {
            continue;
        }
        auto socketHandle = CreateLocalSocket();
        if (socketHandle == nullptr) {
            BM_LOG_ERROR("create local socket handle failed");
            CloseClientConnections();
            return BM_DL_FUNCTION_FAILED;
        }
        std::string server{inet_ntoa(it->second.network.sin_addr)};
        clientConnections_.emplace(server, ConnectionChannel{it->second.network.sin_addr, socketHandle});

        HccpSocketConnectInfo connectInfo;
        connectInfo.handle = socketHandle;
        connectInfo.remoteIp.addr = it->second.network.sin_addr;
        connectInfo.port = it->second.network.sin_port;
        bzero(connectInfo.tag, sizeof(connectInfo.tag));
        BM_LOG_DEBUG("add connecting server " << connectInfo);
        connectInfos.emplace_back(connectInfo);
    }

    auto ret = DlHccpApi::RaSocketBatchConnect(connectInfos.data(), connectInfos.size());
    if (ret != 0) {
        BM_LOG_ERROR("connect to all servers failed: " << ret << ", servers count = " << connectInfos.size());
        CloseClientConnections();
        return BM_DL_FUNCTION_FAILED;
    }

    ret = WaitConnectionsReady(clientConnections_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("client wait connections failed: " << ret);
        CloseClientConnections();
        return ret;
    }

    ret = CreateQpWaitingReady(clientConnections_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("client create qp failed: " << ret);
        CloseClientConnections();
        return ret;
    }

    return BM_OK;
}

void RdmaConnectionManager::CloseClientConnections(bool closeAll)
{
    std::vector<HccpSocketCloseInfo> socketCloseInfos;
    for (auto it = clientConnections_.begin(); it != clientConnections_.end(); ++it) {
        if (!closeAll && addedNetworks_.find(it->first) == addedNetworks_.end()) {
            continue;
        }

        if (it->second.socketFd != nullptr) {
            HccpSocketCloseInfo info;
            info.handle = it->second.socketHandle;
            info.fd = it->second.socketFd;
            info.linger = 0;
            socketCloseInfos.push_back(info);
        }
    }

    auto ret = DlHccpApi::RaSocketBatchClose(socketCloseInfos.data(), socketCloseInfos.size());
    if (ret != 0) {
        BM_LOG_WARN("close sockets failed: " << ret);
    }

    for (auto it = clientConnections_.begin(); it != clientConnections_.end(); ++it) {
        if (!closeAll && addedNetworks_.find(it->first) == addedNetworks_.end()) {
            continue;
        }

        ret = DlHccpApi::RaSocketDeinit(it->second.socketHandle);
        if (ret != 0) {
            BM_LOG_WARN("deinit socket to server: " << it->first << " failed: " << ret);
        }
    }

    clientConnections_.clear();
}

Result RdmaConnectionManager::WaitServerSideConnected()
{
    if (rankId_ + 1U == rankCount_) {
        BM_LOG_INFO("rank(" << rankId_ << ") largest need not listen.");
        return BM_OK;
    }

    if (serverConnectThread_ == nullptr) {
        BM_LOG_ERROR("not start server listen.");
        return BM_ERROR;
    }

    serverConnectThread_->join();
    serverConnectThread_ = nullptr;
    return serverConnectResult;
}

Result RdmaConnectionManager::UpdateRanks(const std::unordered_map<uint32_t, ConnectRankInfo> &ranks)
{
    GenerateNewAdds(ranks);
    if (rankId_ + 1U < rankCount_) {
        auto ret = StartServerCommon();
        if (ret != BM_OK) {
            BM_LOG_ERROR("start server socket failed: " << ret);
            return ret;
        }
    }

    auto ret = ClientConnectServers();
    if (ret != BM_OK) {
        BM_LOG_ERROR("client connect to severs failed: " << ret);
        return ret;
    }

    ret = WaitServerSideConnected();
    if (ret != BM_OK) {
        BM_LOG_ERROR("wait client connect to severs failed: " << ret);
        return ret;
    }

    BM_LOG_INFO("update ranks success.");
    return BM_OK;
}

const AiQpRMAQueueInfo *RdmaConnectionManager::GetAiQpRMAQueueInfo() const
{
    return qpInfo_;
}

int RdmaConnectionManager::StartServerCommon()
{
    if (serverConnectThread_ != nullptr) {
        BM_LOG_ERROR("last time connection not ready!");
        return BM_ERROR;
    }

    auto ret = CreateServerSocket();
    if (ret != BM_OK) {
        BM_LOG_ERROR("create server socket failed: " << ret);
        return ret;
    }

    ret = GenerateWhiteList();
    if (ret != 0) {
        BM_LOG_ERROR("generate white list failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    serverConnectThread_ = std::make_shared<std::thread>([this]() {
        DlAclApi::AclrtSetDevice(deviceId_);
        auto ret = WaitConnectionsReady(serverConnections_);
        if (ret != BM_OK) {
            BM_LOG_ERROR("wait connection ready failed: " << ret);
            serverConnectResult = ret;
            return;
        }
        ret = CreateQpWaitingReady(serverConnections_);
        if (ret != BM_OK) {
            BM_LOG_ERROR("wait connection qp ready failed: " << ret);
            serverConnectResult = ret;
        }

        serverConnectResult = BM_OK;
    });
    BM_LOG_INFO("start to listen on port: " << deviceAddress_.sin_port << " success.");
    return BM_OK;
}

void RdmaConnectionManager::GenerateNewAdds(const std::unordered_map<uint32_t, ConnectRankInfo> &ranks)
{
    addedRanks_.clear();
    addedNetworks_.clear();
    for (auto it = ranks.begin(); it != ranks.end(); ++it) {
        if (currentRanksInfo_.find(it->first) != currentRanksInfo_.end()) {
            continue;
        }

        auto &temp = it->second.network.sin_addr;
        std::string key{inet_ntoa(temp)};
        addedRanks_.emplace(it->first);
        addedNetworks_.emplace(key);
        currentRanksInfo_.emplace(it->first, it->second);
    }
}

int RdmaConnectionManager::GenerateWhiteList()
{
    std::vector<HccpSocketWhiteListInfo> whitelist;
    for (auto &rank : addedRanks_) {
        if (rank <= rankId_) {
            continue;
        }

        auto pos = currentRanksInfo_.find(rank);
        if (pos == currentRanksInfo_.end()) {
            return BM_ERROR;
        }

        HccpSocketWhiteListInfo info{};
        info.remoteIp.addr = pos->second.network.sin_addr;
        info.connLimit = 10U;
        bzero(info.tag, sizeof(info.tag));
        whitelist.emplace_back(info);
        auto key = inet_ntoa(info.remoteIp.addr);
        serverConnections_.emplace(key, ConnectionChannel{info.remoteIp.addr, serverSocketHandle_});
    }
    BM_LOG_DEBUG("generate whitelist added ranks count: " << addedRanks_.size()
                                                          << ", whitelist size = " << whitelist.size());
    if (whitelist.empty()) {
        return BM_OK;
    }

    auto ret = DlHccpApi::RaSocketWhiteListAdd(serverSocketHandle_, whitelist.data(), whitelist.size());
    if (ret != 0) {
        BM_LOG_ERROR("socket handle add white list failed: " << ret);
        for (auto &key : addedNetworks_) {
            serverConnections_.erase(key);
        }
        for (auto &rk : addedRanks_) {
            currentRanksInfo_.erase(rk);
        }
        return BM_DL_FUNCTION_FAILED;
    }

    return BM_OK;
}

void *RdmaConnectionManager::CreateLocalSocket()
{
    void *socketHandle = nullptr;
    HccpRdev rdev;
    rdev.phyId = deviceId_;
    rdev.family = AF_INET;
    rdev.localIp.addr = deviceAddress_.sin_addr;
    auto ret = DlHccpApi::RaSocketInit(HccpNetworkMode::NETWORK_OFFLINE, rdev, socketHandle);
    if (ret != 0) {
        BM_LOG_ERROR("initialize socket handle failed: " << ret);
        return nullptr;
    }

    return socketHandle;
}

int RdmaConnectionManager::CreateServerSocket()
{
    if (serverSocketHandle_ != nullptr) {
        return BM_OK;
    }

    auto socketHandle = CreateLocalSocket();
    if (socketHandle == nullptr) {
        BM_LOG_ERROR("create local socket handle failed.");
        return BM_DL_FUNCTION_FAILED;
    }

    HccpSocketListenInfo listenInfo{};
    listenInfo.handle = socketHandle;
    listenInfo.port = deviceAddress_.sin_port;
    auto ret = DlHccpApi::RaSocketListenStart(&listenInfo, 1);
    if (ret != 0) {
        BM_LOG_ERROR("start to listen on port: " << listenInfo.port << " failed: " << ret);
        DlHccpApi::RaSocketDeinit(socketHandle);
        return BM_DL_FUNCTION_FAILED;
    }

    BM_LOG_INFO("start to listen on port: " << listenInfo.port << " success.");
    serverSocketHandle_ = socketHandle;
    return BM_OK;
}

int RdmaConnectionManager::WaitConnectionsReady(std::unordered_map<std::string, ConnectionChannel> &connections)
{
    uint32_t totalSuccessCount = 0;
    auto start = std::chrono::steady_clock::now();
    auto timeout = start + std::chrono::minutes(1);
    while (totalSuccessCount < connections.size()) {
        if (std::chrono::steady_clock::now() >= timeout) {
            BM_LOG_ERROR("waiting connection ready timeout.");
            return BM_ERROR;
        }

        uint32_t successCount = 0;
        std::vector<HccpSocketInfo> socketInfos;
        for (auto it = connections.begin(); it != connections.end(); ++it) {
            if (it->second.socketFd != nullptr) {
                continue;
            }

            HccpSocketInfo info{};
            info.handle = it->second.socketHandle;
            info.fd = nullptr;
            info.remoteIp.addr = it->second.remoteIp;
            info.status = 0;
            bzero(info.tag, sizeof(info.tag));
            socketInfos.push_back(info);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto role = (&connections == &clientConnections_) ? 1 : 0;
        auto ret = DlHccpApi::RaGetSockets(role, socketInfos.data(), socketInfos.size(), successCount);
        if (ret != 0) {
            BM_LOG_ERROR("role(" << role << ") side get sockets failed: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }

        for (auto i = 0U; i < successCount; i++) {
            std::string ip{inet_ntoa(socketInfos[i].remoteIp.addr)};
            auto pos = connections.find(ip);
            if (pos == connections.end()) {
                BM_LOG_ERROR("socket remote ip(" << ip << ") should not exist.");
                return BM_DL_FUNCTION_FAILED;
            }

            if (pos->second.socketFd != nullptr) {
                BM_LOG_ERROR("get socket connection remote ip(" << ip << ") already get socket fd.");
                return BM_DL_FUNCTION_FAILED;
            }

            if (pos->second.socketHandle != socketInfos[i].handle) {
                BM_LOG_ERROR("get socket connection remote ip(" << ip << ") socket handle not match.");
                return BM_DL_FUNCTION_FAILED;
            }

            pos->second.socketFd = socketInfos[i].fd;
            BM_LOG_INFO("connect to (" << ip << ") ready.");
        }

        totalSuccessCount += successCount;
    }

    return BM_OK;
}

int RdmaConnectionManager::CreateQpWaitingReady(std::unordered_map<std::string, ConnectionChannel> &connections)
{
    HccpQpExtAttrs attr{};
    attr.qpMode = static_cast<int>(HccpNetworkMode::NETWORK_OFFLINE);
    attr.version = 1;
    attr.cqAttr.sendCqDepth = 8192;
    attr.cqAttr.recvDqDepth = 128;
    attr.qp_attr.cap.max_recv_sge = 1;
    attr.qp_attr.cap.max_recv_wr = 128;
    attr.qp_attr.cap.max_recv_sge = 1;
    attr.qp_attr.qp_type = ibv_qp_type::IBV_QPT_RC;
    attr.qp_attr.cap.max_send_wr = 8192;
    attr.data_plane_flag.bs.cq_cstm = 1;

    for (auto it = connections.begin(); it != connections.end(); ++it) {
        if (addedNetworks_.find(it->first) == addedNetworks_.end()) {
            continue;
        }

        auto ret = DlHccpApi::RaQpAiCreate(rdmaHandle_, attr, it->second.aiQpInfo, it->second.qpHandle);
        if (ret != 0) {
            BM_LOG_ERROR("create AI QP to " << it->first << " failed: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }

        BM_LOG_DEBUG("create one qp=" << it->second.aiQpInfo);
        ret = DlHccpApi::RaQpConnectAsync(it->second.qpHandle, it->second.socketFd);
        if (ret != 0) {
            BM_LOG_ERROR("connect AI QP to " << it->first << " failed: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }
    }

    auto start = std::chrono::steady_clock::now();
    auto timeout = start + std::chrono::minutes(1);
    while (std::chrono::steady_clock::now() < timeout) {
        int connectingCount = 0;
        for (auto it = connections.begin(); it != connections.end(); ++it) {
            int status = 0;
            auto ret = DlHccpApi::RaGetQpStatus(it->second.qpHandle, status);
            if (ret != 0) {
                BM_LOG_ERROR("get AI QP status to " << it->first << " failed: " << ret);
                return BM_DL_FUNCTION_FAILED;
            }
            if (status != 1) {
                connectingCount++;
            }
        }
        if (connectingCount == 0) {
            return FillQpInfo();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return BM_TIMEOUT;
}

bool RdmaConnectionManager::ReserveQpInfoSpace()
{
    if (qpInfo_ != nullptr) {
        return true;
    }

    void *ptr = nullptr;
    auto oneQpSize = 2U * (sizeof(AiQpRMAWQ) + sizeof(AiQpRMACQ)) + sizeof(RdmaMemRegionInfo);
    qpInfoSize_ = sizeof(AiQpRMAQueueInfo) + oneQpSize * rankCount_;
    auto ret = DlAclApi::AclrtMalloc(&ptr, qpInfoSize_, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate device size: " << qpInfoSize_ << ", failed: " << ret);
        return false;
    }

    qpInfo_ = (AiQpRMAQueueInfo *)ptr;
    return true;
}

void RdmaConnectionManager::ReleaseQpInfoSpace()
{
    if (qpInfo_ != nullptr) {
        void* qpInfo = (void*)qpInfo_;
        auto ret = DlAclApi::AclrtFree(qpInfo);
        BM_LOG_INFO("free qpInfo pointer return : " << ret);
        qpInfo_ = nullptr;
    }
}

int RdmaConnectionManager::FillQpInfo()
{
    std::vector<uint8_t> qpInfoBuffer(qpInfoSize_);
    auto copyInfo = (AiQpRMAQueueInfo *)(void *)qpInfoBuffer.data();
    copyInfo->count = 1;
    copyInfo->sq = (AiQpRMAWQ *)(void *)(copyInfo + 1);
    copyInfo->rq = (AiQpRMAWQ *)(void *)(copyInfo->sq + rankCount_);
    copyInfo->scq = (AiQpRMACQ *)(void *)(copyInfo->rq + rankCount_);
    copyInfo->rcq = (AiQpRMACQ *)(void *)(copyInfo->scq + rankCount_);
    copyInfo->mr = (RdmaMemRegionInfo *)(void *)(copyInfo->rcq + rankCount_);
    for (auto it = currentRanksInfo_.begin(); it != currentRanksInfo_.end(); ++it) {
        if (addedRanks_.find(it->first) == addedRanks_.end()) {
            continue;
        }

        RegMemKeyUnion keyUnion{};
        keyUnion.commonKey = it->second.memories[0];
        copyInfo->mr[it->first].size = keyUnion.deviceKey.size;
        copyInfo->mr[it->first].addr = keyUnion.deviceKey.address;
        copyInfo->mr[it->first].lkey = keyUnion.deviceKey.lkey;
        copyInfo->mr[it->first].rkey = keyUnion.deviceKey.rkey;
        if (it->first == rankId_) {
            continue;
        }

        std::unordered_map<std::string, ConnectionChannel> *connections;
        std::string key{inet_ntoa(it->second.network.sin_addr)};
        if (it->first < rankId_) {
            connections = &clientConnections_;
        } else {
            connections = &serverConnections_;
        }

        auto pos = connections->find(key);
        if (pos == connections->end()) {
            BM_LOG_ERROR("missing for remote: " << key);
            return BM_ERROR;
        }

        CopyAiWQInfo(copyInfo->sq[it->first], pos->second.aiQpInfo.data_plane_info.sq, DBMode::HW_DB, 4);
        CopyAiWQInfo(copyInfo->rq[it->first], pos->second.aiQpInfo.data_plane_info.rq, DBMode::SW_DB, 4);
        CopyAiCQInfo(copyInfo->scq[it->first], pos->second.aiQpInfo.data_plane_info.scq, DBMode::HW_DB);
        CopyAiCQInfo(copyInfo->rcq[it->first], pos->second.aiQpInfo.data_plane_info.rcq, DBMode::SW_DB);
    }

    BM_LOG_DEBUG("transport qp info : " << AiQpInfoToString(*copyInfo, rankCount_));
    auto pointer = (ptrdiff_t)(void *)(qpInfo_);
    pointer += sizeof(AiQpRMAQueueInfo);
    copyInfo->sq = (AiQpRMAWQ *)(void *)(pointer);

    pointer += sizeof(AiQpRMAWQ) * rankCount_;
    copyInfo->rq = (AiQpRMAWQ *)(void *)(pointer);

    pointer += sizeof(AiQpRMAWQ) * rankCount_;
    copyInfo->scq = (AiQpRMACQ *)(void *)(pointer);

    pointer += sizeof(AiQpRMACQ) * rankCount_;
    copyInfo->rcq = (AiQpRMACQ *)(void *)(pointer);

    pointer += sizeof(AiQpRMACQ) * rankCount_;
    copyInfo->mr = (RdmaMemRegionInfo *)(void *)pointer;

    auto ret = DlAclApi::AclrtMemcpy(qpInfo_, qpInfoSize_, copyInfo, qpInfoSize_, ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != 0) {
        BM_LOG_ERROR("copy qp info to device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    BM_LOG_INFO("copy qp info success");

    return BM_OK;
}

void RdmaConnectionManager::CopyAiWQInfo(struct AiQpRMAWQ &dest, const struct ai_data_plane_wq &source, DBMode dbMode,
                                         uint32_t sl)
{
    dest.wqn = source.wqn;
    dest.bufAddr = source.buf_addr;
    dest.wqeSize = source.wqebb_size;
    dest.depth = source.depth;
    dest.headAddr = source.head_addr;
    dest.tailAddr = source.tail_addr;
    dest.dbMode = dbMode;
    if (dbMode == DBMode::SW_DB) {
        dest.dbAddr = source.swdb_addr;
    } else if (dbMode == DBMode::HW_DB) {
        dest.dbAddr = source.db_reg;
    }
    dest.sl = sl;
}

void RdmaConnectionManager::CopyAiCQInfo(struct AiQpRMACQ &dest, const ai_data_plane_cq &source, DBMode dbMode)
{
    dest.cqn = source.cqn;
    dest.bufAddr = source.buf_addr;
    dest.cqeSize = source.cqe_size;
    dest.depth = source.depth;
    dest.headAddr = source.head_addr;
    dest.tailAddr = source.tail_addr;
    dest.dbMode = dbMode;
    if (dbMode == DBMode::SW_DB) {
        dest.dbAddr = source.swdb_addr;
    } else if (dbMode == DBMode::HW_DB) {
        dest.dbAddr = source.db_reg;
    }
}
}
}
}
}