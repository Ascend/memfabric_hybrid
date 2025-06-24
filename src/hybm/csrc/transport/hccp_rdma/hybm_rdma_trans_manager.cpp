/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <vector>
#include <thread>

#include "hybm_logger.h"
#include "dl_acl_api.h"
#include "dl_hccp_api.h"
#include "hybm_rdma_trans_manager.h"

namespace ock {
namespace mf {
namespace {
const std::unordered_map<RdmaManagerState, std::string> runStateNames = {
    {RDMA_IDLE, "RDMA_IDLE"},
    {RDMA_INIT, "RDMA_INIT"},
    {RDMA_SOCKET_CONNECTING, "RDMA_SOCKET_CONNECTING"},
    {RDMA_SOCKET_LISTENING, "RDMA_SOCKET_LISTENING"},
    {RDMA_SOCKET_ACCEPTING, "RDMA_SOCKET_ACCEPTING"},
    {RDMA_SOCKET_CONNECTED, "RDMA_SOCKET_CONNECTED"},
    {RDMA_QP_CONNECTING, "RDMA_QP_CONNECTING"},
    {RDMA_READY, "RDMA_READY"},
    {RDMA_EXITING, "RDMA_EXITING"},
    {RDMA_STATE_BUTT, "RDMA_STATE_BUTT"}};

std::string GetRunStateMessage(RdmaManagerState state)
{
    auto pos = runStateNames.find(state);
    if (pos == runStateNames.end()) {
        return std::string("UnknownRdmaManagerState(").append(std::to_string(state)).append(")");
    }

    return pos->second;
}

std::string AiQpRMAWQ2String(const AiQpRMAWQ &info)
{
    std::stringstream ss;
    ss << "wqn=" << info.wqn << ", buff_addr=" << (void *)(ptrdiff_t)info.bufAddr << ", wqe_size=" << info.wqeSize
       << ", depth=" << info.depth << ", head=" << (void *)(ptrdiff_t)info.headAddr
       << ", tail=" << (void *)(ptrdiff_t)info.tailAddr << ", db_mode=" << static_cast<int>(info.dbMode)
       << ", db_addr=" << (void *)(ptrdiff_t)info.dbAddr << ", sl=" << info.sl;
    return ss.str();
}

std::string AiQpRMACQ2String(const AiQpRMACQ &info)
{
    std::stringstream ss;
    ss << "cqn=" << info.cqn << ", buff_addr=" << (void *)(ptrdiff_t)info.bufAddr << ", cqe_size=" << info.cqeSize
       << ", depth=" << info.depth << ", head=" << (void *)(ptrdiff_t)info.headAddr
       << ", tail=" << (void *)(ptrdiff_t)info.tailAddr << ", db_mode=" << static_cast<int>(info.dbMode)
       << ", db_addr=" << (void *)(ptrdiff_t)info.dbAddr;
    return ss.str();
}

std::string RdmaMemRegionInfo2String(const RdmaMemRegionInfo &info)
{
    std::stringstream ss;
    ss << "size=" << info.size << ", addr=" << (void *)(ptrdiff_t)(info.addr) << ", lkey=" << info.lkey
       << ", rkey=" << info.rkey;
    return ss.str();
}

std::string AiQpInfoToString(const AiQpRMAQueueInfo &info, uint32_t rankCount)
{
    std::stringstream ss;
    ss << "QiQpInfo(rankCount=" << rankCount << ", mq_count=" << info.count << ")={\n";
    for (auto i = 0U; i < rankCount; i++) {
        ss << "  rank" << i << "={\n";
        for (auto j = 0U; j < info.count; j++) {
            ss << "    qp" << j << "_info={\n";
            ss << "      sq=<" << AiQpRMAWQ2String(*(info.sq + i * info.count + j)) << ">\n";
            ss << "      rq=<" << AiQpRMAWQ2String(*(info.rq + i * info.count + j)) << ">\n";
            ss << "      scq=<" << AiQpRMACQ2String(*(info.scq + i * info.count + j)) << ">\n";
            ss << "      rcq=<" << AiQpRMACQ2String(*(info.rcq + i * info.count + j)) << ">\n";
            ss << "    }\n";
        }
        ss << "    MR-rank-" << i << "=<" << RdmaMemRegionInfo2String(*(info.mr + i)) << ">";
        ss << "  }\n";
    }
    ss << "}";
    return ss.str();
}

std::string ai_data_plane_wq_2string(const ai_data_plane_wq &info)
{
    std::stringstream ss;
    ss << "wqn=" << info.wqn << ", buff_addr=" << (void *)(ptrdiff_t)info.buf_addr << ", wqebb_size=" << info.wqebb_size
       << ", depth=" << info.depth << ", head=" << (void *)(ptrdiff_t)info.head_addr
       << ", tail=" << (void *)(ptrdiff_t)info.tail_addr << ", swdb_addr=" << (void *)(ptrdiff_t)info.swdb_addr
       << ", db_reg=" << info.db_reg;
    return ss.str();
}

std::string ai_data_plane_cq_2string(const ai_data_plane_cq &info)
{
    std::stringstream ss;
    ss << "cqn=" << info.cqn << ", buff_addr=" << (void *)(ptrdiff_t)info.buf_addr << ", cqe_size=" << info.cqe_size
       << ", depth=" << info.depth << ", head=" << (void *)(ptrdiff_t)info.head_addr
       << ", tail=" << (void *)(ptrdiff_t)info.tail_addr << ", swdb_addr=" << (void *)(ptrdiff_t)info.swdb_addr
       << ", db_reg=" << info.db_reg;
    return ss.str();
}

std::string HccpAiQpInfo2String(const HccpAiQpInfo &info)
{
    std::stringstream ss;
    ss << "addr=" << (void *)(ptrdiff_t)info.aiQpAddr << ", sq_index=" << info.sqIndex << ", db_index=" << info.dbIndex
       << ", ai_scq_addr=" << (void *)(ptrdiff_t)info.ai_scq_addr << ", ai_rcq_addr"
       << (void *)(ptrdiff_t)info.ai_rcq_addr
       << ", data_plane_info=data_plane_info(sq=" << ai_data_plane_wq_2string(info.data_plane_info.sq)
       << ", rq=" << ai_data_plane_wq_2string(info.data_plane_info.rq)
       << ", scq=" << ai_data_plane_cq_2string(info.data_plane_info.scq)
       << ", rcq=" << ai_data_plane_cq_2string(info.data_plane_info.rcq) << ")";
    return ss.str();
}
}

RdmaTransportManager::RdmaTransportManager(uint32_t deviceId, uint32_t port) : deviceId_{deviceId}, listenPort_{port} {}

TransHandlePtr RdmaTransportManager::OpenDevice(const TransDeviceOptions &options)
{
    BM_LOG_INFO("open device with options(type=" << options.transType << ")");
    if (!OpenTsd()) {
        return nullptr;
    }

    if (!RaInit()) {
        return nullptr;
    }

    if (!RetireDeviceIp()) {
        return nullptr;
    }

    if (!RaRdevInit()) {
        return nullptr;
    }

    void *ptr = nullptr;
    auto oneQpSize = 2U * (sizeof(AiQpRMAWQ) + sizeof(AiQpRMACQ)) + sizeof(RdmaMemRegionInfo);
    qpInfoSize_ = sizeof(AiQpRMAQueueInfo) + oneQpSize * options.rankCount;
    auto ret = DlAclApi::AclrtMalloc(&ptr, qpInfoSize_, 0);
    if (ret != 0) {
        BM_LOG_ERROR("allocate device size: " << qpInfoSize_ << ", failed: " << ret);
        return nullptr;
    }

    qpInfo_ = (AiQpRMAQueueInfo *)ptr;
    localRankId_ = options.rankId;
    totalRankCount_ = options.rankCount;
    SetClientState(RDMA_INIT);
    SetServerState(RDMA_INIT);
    return std::make_shared<TransHandle>(rdmaHandle_);
}

void RdmaTransportManager::CloseDevice(const TransHandlePtr &h) {}

uint64_t RdmaTransportManager::GetTransportId() const
{
    return static_cast<uint64_t>(deviceIp_.s_addr);
}

void RdmaTransportManager::SetTransportIds(const std::vector<uint64_t> transports)
{
    clusterTransports_ = transports;
}

Result RdmaTransportManager::RegMemToDevice(const TransHandlePtr &h, const TransMemRegInput &in, TransMemRegOutput &out)
{
    void *mrHandle = nullptr;
    HccpMrInfo info{};
    info.addr = in.addr;
    info.size = in.size;
    info.access = in.access;
    auto ret = DlHccpApi::RaRegisterMR(rdmaHandle_, &info, mrHandle);
    if (ret != 0) {
        BM_LOG_ERROR("register MR(address=" << in.addr << ", size=" << in.size << ", access=" << in.addr
                                            << ") failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    out.handle = mrHandle;
    out.lkey = info.lkey;
    out.rkey = info.rkey;
    BM_LOG_DEBUG("register MR(address=" << in.addr << ", size=" << in.size << ", lkey=" << info.lkey
                                        << ", rkey=" << info.rkey << ")");

    return BM_OK;
}

Result RdmaTransportManager::UnRegMemFromDevice(const ock::mf::TransMemRegOutput &out)
{
    auto ret = DlHccpApi::RaDeregisterMR(rdmaHandle_, out.handle);
    if (ret != 0) {
        BM_LOG_ERROR("Unregister MR failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    return BM_OK;
}

Result RdmaTransportManager::SetGlobalRegisterMrInfo(const std::vector<RdmaMemRegionInfo> &mrs)
{
    if (mrs.size() != totalRankCount_) {
        BM_LOG_ERROR("MR size :" << mrs.size() << " should be " << totalRankCount_);
        return BM_INVALID_PARAM;
    }

    for (auto i = 0U; i < mrs.size(); i++) {
        BM_LOG_DEBUG("MR(" << i << ") addr=" << (void *)(ptrdiff_t)(mrs[i].addr) << ", size=" << mrs[i].size
                           << ", lkey=" << mrs[i].lkey << ", rkey=" << mrs[i].rkey);
    }
    rdmaMemRegionInfos_ = mrs;
    return BM_OK;
}

Result RdmaTransportManager::PrepareDataConn(const TransPrepareOptions &options)
{
    if (totalRankCount_ != clusterTransports_.size()) {
        BM_LOG_ERROR("total rank size : " << totalRankCount_ << ", transports size(): " << clusterTransports_.size());
        return BM_ERROR;
    }

    if (localRankId_ + 1U == totalRankCount_) {
        SetServerState(RDMA_READY);
        return BM_OK;
    }

    void *socketHandle;
    HccpRdev rdev;
    rdev.phyId = deviceId_;
    rdev.family = AF_INET;
    rdev.localIp.addr = deviceIp_;
    auto ret = DlHccpApi::RaSocketInit(HccpNetworkMode::NETWORK_OFFLINE, rdev, socketHandle);
    if (ret != 0) {
        BM_LOG_ERROR("initialize socket handle failed: " << ret);
        SetServerState(RDMA_EXITING);
        return BM_DL_FUNCTION_FAILED;
    }

    std::vector<HccpSocketWhiteListInfo> allWhitelist;
    SetServerState(RDMA_SOCKET_LISTENING);
    for (auto i = localRankId_ + 1U; i < totalRankCount_; i++) {
        in_addr temp{};
        temp.s_addr = static_cast<uint32_t>(clusterTransports_[i]);

        HccpSocketWhiteListInfo info{};
        info.remoteIp.addr = temp;
        info.connLimit = 10U;
        memset(info.tag, 0, sizeof(info.tag));
        allWhitelist.emplace_back(info);
        serverConnections_.emplace(inet_ntoa(temp), ChannelConnection{temp, socketHandle});
    }

    HccpSocketListenInfo listenInfo;
    listenInfo.handle = socketHandle;
    listenInfo.port = listenPort_;
    listenInfo.phase = 0;
    listenInfo.err = 0;
    ret = DlHccpApi::RaSocketListenStart(&listenInfo, 1);
    if (ret != 0) {
        BM_LOG_WARN("start to listen on port: " << listenPort_ << " failed: " << ret);
        SetServerState(RDMA_EXITING);
        return BM_DL_FUNCTION_FAILED;
    }

    if (!allWhitelist.empty()) {
        ret = DlHccpApi::RaSocketWhiteListAdd(socketHandle, allWhitelist.data(), allWhitelist.size());
        if (ret != 0) {
            BM_LOG_ERROR("socket handle add white list failed: " << ret);
            DlHccpApi::RaSocketDeinit(socketHandle);
            SetServerState(RDMA_EXITING);
            return BM_DL_FUNCTION_FAILED;
        }
    }

    serverSocketHandle_ = socketHandle;
    SetServerState(RDMA_SOCKET_ACCEPTING);
    BM_LOG_INFO("start to listen on port: " << listenPort_ << " success.");

    std::thread waitingTask{[this]() {
        DlAclApi::AclrtSetDevice(deviceId_);
        auto ret = WaitConnectionsReady(serverConnections_);
        if (ret != BM_OK) {
            SetServerState(RDMA_EXITING);
            return;
        }

        SetServerState(RDMA_SOCKET_CONNECTED);
        ret = CreateQpWaitingReady(serverConnections_);
        if (ret != BM_OK) {
            SetServerState(RDMA_EXITING);
        } else {
            SetServerState(RDMA_READY);
        }
    }};

    waitingTask.detach();
    return BM_OK;
}

void RdmaTransportManager::UnPrepareDataConn()
{
    if (serverSocketHandle_ == nullptr) {
        BM_LOG_ERROR("Not Prepared.");
        return;
    }

    HccpSocketListenInfo listenInfo;
    listenInfo.handle = serverSocketHandle_;
    listenInfo.port = listenPort_;
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

Result RdmaTransportManager::CreateDataConn(const TransDataConnOptions &options)
{
    std::vector<in_addr> serverIps;
    if (localRankId_ == 0) {
        SetClientState(RDMA_READY);
        return BM_OK;
    }

    for (auto i = 0U; i < localRankId_; i++) {
        in_addr temp{};
        temp.s_addr = static_cast<uint32_t>(clusterTransports_[i]);
        serverIps.push_back(temp);
    }

    std::vector<HccpSocketConnectInfo> connectInfos;
    for (auto &ip : serverIps) {
        void *socketHandle;
        HccpRdev rdev;
        rdev.phyId = deviceId_;
        rdev.family = AF_INET;
        rdev.localIp.addr = deviceIp_;
        auto ret = DlHccpApi::RaSocketInit(HccpNetworkMode::NETWORK_OFFLINE, rdev, socketHandle);
        if (ret != 0) {
            BM_LOG_ERROR("initialize socket handle failed: " << ret);
            CloseAllDataConn();
            SetClientState(RDMA_EXITING);
            return BM_DL_FUNCTION_FAILED;
        }

        std::string server{inet_ntoa(ip)};
        clientConnections_.emplace(server, ChannelConnection{ip, socketHandle});

        HccpSocketConnectInfo connectInfo;
        connectInfo.handle = socketHandle;
        connectInfo.remoteIp.addr = ip;
        connectInfo.port = listenPort_;
        memset(connectInfo.tag, 0, sizeof(connectInfo.tag));
        connectInfos.emplace_back(connectInfo);
    }

    auto ret = DlHccpApi::RaSocketBatchConnect(connectInfos.data(), connectInfos.size());
    if (ret != 0) {
        BM_LOG_ERROR("connect to all servers failed: " << ret);
        CloseAllDataConn();
        SetClientState(RDMA_EXITING);
        return BM_DL_FUNCTION_FAILED;
    }

    SetClientState(RDMA_SOCKET_CONNECTING);
    ret = WaitConnectionsReady(clientConnections_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("client wait connections failed: " << ret);
        CloseAllDataConn();
        SetClientState(RDMA_EXITING);
        return ret;
    }

    SetClientState(RDMA_SOCKET_CONNECTING);
    ret = CreateQpWaitingReady(clientConnections_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("client create qp failed: " << ret);
        CloseAllDataConn();
        SetClientState(RDMA_EXITING);
        return ret;
    }

    SetClientState(RDMA_READY);
    return BM_OK;
}

void RdmaTransportManager::CloseAllDataConn()
{
    std::vector<HccpSocketCloseInfo> socketCloseInfos;
    for (auto it = clientConnections_.begin(); it != clientConnections_.end(); ++it) {
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
        ret = DlHccpApi::RaSocketDeinit(it->second.socketHandle);
        if (ret != 0) {
            BM_LOG_WARN("deinit socket to server: " << it->first << " failed: " << ret);
        }
    }

    clientConnections_.clear();
}

bool RdmaTransportManager::IsReady()
{
    std::unique_lock<std::mutex> lockGuard{stateMutex_};
    return clientState_ == RDMA_READY && serverState_ == RDMA_READY;
}

Result RdmaTransportManager::WaitingReady(int64_t timeoutNs)
{
    std::unique_lock<std::mutex> lockGuard{stateMutex_};
    if (clientState_ == RDMA_READY && serverState_ == RDMA_READY) {
        return BM_OK;
    }

    if (clientState_ > RDMA_READY || serverState_ > RDMA_READY) {
        return BM_ERROR;
    }

    auto cvState = stateCond_.wait_for(lockGuard, std::chrono::nanoseconds(timeoutNs));
    if (cvState == std::cv_status::timeout) {
        return BM_TIMEOUT;
    }

    if (clientState_ == RDMA_READY && serverState_ == RDMA_READY) {
        return BM_OK;
    }

    return BM_ERROR;
}

std::vector<TransDataConnPtr> RdmaTransportManager::GetDataConn()
{
    return std::vector<TransDataConnPtr>{};
}

TransDataConnAddressInfo RdmaTransportManager::GetDataConnAddrInfo()
{
    return TransDataConnAddressInfo{qpInfo_};
}

bool RdmaTransportManager::OpenTsd()
{
    auto res = DlHccpApi::TsdOpen(deviceId_, 2U);
    if (res != 0) {
        BM_LOG_ERROR("TsdOpen failed: " << res);
        return false;
    }
    return true;
}

bool RdmaTransportManager::RaInit()
{
    HccpRaInitConfig initConfig{};
    initConfig.phyId = deviceId_;
    initConfig.nicPosition = NETWORK_OFFLINE;
    initConfig.hdcType = 6;
    BM_LOG_INFO("RaInit, config(phyId=" << initConfig.phyId << ", nicPosition=" << initConfig.nicPosition
                                        << ", hdcType=" << initConfig.hdcType << ")");
    auto ret = DlHccpApi::RaInit(initConfig);
    if (ret != 0) {
        BM_LOG_ERROR("Hccp Init RA failed: " << ret);
        return false;
    }
    return true;
}

bool RdmaTransportManager::RetireDeviceIp()
{
    uint32_t count = 0;
    std::vector<HccpInterfaceInfo> infos;

    HccpRaGetIfAttr config;
    config.phyId = deviceId_;
    config.nicPosition = HccpNetworkMode::NETWORK_OFFLINE;
    config.isAll = true;

    auto ret = DlHccpApi::RaGetIfNum(config, count);
    if (ret != 0 || count == 0) {
        BM_LOG_ERROR("get interface count failed: " << ret << ", count: " << count);
        return false;
    }

    infos.resize(count);
    ret = DlHccpApi::RaGetIfAddrs(config, infos.data(), count);
    if (ret != 0) {
        BM_LOG_ERROR("get interface information failed: " << ret);
        return false;
    }

    for (auto &info : infos) {
        if (info.family == AF_INET) {
            deviceIp_ = info.ifaddr.ip.addr;
            return true;
        }
    }

    return false;
}

bool RdmaTransportManager::RaRdevInit()
{
    HccpRdevInitInfo info{};
    HccpRdev rdev{};

    info.mode = NETWORK_OFFLINE;
    info.notifyType = NOTIFY;
    info.enabled2mbLite = true;
    rdev.phyId = deviceId_;
    rdev.family = AF_INET;
    rdev.localIp.addr = deviceIp_;
    BM_LOG_INFO("RaRdevInitV2, info(mode=" << info.mode << ", notify=" << info.notifyType << ", enabled910aLite="
                                           << info.enabled910aLite << ", disabledLiteThread=" << info.disabledLiteThread
                                           << ", enabled2mbLite=" << info.enabled2mbLite
                                           << "), rdev(phyId=" << rdev.phyId << ", family=" << rdev.family
                                           << ", rdev.ip=" << inet_ntoa(rdev.localIp.addr) << ")");
    auto ret = DlHccpApi::RaRdevInitV2(info, rdev, rdmaHandle_);
    if (ret != 0) {
        BM_LOG_ERROR("Hccp Init RDev failed: " << ret);
        return false;
    }

    BM_LOG_INFO("initialize RDev success, rdmaHandle: " << rdmaHandle_);
    return true;
}

void RdmaTransportManager::SetClientState(RdmaManagerState state)
{
    BM_LOG_INFO("clientState_ set to: " << GetRunStateMessage(state));
    std::unique_lock<std::mutex> lockGuard{stateMutex_};
    clientState_ = state;
    if (clientState_ > RDMA_READY) {
        stateCond_.notify_all();
        return;
    }

    if (clientState_ >= RDMA_READY && serverState_ >= RDMA_READY) {
        stateCond_.notify_all();
    }
}

void RdmaTransportManager::SetServerState(RdmaManagerState state)
{
    BM_LOG_INFO("serverState_ set to: " << GetRunStateMessage(state));
    std::unique_lock<std::mutex> lockGuard{stateMutex_};
    serverState_ = state;
    if (serverState_ > RDMA_READY) {
        stateCond_.notify_all();
        return;
    }

    if (clientState_ >= RDMA_READY && serverState_ >= RDMA_READY) {
        stateCond_.notify_all();
    }
}

int RdmaTransportManager::WaitConnectionsReady(std::unordered_map<std::string, ChannelConnection> &connections)
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
            memset(info.tag, 0, sizeof(info.tag));
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

int RdmaTransportManager::CreateQpWaitingReady(std::unordered_map<std::string, ChannelConnection> &connections)
{
    HccpQpExtAttrs attr{};
    attr.qpMode = NETWORK_OFFLINE;
    attr.version = 1;
    attr.cqAttr.sendCqDepth = 32768;
    attr.cqAttr.recvDqDepth = 128;
    attr.qp_attr.cap.max_recv_sge = 1;
    attr.qp_attr.cap.max_recv_wr = 128;
    attr.qp_attr.cap.max_recv_sge = 1;
    attr.qp_attr.qp_type = IBV_QPT_RC;
    attr.qp_attr.cap.max_send_wr = 128;

    for (auto it = connections.begin(); it != connections.end(); ++it) {
        auto ret = DlHccpApi::RaQpAiCreate(rdmaHandle_, attr, it->second.aiQpInfo, it->second.qpHandle);
        if (ret != 0) {
            BM_LOG_ERROR("create AI QP to " << it->first << " failed: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }

        BM_LOG_DEBUG("create one qp=" << HccpAiQpInfo2String(it->second.aiQpInfo));
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

int RdmaTransportManager::FillQpInfo()
{
    in_addr addr{};
    AiQpRMAQueueInfo *copyInfo = (AiQpRMAQueueInfo *)malloc(qpInfoSize_);
    if (copyInfo == nullptr) {
        BM_LOG_ERROR("allocate memory with size: " << qpInfoSize_ << " failed.");
        return BM_ERROR;
    }

    copyInfo->count = 1;
    copyInfo->sq = (AiQpRMAWQ *)(void *)(copyInfo + 1);
    copyInfo->rq = (AiQpRMAWQ *)(void *)(copyInfo->sq + clusterTransports_.size());
    copyInfo->scq = (AiQpRMACQ *)(void *)(copyInfo->rq + clusterTransports_.size());
    copyInfo->rcq = (AiQpRMACQ *)(void *)(copyInfo->scq + clusterTransports_.size());
    copyInfo->mr = (RdmaMemRegionInfo *)(void *)(copyInfo->rcq + clusterTransports_.size());
    for (auto i = 0U; i < totalRankCount_; i++) {
        copyInfo->mr[i] = rdmaMemRegionInfos_[i];
        if (i == localRankId_) {
            continue;
        }

        std::unordered_map<std::string, ChannelConnection> *connections;
        addr.s_addr = static_cast<uint32_t>(clusterTransports_[i]);
        std::string key{inet_ntoa(addr)};
        if (i < localRankId_) {
            connections = &clientConnections_;
        } else {
            connections = &serverConnections_;
        }

        auto pos = connections->find(key);
        if (pos == connections->end()) {
            BM_LOG_ERROR("missing for remote: " << key);
            free(copyInfo);
            return BM_ERROR;
        }

        CopyAiWQInfo(copyInfo->sq[i], pos->second.aiQpInfo.data_plane_info.sq, DBMode::HW_DB, 4);
        CopyAiWQInfo(copyInfo->rq[i], pos->second.aiQpInfo.data_plane_info.rq, DBMode::SW_DB, 4);
        CopyAiCQInfo(copyInfo->scq[i], pos->second.aiQpInfo.data_plane_info.scq, DBMode::SW_DB);
        CopyAiCQInfo(copyInfo->rcq[i], pos->second.aiQpInfo.data_plane_info.rcq, DBMode::SW_DB);
    }

    BM_LOG_DEBUG("transport qp info : " << AiQpInfoToString(*copyInfo, totalRankCount_));
    auto pointer = (ptrdiff_t)(void *)(qpInfo_);
    pointer += sizeof(AiQpRMAQueueInfo);
    copyInfo->sq = (AiQpRMAWQ *)(void *)(pointer);

    pointer += sizeof(AiQpRMAWQ) * clusterTransports_.size();
    copyInfo->rq = (AiQpRMAWQ *)(void *)(pointer);

    pointer += sizeof(AiQpRMAWQ) * clusterTransports_.size();
    copyInfo->scq = (AiQpRMACQ *)(void *)(pointer);

    pointer += sizeof(AiQpRMACQ) * clusterTransports_.size();
    copyInfo->rcq = (AiQpRMACQ *)(void *)(pointer);

    pointer += sizeof(AiQpRMACQ) * clusterTransports_.size();
    copyInfo->mr = (RdmaMemRegionInfo *)(void *)pointer;

    auto ret = DlAclApi::AclrtMemcpy(qpInfo_, qpInfoSize_, copyInfo, qpInfoSize_, ACL_MEMCPY_HOST_TO_DEVICE);
    free(copyInfo);
    if (ret != 0) {
        BM_LOG_ERROR("copy qp info to device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    BM_LOG_INFO("copy qp info success");

    return BM_OK;
}

void RdmaTransportManager::CopyAiWQInfo(struct AiQpRMAWQ &dest, const struct ai_data_plane_wq &source, DBMode dbMode,
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

void RdmaTransportManager::CopyAiCQInfo(struct AiQpRMACQ &dest, const ai_data_plane_cq &source, DBMode dbMode)
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