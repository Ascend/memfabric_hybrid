/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <thread>
#include <algorithm>
#include "hybm_logger.h"
#include "dl_acl_api.h"
#include "dl_hccp_api.h"
#include "bipartite_ranks_qp_manager.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
const int delay = 5;
static constexpr auto WAIT_DELAY_TIME = std::chrono::seconds(delay);
constexpr uint32_t QP_MAX_CONNECTIONS = 1024;
BipartiteRanksQpManager::BipartiteRanksQpManager(uint32_t deviceId, uint32_t rankId, uint32_t rankCount,
                                                 sockaddr_in devNet, bool server) noexcept
    : DeviceQpManager{deviceId, rankId, rankCount, devNet, server ? HYBM_ROLE_RECEIVER : HYBM_ROLE_SENDER}
{
    connectionView_.resize(rankCount);
    userQpInfo_.resize(rankCount);
}

BipartiteRanksQpManager::~BipartiteRanksQpManager() noexcept
{
    CloseServices();
}

int BipartiteRanksQpManager::SetRemoteRankInfo(const std::unordered_map<uint32_t, ConnectRankInfo> &ranks) noexcept
{
    std::unordered_map<uint32_t, ConnectRankInfo> tempRanks;
    for (auto it = ranks.begin(); it != ranks.end(); ++it) {
        if (it->second.role == rankRole_) {
            continue;
        }

        if (it->first >= rankCount_) {
            BM_LOG_ERROR("contains too large rankId: " << it->first);
            return BM_ERROR;
        }

        tempRanks.emplace(it->first, it->second);
    }

    std::unordered_map<uint32_t, sockaddr_in> addedRanks;
    std::unordered_set<uint32_t> addMrRanks;

    std::unique_lock<std::mutex> uniqueLock{mutex_};
    auto lastTimeRanksInfo = std::move(currentRanksInfo_);
    currentRanksInfo_ = std::move(tempRanks);
    if (backGroundThread_ == nullptr) {
        return BM_OK;
    }

    BM_LOG_INFO("SetRemoteRankInfo currentRanksInfo_.size=: " << currentRanksInfo_.size()
                << ", lastTimeRanksInfo.size=" << lastTimeRanksInfo.size());
    GenDiffInfoChangeRanks(lastTimeRanksInfo, addedRanks, addMrRanks);
    uniqueLock.unlock();

    GenTaskFromChangeRanks(addedRanks, addMrRanks);
    return BM_OK;
}

int BipartiteRanksQpManager::Startup(void *rdma) noexcept
{
    if (rdma == nullptr) {
        BM_LOG_ERROR("input rdma is null");
        return BM_INVALID_PARAM;
    }

    rdmaHandle_ = rdma;
    std::unique_lock<std::mutex> uniqueLock{mutex_};
    if (rankRole_ == HYBM_ROLE_RECEIVER) {
        auto ret = CreateServerSocket();
        if (ret != BM_OK) {
            BM_LOG_ERROR("create server socket failed: " << ret);
            return ret;
        }

        auto &task = connectionTasks_.whitelistTask;
        task.locker.lock();
        for (auto it = currentRanksInfo_.begin(); it != currentRanksInfo_.end(); ++it) {
            in_addr remoteIp = it->second.network.sin_addr;
            task.remoteIps.emplace(it->first, remoteIp);
        }
        task.status.failedTimes = 0;
        task.status.exist = true;
        task.locker.unlock();
    } else {
        auto &task = connectionTasks_.clientConnectTask;
        task.locker.lock();
        for (auto it = currentRanksInfo_.begin(); it != currentRanksInfo_.end(); ++it) {
            task.remoteAddress.emplace(it->first, it->second.network);
        }
        task.status.failedTimes = 0;
        task.status.exist = true;
        task.locker.unlock();
    }

    if (backGroundThread_ != nullptr) {
        BM_LOG_ERROR("already started");
        return BM_ERROR;
    }

    managerRunning_.store(true);
    backGroundThread_ = std::make_shared<std::thread>([this]() { BackgroundProcess(); });
    return BM_OK;
}

void BipartiteRanksQpManager::Shutdown() noexcept
{
    try {
        CloseServices();
    } catch (const std::exception &e) {
        BM_LOG_ERROR("dynamic ranks qp mgr close services failed: " << e.what());
    }
}

UserQpInfo *BipartiteRanksQpManager::GetQpHandleWithRankId(uint32_t rankId) noexcept
{
    ReadGuard lockGuard(qpLock_);
    if (rankId >= userQpInfo_.size()) {
        BM_LOG_ERROR("get qp handle with rankId: " << rankId << ", too large.");
        return nullptr;
    }

    if (userQpInfo_[rankId].qpHandle == nullptr) {
        BM_LOG_ERROR("get qp handle with rankId: " << rankId << ", no connection.");
        return nullptr;
    }

    return &userQpInfo_[rankId];
}

void BipartiteRanksQpManager::PutQpHandle(UserQpInfo *qp) const noexcept
{
    return;
}

void BipartiteRanksQpManager::BackgroundProcess() noexcept
{
    DlAclApi::AclrtSetDevice(deviceId_);
    while (managerRunning_.load()) {
        std::unique_lock<std::mutex> uniqueLock{mutex_};
        cond_.wait_for(uniqueLock, std::chrono::milliseconds(1));
        if (!managerRunning_.load()) {
            break;
        }
        const int retryTimes = 100;
        for (int i = 0; i < retryTimes; ++i) {
            auto count = ProcessServerAddWhitelistTask();
            count += ProcessClientConnectSocketTask();
            count += ProcessQueryConnectionStateTask();
            count += ProcessConnectQpTask();
            count += ProcessQueryQpStateTask();
            ProcessUpdateLocalMrTask();
            ProcessUpdateRemoteMrTask();
            if (count == 0) {
                break;
            }
        }
    }
}

void BipartiteRanksQpManager::InitializeWhiteList(std::vector<HccpSocketWhiteListInfo> &whitelist,
                                                  std::unordered_map<uint32_t, in_addr> remotes) noexcept
{
    const uint32_t MAX_CONNECTIONS = QP_MAX_CONNECTIONS;
    for (auto it = remotes.begin(); it != remotes.end(); ++it) {
        if (connections_.find(it->first) != connections_.end()) {
            continue;
        }

        HccpSocketWhiteListInfo info{};
        info.connLimit = MAX_CONNECTIONS;
        info.remoteIp.addr = it->second;
        bzero(info.tag, sizeof(info.tag));
        whitelist.emplace_back(info);
        auto res = connections_.emplace(it->first, ConnectionChannel{Ip2Net(info.remoteIp.addr), serverSocketHandle_});
        connectionView_[it->first] = &res.first->second;
        WriteGuard lockGuard(qpLock_);
        userQpInfo_[it->first].qpHandle = connectionView_[it->first]->qpHandle;
        BM_LOG_INFO("connections list add rank: " << it->first << ", remoteIP: " << inet_ntoa(info.remoteIp.addr));
    }
}

int BipartiteRanksQpManager::ProcessServerAddWhitelistTask() noexcept
{
    if (rankRole_ != HYBM_ROLE_RECEIVER) {
        return 0;
    }

    auto &currTask = connectionTasks_.whitelistTask;
    std::unique_lock<std::mutex> uniqueLock{currTask.locker};
    if (!currTask.status.exist) {
        return 0;
    }

    auto remotes = std::move(currTask.remoteIps);
    currTask.status.exist = false;
    uniqueLock.unlock();

    std::vector<HccpSocketWhiteListInfo> whitelist;
    InitializeWhiteList(whitelist, remotes);

    if (whitelist.empty()) {
        return 0;
    }

    auto ret = DlHccpApi::RaSocketWhiteListAdd(serverSocketHandle_, whitelist.data(), whitelist.size());
    if (ret != 0) {
        auto failedTimes = currTask.Failed(remotes);
        BM_LOG_ERROR("RaSocketWhiteListAdd() with size=" << whitelist.size() << " failed: " << ret
                                                         << ", times=" << failedTimes);
        return 1;
    }
    currTask.Success();
    auto &nextTask = connectionTasks_.queryConnectTask;
    for (auto &rank : remotes) {
        nextTask.ip2rank.emplace(rank.second.s_addr, rank.first);
    }
    nextTask.status.exist = true;
    nextTask.status.failedTimes = 0;
    return 0;
}

int BipartiteRanksQpManager::CreateConnectInfos(std::unordered_map<uint32_t, sockaddr_in> &remotes,
                                                std::vector<HccpSocketConnectInfo> &connectInfos,
                                                ClientConnectSocketTask &currTask)
{
    for (auto it = remotes.begin(); it != remotes.end(); ++it) {
        void *socketHandle;
        auto pos = connections_.find(it->first);
        if (pos == connections_.end()) {
            socketHandle = CreateLocalSocket();
            if (socketHandle == nullptr) {
                auto failedCount = currTask.Failed(remotes);
                BM_LOG_ERROR("create local socket handle failed times: " << failedCount);
                return 1;
            }
            pos = connections_.emplace(it->first, ConnectionChannel{it->second, socketHandle}).first;
            connectionView_[it->first] = &pos->second;
            WriteGuard lockGuard(qpLock_);
            userQpInfo_[it->first].qpHandle = connectionView_[it->first]->qpHandle;
        } else {
            socketHandle = pos->second.socketHandle;
        }

        if (pos->second.socketFd != nullptr) {
            continue;
        }

        HccpSocketConnectInfo connectInfo;
        connectInfo.handle = socketHandle;
        connectInfo.remoteIp.addr = it->second.sin_addr;
        connectInfo.port = it->second.sin_port;
        bzero(connectInfo.tag, sizeof(connectInfo.tag));
        BM_LOG_DEBUG("add connecting server " << connectInfo);
        connectInfos.emplace_back(connectInfo);
    }
    return BM_OK;
}

int BipartiteRanksQpManager::BatchConnectWithRetry(std::vector<HccpSocketConnectInfo> connectInfos,
                                                   ClientConnectSocketTask &currTask,
                                                   std::unordered_map<uint32_t, sockaddr_in> &remotes) noexcept
{
    uint32_t batchSize = 16;
    for (size_t i = 0; i < connectInfos.size(); i += batchSize) {
        size_t currentBatchSize = (connectInfos.size() - i) >= batchSize ? batchSize : (connectInfos.size() - i);
        auto batchStart = connectInfos.begin() + i;
        auto batchEnd = batchStart + currentBatchSize;
        std::vector<HccpSocketConnectInfo> currentBatch(batchStart, batchEnd);

        auto ret = DlHccpApi::RaSocketBatchConnect(currentBatch.data(), currentBatch.size());
        if (ret != 0) {
            auto failedCount = currTask.Failed(remotes);
            BM_LOG_ERROR("connect to all servers failed: " << ret << ", servers count = " << connectInfos.size()
                                                           << ", failed times: " << failedCount);
            return 1;
        }
    }
    return 0;
}

int BipartiteRanksQpManager::ProcessClientConnectSocketTask() noexcept
{
    if (rankRole_ != HYBM_ROLE_SENDER) {
        return 0;
    }

    auto &currTask = connectionTasks_.clientConnectTask;
    std::unique_lock<std::mutex> uniqueLock{currTask.locker};
    if (!currTask.status.exist) {
        return 0;
    }

    std::this_thread::sleep_for(WAIT_DELAY_TIME);
    auto remotes = std::move(currTask.remoteAddress);
    currTask.status.exist = false;
    uniqueLock.unlock();

    std::vector<HccpSocketConnectInfo> connectInfos;
    auto ret = CreateConnectInfos(remotes, connectInfos, currTask);
    if (ret != 0) {
        return ret;
    }

    if (connectInfos.empty()) {
        return 0;
    }

    if (BatchConnectWithRetry(connectInfos, currTask, remotes) != 0) {
        return 1;
    }
    currTask.Success();
    auto &nextTask = connectionTasks_.queryConnectTask;
    for (auto &rank : remotes) {
        nextTask.ip2rank.emplace(rank.second.sin_addr.s_addr, rank.first);
    }
    nextTask.status.exist = true;
    nextTask.status.failedTimes = 0;
    return 0;
}

void BipartiteRanksQpManager::Parse2SocketInfo(std::unordered_map<in_addr_t, uint32_t> &ip2rank,
                                               std::vector<HccpSocketInfo> &socketInfos, std::vector<IpType> &types)
{
    for (auto &pair : ip2rank) {
        auto pos = connections_.find(pair.second);
        if (pos != connections_.end()) {
            HccpSocketInfo info;
            info.handle = pos->second.socketHandle;
            info.fd = nullptr;
            info.remoteIp.addr.s_addr = pair.first;
            info.status = 0;
            bzero(info.tag, sizeof(info.tag));
            socketInfos.emplace_back(info);
            types.emplace_back(IpType::IpV4);
        }
    }
    if (socketInfos.size() == 0) {
        BM_LOG_INFO("ProcessQueryConnectionStateTask socketInfos.size is 0.");
    }
}

void BipartiteRanksQpManager::ProcessSocketConnectionsByIP(uint32_t getSize, std::vector<HccpSocketInfo> &socketInfos,
                                                           std::unordered_map<in_addr_t, uint32_t> &ip2rank,
                                                           std::vector<IpType> &types,
                                                           std::unordered_set<uint32_t> &connectedRanks,
                                                           uint32_t &successCount)
{
    for (auto i = 0U; i < getSize; i++) {
        if (socketInfos[i].status != 1) {
            continue;
        }
        in_addr_t addr;
        char* result {};
        addr = {socketInfos[i].remoteIp.addr.s_addr};
        result = inet_ntoa(socketInfos[i].remoteIp.addr);
        auto pos = ip2rank.find(addr);
        if (pos == ip2rank.end()) {
            BM_LOG_ERROR("get non-expected socket remote ip: " << result);
            continue;
        }

        auto rankId = pos->second;
        auto nPos = connections_.find(rankId);
        if (nPos == connections_.end()) {
            BM_LOG_ERROR("get non-expected ip: " << result << ", rank: " << rankId);
            continue;
        }

        nPos->second.socketFd = socketInfos[i].fd;
        connectedRanks.emplace(pos->second);
        ip2rank.erase(pos);
        successCount++;
    }
}

int32_t BipartiteRanksQpManager::GetSocketConn(std::vector<HccpSocketInfo> &socketInfos,
                                               QueryConnectionStateTask &currTask,
                                               std::unordered_map<in_addr_t, uint32_t> &ip2rank,
                                               std::unordered_set<uint32_t> &connectedRanks, std::vector<IpType> &types)
{
    uint32_t cnt = 0;
    uint32_t successCount = 0;
    uint32_t batchCnt = 16;
    do {
        auto socketRole = rankRole_ == HYBM_ROLE_SENDER ? 1 : 0;
        uint32_t getSize = socketInfos.size() < batchCnt ? socketInfos.size() : batchCnt;
        auto ret = DlHccpApi::RaGetSockets(socketRole, socketInfos.data(), getSize, cnt);
        if (ret != 0) {
            auto failedCount = currTask.Failed(ip2rank);
            BM_LOG_ERROR("socketRole(" << socketRole << ") side get sockets failed: "
                                       << ret << ", count: " << failedCount);
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (cnt == 0) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }
        ProcessSocketConnectionsByIP(getSize, socketInfos, ip2rank, types, connectedRanks, successCount);
        std::vector<HccpSocketInfo>::iterator it = socketInfos.begin();
        for (; it != socketInfos.end();) {
            if (it->status == 1) {
                it = socketInfos.erase(it);
            } else {
                it++;
            }
        }
    } while (socketInfos.size() > 0);
    return BM_OK;
}

int BipartiteRanksQpManager::ProcessQueryConnectionStateTask() noexcept
{
    auto &currTask = connectionTasks_.queryConnectTask;
    if (!currTask.status.exist || currTask.ip2rank.empty()) {
        currTask.status.exist = false;
        return 0;
    }

    currTask.status.exist = false;
    auto ip2rank = std::move(currTask.ip2rank);
    if (currTask.status.failedTimes > 0L) {
        std::this_thread::sleep_for(std::chrono::seconds(delay));
    }

    std::vector<HccpSocketInfo> socketInfos;
    std::vector<IpType> types{};
    Parse2SocketInfo(ip2rank, socketInfos, types);

    std::unordered_set<uint32_t> connectedRanks;
    auto ret = GetSocketConn(socketInfos, currTask, ip2rank, connectedRanks, types);
    if (ret != 0) {
        return ret;
    }

    if (!ip2rank.empty()) {
        currTask.Failed(ip2rank);
    } else {
        currTask.status.failedTimes = 0;
    }

    auto &nextTask = connectionTasks_.connectQpTask;
    nextTask.ranks.insert(connectedRanks.begin(), connectedRanks.end());
    nextTask.status.exist = true;
    nextTask.status.failedTimes = 0;
    return !ip2rank.empty();
}

int BipartiteRanksQpManager::ProcessConnectQpTask() noexcept
{
    auto &currTask = connectionTasks_.connectQpTask;
    if (!currTask.status.exist || currTask.ranks.empty()) {
        currTask.status.exist = false;
        return 0;
    }

    currTask.status.exist = false;
    auto ranks = std::move(currTask.ranks);
    if (currTask.status.failedTimes > 0L) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    int failedCount = 0;
    std::unordered_set<uint32_t> connectedQpRanks;
    for (auto rank : ranks) {
        auto pos = connections_.find(rank);
        if (pos == connections_.end()) {
            continue;
        }

        if (pos->second.qpHandle == nullptr) {
            auto ret = DlHccpApi::RaQpCreate(rdmaHandle_, 0, 2, pos->second.qpHandle);
            if (ret != 0) {
                auto times = currTask.Failed(ranks);
                BM_LOG_ERROR("create QP to " << rank << " failed: " << ret << ", times: " << times);
                failedCount++;
                continue;
            }
            pos->second.qpConnectCalled = false;
        }

        if (!pos->second.qpConnectCalled) {
            auto ret = DlHccpApi::RaQpConnectAsync(pos->second.qpHandle, pos->second.socketFd);
            if (ret != 0) {
                auto times = currTask.Failed(ranks);
                BM_LOG_ERROR("create QP to " << rank << " failed: " << ret << ", times: " << times);
                failedCount++;
                continue;
            }
            pos->second.qpConnectCalled = true;
        }

        connectedQpRanks.emplace(rank);
    }

    auto &nextTask = connectionTasks_.queryQpStateTask;
    nextTask.ranks.insert(connectedQpRanks.begin(), connectedQpRanks.end());
    nextTask.status.exist = true;
    nextTask.status.failedTimes = 0;
    return failedCount > 0;
}

int BipartiteRanksQpManager::ProcessQueryQpStateTask() noexcept
{
    auto &currTask = connectionTasks_.queryQpStateTask;
    if (!currTask.status.exist || currTask.ranks.empty()) {
        currTask.status.exist = false;
        return 0;
    }

    currTask.status.exist = false;
    auto ranks = std::move(currTask.ranks);
    if (currTask.status.failedTimes > 0L) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    auto localMrs = GenerateLocalLiteMrs();
    for (auto rank : ranks) {
        auto pos = connections_.find(rank);
        if (pos == connections_.end()) {
            continue;
        }

        auto ret = DlHccpApi::RaGetQpStatus(pos->second.qpHandle, pos->second.qpStatus);
        if (ret != 0) {
            auto times = currTask.Failed(ranks);
            BM_LOG_ERROR("get QP status to " << rank << " failed: " << ret << ", fail times: " << times);
            currTask.ranks.emplace(rank);
            continue;
        }

        BM_LOG_INFO("get QP status to " << rank << " success. qpStatus: " << pos->second.qpStatus);
        if (pos->second.qpStatus != 1) {
            currTask.ranks.emplace(rank);
            continue;
        }
        auto remoteMrs = GenerateRemoteLiteMrs(rank);
        SetQpHandleRegisterMr(pos->second.qpHandle, localMrs, true);
        SetQpHandleRegisterMr(pos->second.qpHandle, remoteMrs, false);
        WriteGuard lockGuard(qpLock_);
        userQpInfo_[rank].qpHandle = pos->second.qpHandle;
    }

    if (!currTask.ranks.empty()) {
        currTask.status.exist = true;
        currTask.status.failedTimes++;
        return 1;
    }

    return 0;
}

void BipartiteRanksQpManager::ProcessUpdateLocalMrTask() noexcept
{
    auto &currTask = connectionTasks_.updateMrTask;
    std::unique_lock<std::mutex> uniqueLock{currTask.locker};
    if (!currTask.status.exist) {
        return;
    }
    currTask.status.exist = false;
    uniqueLock.unlock();

    auto localMRs = GenerateLocalLiteMrs();
    for (auto it = connections_.begin(); it != connections_.end(); ++it) {
        if (it->second.qpHandle == nullptr || it->second.qpStatus != 1) {
            continue;
        }
        SetQpHandleRegisterMr(it->second.qpHandle, localMRs, true);
    }
}

void BipartiteRanksQpManager::ProcessUpdateRemoteMrTask() noexcept
{
    auto &currTask = connectionTasks_.updateRemoteMrTask;
    std::unique_lock<std::mutex> uniqueLock{currTask.locker};
    if (!currTask.status.exist) {
        return;
    }
    currTask.status.exist = false;
    auto addedMrRanks = std::move(currTask.addedMrRanks);
    uniqueLock.unlock();
    for (auto remoteRank : addedMrRanks) {
        auto mrs = GenerateRemoteLiteMrs(remoteRank);
        auto pos = connections_.find(remoteRank);
        if (pos == connections_.end()) {
            continue;
        }

        SetQpHandleRegisterMr(pos->second.qpHandle, mrs, false);
    }
}

void BipartiteRanksQpManager::CloseServices() noexcept
{
    if (backGroundThread_ != nullptr) {
        managerRunning_.store(false);
        cond_.notify_one();
        backGroundThread_->join();
        backGroundThread_ = nullptr;
    }

    std::vector<HccpSocketCloseInfo> socketCloseInfos;
    for (auto it = connections_.begin(); it != connections_.end(); ++it) {
        if (it->second.qpHandle != nullptr) {
            auto ret = DlHccpApi::RaQpDestroy(it->second.qpHandle);
            if (ret != 0) {
                BM_LOG_WARN("destroy QP to server: " << it->first << " failed: " << ret);
            }
            it->second.qpHandle = nullptr;
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

    auto ret = DlHccpApi::RaSocketBatchClose(socketCloseInfos.data(), socketCloseInfos.size());
    if (ret != 0) {
        BM_LOG_INFO("close sockets return: " << ret);
    }

    for (auto it = connections_.begin(); it != connections_.end(); ++it) {
        ret = DlHccpApi::RaSocketDeinit(it->second.socketHandle);
        if (ret != 0) {
            BM_LOG_INFO("deinit socket to server: " << it->first << " return: " << ret);
        }
    }

    for (auto &conn : connectionView_) {
        conn = nullptr;
    }
    connections_.clear();
    DestroyServerSocket();
}

void BipartiteRanksQpManager::ProcessRankRemoval(uint32_t rank, std::vector<HccpSocketCloseInfo>& socketCloseInfos,
                                                 std::vector<HccpSocketWhiteListInfo>& whitelist) noexcept
{
    std::unique_lock<std::mutex> uniqueLock{mutex_};
    auto it = connections_.find(rank);
    if (it != connections_.end()) {
        if (rankRole_ == HYBM_ROLE_RECEIVER) {
            HccpSocketWhiteListInfo info{};
            info.remoteIp.addr = it->second.remoteNet.sin_addr;
            info.connLimit = QP_MAX_CONNECTIONS;
            bzero(info.tag, sizeof(info.tag));
            whitelist.emplace_back(info);
        }

        if (it->second.qpHandle != nullptr) {
            auto ret = DlHccpApi::RaQpDestroy(it->second.qpHandle);
            if (ret != 0) {
                BM_LOG_WARN("destroy QP to server: " << it->first << " failed: " << ret);
            }
            it->second.qpHandle = nullptr;
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
    uniqueLock.unlock();
}

// 上层保证没有并发调用
int BipartiteRanksQpManager::RemoveRanks(const std::unordered_set<uint32_t> &ranks) noexcept
{
    std::vector<HccpSocketCloseInfo> socketCloseInfos;
    std::vector<HccpSocketWhiteListInfo> whitelist;
    BM_LOG_INFO("BipartiteRanksQpManager remove ranks start");
    int ret;
    for (auto rank : ranks) {
        {
            WriteGuard lockGuard(qpLock_);
            userQpInfo_[rank].qpHandle = nullptr;
            userQpInfo_[rank].ref = 0;
        }
        ProcessRankRemoval(rank, socketCloseInfos, whitelist);
    }
    if (whitelist.size() > 0) {
        ret = DlHccpApi::RaSocketWhiteListDel(serverSocketHandle_, whitelist.data(), whitelist.size());
        if (ret != 0) {
            BM_LOG_WARN("del wlist failed: " << ret);
        }
    }
    if (socketCloseInfos.size() > 0) {
        ret = DlHccpApi::RaSocketBatchClose(socketCloseInfos.data(), socketCloseInfos.size());
        if (ret != 0) {
            BM_LOG_WARN("close sockets return: " << ret);
        }
    }
    std::unique_lock<std::mutex> uniqueLock{mutex_};
    for (auto rank : ranks) {
        currentRanksInfo_.erase(rank);
        auto it = connections_.find(rank);
        if (it == connections_.end()) {
            continue;
        }
        if (rankRole_ != HYBM_ROLE_RECEIVER) {
            ret = DlHccpApi::RaSocketDeinit(it->second.socketHandle);
            if (ret != 0) {
                BM_LOG_WARN("deinit socket to server: " << it->first << " return: " << ret);
            }
        }
        connections_.erase(rank);
        connectionView_[rank] = nullptr;
        BM_LOG_INFO("remove connection infos rank id=" << rank);
    }
    uniqueLock.unlock();
    BM_LOG_INFO("BipartiteRanksQpManager remove ranks end");
    return BM_OK;
}

std::vector<lite_mr_info> BipartiteRanksQpManager::GenerateLocalLiteMrs() noexcept
{
    std::vector<lite_mr_info> localMrs;
    for (auto it = currentLocalMrs_.begin(); it != currentLocalMrs_.end(); ++it) {
        lite_mr_info info;
        info.key = it->second.lkey;
        info.addr = it->second.address;
        info.len = it->second.size;
        localMrs.emplace_back(info);
    }
    return localMrs;
}

std::vector<lite_mr_info> BipartiteRanksQpManager::GenerateRemoteLiteMrs(uint32_t rankId) noexcept
{
    std::vector<lite_mr_info> remoteMrs;
    auto pos = currentRanksInfo_.find(rankId);
    if (pos == currentRanksInfo_.end()) {
        return remoteMrs;
    }

    for (auto it = pos->second.memoryMap.begin(); it != pos->second.memoryMap.end(); ++it) {
        lite_mr_info info;
        info.key = it->second.lkey;
        info.addr = it->second.address;
        info.len = it->second.size;
        remoteMrs.emplace_back(info);
    }
    return remoteMrs;
}

void BipartiteRanksQpManager::GenDiffInfoChangeRanks(const std::unordered_map<uint32_t, ConnectRankInfo> &last,
                                                     std::unordered_map<uint32_t, sockaddr_in> &addedRanks,
                                                     std::unordered_set<uint32_t> &addMrRanks) noexcept
{
    for (auto it = currentRanksInfo_.begin(); it != currentRanksInfo_.end(); ++it) {
        auto pos = last.find(it->first);
        if (pos == last.end()) {
            addedRanks.emplace(it->first, it->second.network);
            continue;
        }
        for (auto mit = it->second.memoryMap.begin(); mit != it->second.memoryMap.end(); ++mit) {
            if (pos->second.memoryMap.find(mit->first) == pos->second.memoryMap.end()) {
                addMrRanks.emplace(it->first);
                break;
            }
        }
    }
}

void BipartiteRanksQpManager::GenTaskFromChangeRanks(const std::unordered_map<uint32_t, sockaddr_in> &addedRanks,
                                                     const std::unordered_set<uint32_t> &addMrRanks) noexcept
{
    if (rankRole_ == HYBM_ROLE_RECEIVER) {
        auto &task = connectionTasks_.whitelistTask;
        std::unique_lock<std::mutex> taskLocker{task.locker};
        for (auto it = addedRanks.begin(); it != addedRanks.end(); ++it) {
            task.remoteIps.emplace(it->first, it->second.sin_addr);
        }
        task.status.exist = !task.remoteIps.empty();
        task.status.failedTimes = 0;
    } else {
        auto &task = connectionTasks_.clientConnectTask;
        std::unique_lock<std::mutex> taskLocker{task.locker};
        for (auto it = addedRanks.begin(); it != addedRanks.end(); ++it) {
            task.remoteAddress.emplace(it->first, it->second);
        }
        task.status.exist = !task.remoteAddress.empty();
        task.status.failedTimes = 0;
    }

    auto &task = connectionTasks_.updateRemoteMrTask;
    std::unique_lock<std::mutex> taskLocker{task.locker};
    task.addedMrRanks.insert(addMrRanks.begin(), addMrRanks.end());
    task.status.exist = !task.addedMrRanks.empty();
    task.status.failedTimes = 0;
    taskLocker.unlock();

    if (addedRanks.empty() && addMrRanks.empty()) {
        return;
    }

    cond_.notify_one();
}

void BipartiteRanksQpManager::SetQpHandleRegisterMr(void *qpHandle, const std::vector<lite_mr_info> &mrs,
                                                    bool local) noexcept
{
    if (qpHandle == nullptr) {
        return;
    }

    auto qp = (ra_qp_handle *)qpHandle;
    auto dest = local ? qp->local_mr : qp->rem_mr;
    pthread_mutex_lock(&qp->qp_mutex);
    for (auto i = 0U; i < mrs.size() && i < RA_MR_MAX_NUM - 1U; i++) {
        dest[i + 1] = mrs[i];
    }
    pthread_mutex_unlock(&qp->qp_mutex);
}
}
}
}
}
