/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <chrono>
#include "hybm_logger.h"
#include "dl_acl_api.h"
#include "dl_hccp_api.h"
#include "fixed_ranks_qp_manager.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
FixedRanksQpManager::FixedRanksQpManager(uint32_t deviceId, uint32_t rankId, uint32_t rankCount,
                                         sockaddr_in devNet) noexcept
    : DeviceQpManager(deviceId, rankId, rankCount, devNet, HYBM_ROLE_PEER)
{
}

FixedRanksQpManager::~FixedRanksQpManager() noexcept
{
    CloseServices();
}

int FixedRanksQpManager::SetRemoteRankInfo(const std::unordered_map<uint32_t, ConnectRankInfo> &ranks) noexcept
{
    if (started_.load()) {
        BM_LOG_ERROR("fixed ranks not support update ranks info after startup");
        return BM_ERROR;
    }

    currentRanksInfo_ = ranks;
    return BM_OK;
}

int FixedRanksQpManager::SetLocalMemories(const MemoryRegionMap &mrs) noexcept
{
    if (started_.load()) {
        BM_LOG_INFO("fixed ranks not support update register MRs after startup");
        return BM_OK;
    }

    currentLocalMrs_ = mrs;
    return BM_OK;
}

int FixedRanksQpManager::Startup(void *rdma) noexcept
{
    if (rdma == nullptr) {
        BM_LOG_ERROR("input rdma is null");
        return BM_INVALID_PARAM;
    }

    if (started_.load()) {
        BM_LOG_ERROR("already started.");
        return BM_ERROR;
    }

    rdmaHandle_ = rdma;
    if (!ReserveQpInfoSpace()) {
        BM_LOG_ERROR("reserve qp info space failed.");
        return BM_ERROR;
    }

    if (currentRanksInfo_.size() != rankCount_) {
        BM_LOG_ERROR("set rank count = " << currentRanksInfo_.size() << ", but rank_size = " << rankCount_);
        return BM_INVALID_PARAM;
    }

    for (auto it = currentRanksInfo_.begin(); it != currentRanksInfo_.end(); ++it) {
        if (it->first >= rankCount_) {
            BM_LOG_ERROR("input options of nics contains rankId:" << it->first << ", rank count: " << rankCount_);
            return BM_INVALID_PARAM;
        }
    }

    auto ret = StartServerSide();
    if (ret != BM_OK) {
        BM_LOG_ERROR("start server side failed: " << ret);
        return ret;
    }

    ret = StartClientSide();
    if (ret != BM_OK) {
        BM_LOG_ERROR("start client side failed: " << ret);
        return ret;
    }

    started_.store(true);
    return BM_OK;
}

void FixedRanksQpManager::Shutdown() noexcept
{
    CloseServices();
}

int FixedRanksQpManager::WaitingConnectionReady() noexcept
{
    if (serverConnectThread_ != nullptr) {
        serverConnectThread_->join();
        serverConnectThread_ = nullptr;
    }

    if (clientConnectThread_ != nullptr) {
        clientConnectThread_->join();
        clientConnectThread_ = nullptr;
    }

    if (serverConnectResult == BM_OK && clientConnectResult == BM_OK) {
        BM_LOG_INFO("client & server connections ready.");
        return BM_OK;
    }

    BM_LOG_ERROR("background connection thread not started.");
    return BM_ERROR;
}

const void *FixedRanksQpManager::GetQpInfoAddress() const noexcept
{
    return qpInfo_;
}

void *FixedRanksQpManager::GetQpHandleWithRankId(uint32_t rankId) const noexcept
{
    auto connections = rankId < rankId_ ? &clientConnections_ : &serverConnections_;
    auto pos = connections->find(rankId);
    if (pos == connections->end()) {
        return nullptr;
    }

    return pos->second.qpHandles[CONN_QP_STARS];
}

bool FixedRanksQpManager::ReserveQpInfoSpace() noexcept
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

int FixedRanksQpManager::StartServerSide() noexcept
{
    if (rankId_ + 1U == rankCount_) {
        serverConnectResult = 0;
        return BM_OK;
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
        ret = CreateQpWaitingReady(serverConnections_, CONN_QP_AI_CORE);
        if (ret != BM_OK) {
            BM_LOG_ERROR("wait connection AI qp ready failed: " << ret);
            serverConnectResult = ret;
        }

        ret = CreateQpWaitingReady(serverConnections_, CONN_QP_STARS);
        if (ret != BM_OK) {
            BM_LOG_ERROR("wait connection STARS qp ready failed: " << ret);
            serverConnectResult = ret;
        }

        serverConnectResult = BM_OK;
    });

    return BM_OK;
}

int FixedRanksQpManager::StartClientSide() noexcept
{
    if (rankId_ == 0U) {
        BM_LOG_INFO("rankId: " << rankId_ << " need not connect to others.");
        clientConnectResult = BM_OK;
        return BM_OK;
    }

    std::vector<HccpSocketConnectInfo> connectInfos;
    for (auto it = currentRanksInfo_.begin(); it != currentRanksInfo_.end(); ++it) {
        if (it->first >= rankId_) {
            continue;  // client connect to small ranks.
        }

        auto socketHandle = CreateLocalSocket();
        if (socketHandle == nullptr) {
            BM_LOG_ERROR("create local socket handle failed");
            CloseClientConnections();
            return BM_DL_FUNCTION_FAILED;
        }

        clientConnections_.emplace(it->first, ConnectionChannel{it->second.network.sin_addr, socketHandle});
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

    clientConnectThread_ = std::make_shared<std::thread>([this]() {
        DlAclApi::AclrtSetDevice(deviceId_);
        auto ret = WaitConnectionsReady(clientConnections_);
        if (ret != BM_OK) {
            BM_LOG_ERROR("client wait connections failed: " << ret);
            CloseClientConnections();
            return ret;
        }

        ret = CreateQpWaitingReady(clientConnections_, CONN_QP_AI_CORE);
        if (ret != BM_OK) {
            BM_LOG_ERROR("client create qp for AI CORE failed: " << ret);
            CloseClientConnections();
            return ret;
        }

        ret = CreateQpWaitingReady(clientConnections_, CONN_QP_STARS);
        if (ret != BM_OK) {
            BM_LOG_ERROR("client create qp for STARS failed: " << ret);
            CloseClientConnections();
            return ret;
        }
        clientConnectResult = BM_OK;
        return 0;
    });
    return BM_OK;
}

int FixedRanksQpManager::GenerateWhiteList() noexcept
{
    std::vector<HccpSocketWhiteListInfo> whitelist;
    for (auto it = currentRanksInfo_.begin(); it != currentRanksInfo_.end(); ++it) {
        if (it->first <= rankId_) {
            continue;  // small id as server, large id as client
        }
        HccpSocketWhiteListInfo info{};
        info.remoteIp.addr = it->second.network.sin_addr;
        info.connLimit = rankCount_;
        bzero(info.tag, sizeof(info.tag));
        whitelist.emplace_back(info);
        serverConnections_.emplace(it->first, ConnectionChannel{info.remoteIp.addr, serverSocketHandle_});
    }

    if (whitelist.empty()) {
        return BM_OK;
    }

    auto ret = DlHccpApi::RaSocketWhiteListAdd(serverSocketHandle_, whitelist.data(), whitelist.size());
    if (ret != 0) {
        BM_LOG_ERROR("socket handle add white list failed: " << ret);
        return BM_ERROR;
    }

    return BM_OK;
}

int FixedRanksQpManager::WaitConnectionsReady(std::unordered_map<uint32_t, ConnectionChannel> &connections) noexcept
{
    uint32_t totalSuccessCount = 0;
    auto start = std::chrono::steady_clock::now();
    auto timeout = start + std::chrono::minutes(2);
    while (totalSuccessCount < connections.size()) {
        if (std::chrono::steady_clock::now() >= timeout) {
            BM_LOG_ERROR("waiting connection ready timeout.");
            return BM_ERROR;
        }

        uint32_t successCount = 0;
        std::vector<HccpSocketInfo> socketInfos;
        std::unordered_map<in_addr_t, uint32_t> addr2index;
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
            addr2index.emplace(it->second.remoteIp.s_addr, it->first);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto role = (&connections == &clientConnections_) ? 1 : 0;
        auto ret = DlHccpApi::RaGetSockets(role, socketInfos.data(), socketInfos.size(), successCount);
        if (ret != 0) {
            BM_LOG_ERROR("role(" << role << ") side get sockets failed: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }

        for (auto i = 0U; i < successCount; i++) {
            auto socketInfoPos = addr2index.find(socketInfos[i].remoteIp.addr.s_addr);
            if (socketInfoPos == addr2index.end()) {
                BM_LOG_ERROR("socket ip(" << inet_ntoa(socketInfos[i].remoteIp.addr) << ") should not exist.");
                return BM_DL_FUNCTION_FAILED;
            }

            auto rankId = socketInfoPos->second;
            auto pos = connections.find(rankId);
            if (pos == connections.end()) {
                BM_LOG_ERROR("socket ip(" << inet_ntoa(socketInfos[i].remoteIp.addr) << ") should not exist.");
                return BM_DL_FUNCTION_FAILED;
            }

            if (pos->second.socketFd != nullptr) {
                BM_LOG_ERROR("get socket ip(" << inet_ntoa(socketInfos[i].remoteIp.addr) << ") already get socket fd.");
                return BM_DL_FUNCTION_FAILED;
            }

            if (pos->second.socketHandle != socketInfos[i].handle) {
                BM_LOG_ERROR("get socket ip(" << inet_ntoa(socketInfos[i].remoteIp.addr)
                                              << ") socket handle not match.");
                return BM_DL_FUNCTION_FAILED;
            }

            pos->second.socketFd = socketInfos[i].fd;
            BM_LOG_INFO("connect to (" << rankId << ") ready.");
        }

        totalSuccessCount += successCount;
    }

    return BM_OK;
}

int FixedRanksQpManager::CreateQpWaitingReady(std::unordered_map<uint32_t, ConnectionChannel> &connections,
                                              ConnQpType qpType) noexcept
{
    for (auto it = connections.begin(); it != connections.end(); ++it) {
        auto ret = CreateOneQp(qpType, it->second);
        if (ret != 0) {
            BM_LOG_ERROR("create QP type:" << qpType << " to " << it->first << " failed: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }

        for (auto pos = currentLocalMrs_.begin(); pos != currentLocalMrs_.end(); ++pos) {
            HccpMrInfo info{};
            info.addr = (void *)(ptrdiff_t)pos->second.address;
            info.size = pos->second.size;
            info.access = 7;
            ret = DlHccpApi::RaMrReg(it->second.qpHandles[qpType], info);
            if (ret != 0) {
                BM_LOG_ERROR("register MR failed: " << ret);
                return BM_DL_FUNCTION_FAILED;
            }
        }

        ret = DlHccpApi::RaQpConnectAsync(it->second.qpHandles[qpType], it->second.socketFd);
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
            auto ret = DlHccpApi::RaGetQpStatus(it->second.qpHandles[qpType], status);
            if (ret != 0) {
                BM_LOG_ERROR("get AI QP status to " << it->first << " failed: " << ret);
                return BM_DL_FUNCTION_FAILED;
            }
            if (status != 1) {
                connectingCount++;
            }
        }
        if (connectingCount == 0) {
            return FillQpInfo(qpType);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return BM_TIMEOUT;
}

int FixedRanksQpManager::CreateOneQp(ConnQpType qpType, ConnectionChannel &channel) noexcept
{
    int ret;
    if (qpType == CONN_QP_AI_CORE) {
        HccpQpExtAttrs attr{};
        attr.qpMode = NETWORK_OFFLINE;
        attr.version = 1;
        attr.cqAttr.sendCqDepth = 8192;
        attr.cqAttr.recvDqDepth = 128;
        attr.qp_attr.cap.max_recv_sge = 1;
        attr.qp_attr.cap.max_recv_wr = 128;
        attr.qp_attr.cap.max_recv_sge = 1;
        attr.qp_attr.qp_type = IBV_QPT_RC;
        attr.qp_attr.cap.max_send_wr = 8192;
        attr.data_plane_flag.bs.cq_cstm = 1;
        ret = DlHccpApi::RaQpAiCreate(rdmaHandle_, attr, channel.aiQpInfo, channel.qpHandles[qpType]);
    } else {
        ret = DlHccpApi::RaQpCreate(rdmaHandle_, 0, 2, channel.qpHandles[qpType]);
    }
    return ret;
}

int FixedRanksQpManager::FillQpInfo(ConnQpType qpType) noexcept
{
    if (qpType != CONN_QP_AI_CORE) {
        return BM_OK;
    }

    std::vector<uint8_t> qpInfoBuffer(qpInfoSize_);
    auto copyInfo = (AiQpRMAQueueInfo *)(void *)qpInfoBuffer.data();
    copyInfo->count = 1;
    copyInfo->sq = (AiQpRMAWQ *)(void *)(copyInfo + 1);
    copyInfo->rq = (AiQpRMAWQ *)(void *)(copyInfo->sq + rankCount_);
    copyInfo->scq = (AiQpRMACQ *)(void *)(copyInfo->rq + rankCount_);
    copyInfo->rcq = (AiQpRMACQ *)(void *)(copyInfo->scq + rankCount_);
    copyInfo->mr = (RdmaMemRegionInfo *)(void *)(copyInfo->rcq + rankCount_);
    for (auto it = currentRanksInfo_.begin(); it != currentRanksInfo_.end(); ++it) {
        auto &map = it->second.memoryMap;
        if (map.empty()) {
            continue;
        }
        copyInfo->mr[it->first].size = map.begin()->second.size;
        copyInfo->mr[it->first].addr = map.begin()->second.address;
        copyInfo->mr[it->first].lkey = map.begin()->second.lkey;
        copyInfo->mr[it->first].rkey = map.begin()->second.rkey;
        if (it->first == rankId_) {
            continue;
        }

        std::unordered_map<uint32_t, ConnectionChannel> *connections;
        if (it->first < rankId_) {
            connections = &clientConnections_;
        } else {
            connections = &serverConnections_;
        }

        auto pos = connections->find(it->first);
        if (pos == connections->end()) {
            BM_LOG_ERROR("missing for remote: " << it->first);
            return BM_ERROR;
        }

        CopyAiWQInfo(copyInfo->sq[it->first], pos->second.aiQpInfo.data_plane_info.sq, DBMode::HW_DB, 4);
        CopyAiWQInfo(copyInfo->rq[it->first], pos->second.aiQpInfo.data_plane_info.rq, DBMode::SW_DB, 4);
        CopyAiCQInfo(copyInfo->scq[it->first], pos->second.aiQpInfo.data_plane_info.scq, DBMode::HW_DB);
        CopyAiCQInfo(copyInfo->rcq[it->first], pos->second.aiQpInfo.data_plane_info.rcq, DBMode::SW_DB);
    }

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

void FixedRanksQpManager::CopyAiWQInfo(struct AiQpRMAWQ &dest, const struct ai_data_plane_wq &src, DBMode dbMode,
                                       uint32_t sl) noexcept
{
    dest.wqn = src.wqn;
    dest.bufAddr = src.buf_addr;
    dest.wqeSize = src.wqebb_size;
    dest.depth = src.depth;
    dest.headAddr = src.head_addr;
    dest.tailAddr = src.tail_addr;
    dest.dbMode = dbMode;
    if (dbMode == DBMode::SW_DB) {
        dest.dbAddr = src.swdb_addr;
    } else if (dbMode == DBMode::HW_DB) {
        dest.dbAddr = src.db_reg;
    }
    dest.sl = sl;
}

void FixedRanksQpManager::CopyAiCQInfo(struct AiQpRMACQ &dest, const ai_data_plane_cq &source, DBMode dbMode) noexcept
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

void FixedRanksQpManager::CloseServices() noexcept
{
    if (serverConnectThread_ != nullptr) {
        serverConnectThread_->join();
        serverConnectThread_ = nullptr;
    }

    if (clientConnectThread_ != nullptr) {
        clientConnectThread_->join();
        clientConnectThread_ = nullptr;
    }

    CloseServerConnections();
    CloseClientConnections();
}

void FixedRanksQpManager::CloseClientConnections() noexcept
{
    CloseConnections(clientConnections_);
}

void FixedRanksQpManager::CloseServerConnections() noexcept
{
    DestroyServerSocket();
    CloseConnections(serverConnections_);
}

void FixedRanksQpManager::CloseConnections(std::unordered_map<uint32_t, ConnectionChannel> &connections) noexcept
{
    std::vector<HccpSocketCloseInfo> socketCloseInfos;
    for (auto it = connections.begin(); it != connections.end(); ++it) {
        if (it->second.qpHandles[CONN_QP_AI_CORE] != nullptr) {
            auto ret = DlHccpApi::RaQpDestroy(it->second.qpHandles[CONN_QP_AI_CORE]);
            if (ret != 0) {
                BM_LOG_WARN("destroy AI QP to server: " << it->first << " failed: " << ret);
            }
            it->second.qpHandles[CONN_QP_AI_CORE] = nullptr;
        }

        if (it->second.qpHandles[CONN_QP_STARS] != nullptr) {
            auto ret = DlHccpApi::RaQpDestroy(it->second.qpHandles[CONN_QP_STARS]);
            if (ret != 0) {
                BM_LOG_WARN("destroy stars QP to server: " << it->first << " failed: " << ret);
            }
            it->second.qpHandles[CONN_QP_STARS] = nullptr;
        }

        if (it->second.socketFd != nullptr) {
            HccpSocketCloseInfo info;
            info.handle = it->second.socketHandle;
            info.fd = it->second.socketFd;
            info.linger = 0;
            socketCloseInfos.push_back(info);
            it->second.socketFd = nullptr;
        }
    }

    if (!socketCloseInfos.empty()) {
        auto ret = DlHccpApi::RaSocketBatchClose(socketCloseInfos.data(), socketCloseInfos.size());
        if (ret != 0) {
            BM_LOG_INFO("close sockets return: " << ret);
        }
    }

    for (auto it = connections.begin(); it != connections.end(); ++it) {
        auto ret = DlHccpApi::RaSocketDeinit(it->second.socketHandle);
        if (ret != 0) {
            BM_LOG_INFO("deinit socket to server: " << it->first << " return: " << ret);
        }
    }

    connections.clear();
}
}
}
}
}