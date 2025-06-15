/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <vector>
#include <thread>

#include "hybm_logger.h"
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

    SetRunningState(RDMA_INIT);
    return std::make_shared<TransHandle>(rdmaHandle_);
}

void RdmaTransportManager::CloseDevice(const TransHandlePtr &h) {}

uint64_t RdmaTransportManager::GetTransportId() const
{
    return static_cast<uint64_t>(deviceIp_.s_addr);
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

Result RdmaTransportManager::PrepareDataConn(const TransPrepareOptions &options)
{
    std::vector<HccpSocketWhiteListInfo> allWhitelist;
    SetRunningState(RDMA_SOCKET_LISTENING);
    for (auto &item : options.whitelist) {
        in_addr temp{};
        temp.s_addr = static_cast<uint32_t>(item);

        HccpSocketWhiteListInfo info{};
        info.remoteIp.addr = temp;
        info.connLimit = 10U;
        memset(info.tag, 0, sizeof(info.tag));
        allWhitelist.emplace_back(info);
        serverConnections_.emplace(inet_ntoa(temp), ChannelConnection{temp});
    }

    void *socketHandle;
    HccpRdev rdev;
    rdev.phyId = deviceId_;
    rdev.family = AF_INET;
    rdev.localIp.addr = deviceIp_;
    auto ret = DlHccpApi::RaSocketInit(HccpNetworkMode::NETWORK_OFFLINE, rdev, socketHandle);
    if (ret != 0) {
        BM_LOG_ERROR("initialize socket handle failed: " << ret);
        SetRunningState(RDMA_EXITING);
        return BM_DL_FUNCTION_FAILED;
    }

    HccpSocketListenInfo listenInfo;
    listenInfo.handle = socketHandle;
    listenInfo.port = listenPort_;
    listenInfo.phase = 0;
    listenInfo.err = 0;
    ret = DlHccpApi::RaSocketListenStart(&listenInfo, 1);
    if (ret != 0) {
        BM_LOG_WARN("start to listen on port: " << listenPort_ << " failed: " << ret);
        SetRunningState(RDMA_EXITING);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = DlHccpApi::RaSocketWhiteListAdd(socketHandle, allWhitelist.data(), allWhitelist.size());
    if (ret != 0) {
        BM_LOG_ERROR("socket handle add white list failed: " << ret);
        DlHccpApi::RaSocketDeinit(socketHandle);
        SetRunningState(RDMA_EXITING);
        return BM_DL_FUNCTION_FAILED;
    }

    serverSocketHandle_ = socketHandle;
    SetRunningState(RDMA_SOCKET_ACCEPTING);
    BM_LOG_INFO("start to listen on port: " << listenPort_ << " success.");

    std::thread waitingTask{[this]() {
        auto ret = WaitConnectionsReady(serverConnections_);
        if (ret != BM_OK) {
            SetRunningState(RDMA_EXITING);
            return;
        }

        SetRunningState(RDMA_SOCKET_CONNECTED);
        ret = CreateQpWaitingReady(serverConnections_);
        if (ret != BM_OK) {
            SetRunningState(RDMA_EXITING);
        } else {
            SetRunningState(RDMA_READY);
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
    for (auto &remote : options.servers) {
        in_addr temp{};
        auto cnt = inet_aton(remote.c_str(), &temp);
        if (cnt != 1) {
            BM_LOG_ERROR("server ip(" << remote << ") invalid.");
            return BM_INVALID_PARAM;
        }

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
            SetRunningState(RDMA_EXITING);
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
        SetRunningState(RDMA_EXITING);
        return BM_DL_FUNCTION_FAILED;
    }

    SetRunningState(RDMA_SOCKET_CONNECTING);
    ret = WaitConnectionsReady(clientConnections_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("client wait connections failed: " << ret);
        CloseAllDataConn();
        SetRunningState(RDMA_EXITING);
        return ret;
    }

    SetRunningState(RDMA_SOCKET_CONNECTING);
    ret = CreateQpWaitingReady(clientConnections_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("client create qp failed: " << ret);
        CloseAllDataConn();
        SetRunningState(RDMA_EXITING);
        return ret;
    }

    SetRunningState(RDMA_READY);
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
    return runningState_ == RDMA_READY;
}

Result RdmaTransportManager::WaitingReady(int64_t timeoutNs)
{
    std::unique_lock<std::mutex> lockGuard{stateMutex_};
    if (runningState_ == RDMA_READY) {
        return BM_OK;
    }

    if (runningState_ > RDMA_READY) {
        return BM_ERROR;
    }

    auto cvState = stateCond_.wait_for(lockGuard, std::chrono::nanoseconds(timeoutNs));
    if (cvState == std::cv_status::timeout) {
        return BM_TIMEOUT;
    }

    if (runningState_ == RDMA_READY) {
        return BM_OK;
    }

    return BM_ERROR;
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

void RdmaTransportManager::SetRunningState(ock::mf::RdmaManagerState state)
{
    BM_LOG_INFO("RunningState set to: " << GetRunStateMessage(state));
    std::unique_lock<std::mutex> lockGuard{stateMutex_};
    runningState_ = state;
    if (runningState_ >= RDMA_READY) {
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
            socketInfos.push_back(info);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto ret = DlHccpApi::RaGetSockets(1, socketInfos.data(), socketInfos.size(), successCount);
        if (ret != 0) {
            BM_LOG_ERROR("client side get sockets failed: " << ret);
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
            return BM_OK;
        }
    }
    return BM_TIMEOUT;
}
}
}