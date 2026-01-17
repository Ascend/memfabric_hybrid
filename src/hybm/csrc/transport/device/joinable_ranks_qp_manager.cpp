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
#include <chrono>
#include "hybm_logger.h"
#include "dl_hccp_api.h"
#include "dl_acl_api.h"
#include "joinable_ranks_qp_manager.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
constexpr int MR_INFO_ACCESS = 7;
constexpr int WAIT_TIME_MS = 300;
JoinableRanksQpManager::JoinableRanksQpManager(uint32_t userDeviceId, uint32_t deviceId, uint32_t rankId,
                                               uint32_t rankCount, sockaddr_in devNet) noexcept
    : DeviceQpManager(deviceId, rankId, rankCount, devNet, HYBM_ROLE_PEER)
{
    connections_.resize(rankCount);
    qpArray_.resize(rankCount, nullptr);
    userDeviceId_ = userDeviceId;
}

JoinableRanksQpManager::~JoinableRanksQpManager() noexcept
{
    CloseServices();
}

int JoinableRanksQpManager::SetRemoteRankInfo(const std::unordered_map<uint32_t, ConnectRankInfo> &ranks) noexcept
{
    std::unique_lock<std::mutex> uniqueLock{mutex_};
    for (auto it = ranks.begin(); it != ranks.end(); ++it) {
        if (it->first >= rankCount_) {
            continue;
        }
        connections_[it->first].remoteNet = it->second.network;
        if (it->first < rankId_) {
            newServers_.emplace(it->first);
        }
        if (it->first > rankId_) {
            newClients_.emplace(it->first);
        }
    }
    uniqueLock.unlock();

    if (started_.load()) {
        cond_.notify_all();
    }

    return BM_OK;
}

// 上层保证 SetRemoteRankInfo和RemoveRanks操作不并发
int JoinableRanksQpManager::RemoveRanks(const std::unordered_set<uint32_t> &ranks) noexcept
{
    std::unique_lock<std::mutex> uniqueLock{mutex_};
    for (auto rank : ranks) {
        if (rank < rankId_) {
            removedServerRanks_.emplace(rank);
        } else {
            removedClientRanks_.emplace(rank);
        }
    }
    uniqueLock.unlock();

    if (started_.load()) {
        cond_.notify_all();
    }

    return BM_OK;
}

int JoinableRanksQpManager::Startup(void *rdma) noexcept
{
    if (rdma == nullptr) {
        BM_LOG_ERROR("input rdma is null");
        return BM_INVALID_PARAM;
    }
    std::unique_lock<std::mutex> unique_lock(mutex_);
    if (started_.load()) {
        BM_LOG_DEBUG("already started.");
        return BM_OK;
    }

    rdmaHandle_ = rdma;
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

void JoinableRanksQpManager::Shutdown() noexcept
{
    std::unique_lock<std::mutex> unique_lock(mutex_);
    std::set<uint32_t> list;
    for (uint32_t i = 0; i < connections_.size(); ++i) {
        list.insert(i);
    }
    RemoveRanksProcess(list);
    started_.store(false);
    running_.store(false);
    rdmaHandle_ = nullptr;
    qpArray_.clear();
    connections_.clear();
    newClients_.clear();
    newServers_.clear();
    removedClientRanks_.clear();
    removedServerRanks_.clear();
    unique_lock.unlock();
    CloseServices();
}

UserQpInfo *JoinableRanksQpManager::GetQpHandleWithRankId(uint32_t rankId) noexcept
{
    if (rankId >= rankCount_) {
        BM_LOG_ERROR("invalid rank id: " << rankId << ", rank count: " << rankCount_);
        return nullptr;
    }
    ReadGuard lockGuard(qpLock_);
    if (qpArray_[rankId] != nullptr) {
        qpArray_[rankId]->ref.fetch_add(1U);
        return qpArray_[rankId];
    } else {
        return nullptr;
    }
}

void JoinableRanksQpManager::PutQpHandle(UserQpInfo *qp) const noexcept
{
    uint32_t val = qp->ref.fetch_sub(1U);
    if (val == 1U) { // 返回减之前的值
        auto ret = DlHccpApi::RaQpDestroy(qp->qpHandle);
        if (ret != 0) {
            BM_LOG_WARN("close qp from " << rankId_ << " failed, ret: " << ret);
        }
        delete qp;
    }
}

bool JoinableRanksQpManager::CheckQpReady(const std::vector<uint32_t> &rankIds) const noexcept
{
    for (auto rankId : rankIds) {
        if (rankId == rankId_) {
            continue;
        }
        if (rankId >= rankCount_) {
            BM_LOG_ERROR("invalid rank id: " << rankId << ", rank count: " << rankCount_);
            return false;
        }
        if (connections_[rankId].qpStatus != 1) {
            return false;
        }
    }
    return true;
}

void JoinableRanksQpManager::CloseServices() noexcept
{
    running_.store(false);
    cond_.notify_all();
    if (serverConnectThread_ != nullptr) {
        serverConnectThread_->join();
        serverConnectThread_ = nullptr;
    }
    if (clientConnectThread_ != nullptr) {
        clientConnectThread_->join();
        clientConnectThread_ = nullptr;
    }
}

int JoinableRanksQpManager::StartServerSide() noexcept
{
    if (rankId_ + 1U == rankCount_) {
        return BM_OK;
    }

    auto ret = CreateServerSocket();
    if (ret != BM_OK) {
        BM_LOG_ERROR("create server socket failed: " << ret);
        return ret;
    }

    serverConnectThread_ = std::make_shared<std::thread>([this]() { ServerSideRunLoop(); });
    return BM_OK;
}

int JoinableRanksQpManager::StartClientSide() noexcept
{
    if (rankId_ > 0U) {
        clientConnectThread_ = std::make_shared<std::thread>([this]() { ClientSideRunLoop(); });
    }
    return BM_OK;
}

void JoinableRanksQpManager::ServerSideHandleNewClients(const std::set<uint32_t> &newRanks) noexcept
{
    auto ret = GenerateWhiteList(newRanks);
    if (ret != 0) {
        BM_LOG_ERROR("generate white list failed: " << ret);
        return;
    }

    ret = WaitSocketConnections(newRanks);
    if (ret != 0) {
        BM_LOG_ERROR("make socket connections for server side failed: " << ret);
        return;
    }

    MakeQpConnections(newRanks);
    WaitQpConnections(newRanks);
}

void JoinableRanksQpManager::ServerSideRunLoop() noexcept
{
    DlAclApi::AclrtSetDevice(userDeviceId_);

    while (running_) {
        std::unique_lock<std::mutex> uniqueLock{mutex_};
        cond_.wait_for(uniqueLock, std::chrono::milliseconds(WAIT_TIME_MS));
        if (newClients_.empty() && removedClientRanks_.empty() && running_) {
            cond_.wait_for(uniqueLock, std::chrono::minutes(1));
        }
        if (!running_) {
            break;
        }
        auto newClients = newClients_;
        auto removedRanks = std::move(removedClientRanks_);
        uniqueLock.unlock();

        if (!newClients.empty()) {
            ServerSideHandleNewClients(newClients);
        }

        if (!removedRanks.empty()) {
            RemoveRanksProcess(removedRanks);
        }
    }
}

void JoinableRanksQpManager::ClientSideRunLoop() noexcept
{
    DlAclApi::AclrtSetDevice(userDeviceId_);
    while (running_) {
        std::unique_lock<std::mutex> uniqueLock{mutex_};
        cond_.wait_for(uniqueLock, std::chrono::milliseconds(WAIT_TIME_MS));
        if (newServers_.empty() && removedServerRanks_.empty() && running_) {
            cond_.wait_for(uniqueLock, std::chrono::minutes(1));
        }
        if (!running_) {
            break;
        }
        auto newServers = newServers_;
        auto removedRanks = std::move(removedServerRanks_);
        uniqueLock.unlock();

        if (!newServers.empty()) {
            auto ret = CreateConnectionToServers(newServers);
            if (ret != 0) {
                BM_LOG_ERROR("create connection to server failed: " << ret);
            }

            WaitSocketConnections(newServers);
            MakeQpConnections(newServers);
            WaitQpConnections(newServers);
        }

        if (!removedRanks.empty()) {
            RemoveRanksProcess(removedRanks);
        }
    }
}

int JoinableRanksQpManager::WaitSocketConnections(const std::set<uint32_t> &newRanks) noexcept
{
    if (newRanks.empty()) {
        return BM_OK;
    }

    bool socketRole = *newRanks.begin() < rankId_ ? 1 : 0;
    uint32_t successCount = 0;
    uint32_t cnt = 0;
    std::vector<HccpSocketInfo> socketInfos;
    std::unordered_map<in_addr_t, uint32_t> addr2rank;
    for (auto rankId : newRanks) {
        if (connections_[rankId].remoteNet.sin_addr.s_addr == 0) {
            BM_LOG_ERROR("rankId: " << rankId << ", no ip address.");
            continue;
        }

        if (connections_[rankId].socketFd != nullptr) {
            continue;
        }

        HccpSocketInfo info{};
        info.handle = connections_[rankId].socketHandle;
        info.fd = nullptr;
        info.remoteIp.addr = connections_[rankId].remoteNet.sin_addr;
        info.status = 0;
        bzero(info.tag, sizeof(info.tag));
        socketInfos.push_back(info);
        addr2rank.emplace(info.remoteIp.addr.s_addr, rankId);
    }

    if (socketInfos.empty()) {
        return BM_OK;
    }

    std::unordered_set<uint32_t> connectedRanks;
    uint32_t batchCnt = 16U;
    do {
        uint32_t getSize = socketInfos.size() < batchCnt ? socketInfos.size() : batchCnt;
        auto ret = DlHccpApi::RaGetSockets(socketRole, socketInfos.data(), getSize, cnt);
        if (ret != 0) {
            BM_LOG_ERROR("socketRole(" << socketRole << ") side get sockets failed: " << ret);
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100L));
        if (cnt == 0) {
            continue;
        }
        for (auto i = 0U; i < getSize; i++) {
            if (socketInfos[i].status != 1) {
                continue;
            }
            auto socketInfoPos = addr2rank.find(socketInfos[i].remoteIp.addr.s_addr);
            if (socketInfoPos == addr2rank.end()) {
                BM_LOG_ERROR("socket ip(" << DescribeIPv4(socketInfos[i].remoteIp.addr) << ") should not exist.");
                continue;
            }
            auto rankId = socketInfoPos->second;
            if (rankId >= rankCount_) {
                BM_LOG_ERROR("socket ip(" << DescribeIPv4(socketInfos[i].remoteIp.addr) << ") should not exist.");
                continue;
            }
            if (connections_[rankId].socketFd != nullptr) {
                BM_LOG_ERROR("get ip(" << DescribeIPv4(socketInfos[i].remoteIp.addr) << ") already get socket fd.");
                continue;
            }
            connections_[rankId].socketFd = socketInfos[i].fd;
            successCount++;
        }
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

void JoinableRanksQpManager::MakeQpConnections(const std::set<uint32_t> &newRanks) noexcept
{
    if (newRanks.empty()) {
        return;
    }

    for (auto rankId : newRanks) {
        if (connections_[rankId].socketFd == nullptr) {
            continue;
        }
        if (connections_[rankId].qpHandle == nullptr) {
            void *qpHandle = nullptr;
            auto info = new (std::nothrow) UserQpInfo;
            BM_ASSERT_RET_VOID(info != nullptr);
            auto ret = DlHccpApi::RaQpCreate(rdmaHandle_, 0, 4, qpHandle);
            if (ret != 0) {
                BM_LOG_ERROR("create QP to " << rankId << " failed: " << ret);
                delete info;
                continue;
            }

            connections_[rankId].qpHandle = qpHandle;
            connections_[rankId].qpConnectCalled = false;

            info->ref.store(1U);
            info->qpHandle = qpHandle;
            WriteGuard lockGuard(qpLock_);
            qpArray_[rankId] = info;
        }

        if (!connections_[rankId].qpConnectCalled) {
            auto ret = DlHccpApi::RaQpConnectAsync(connections_[rankId].qpHandle, connections_[rankId].socketFd);
            if (ret != 0) {
                BM_LOG_ERROR("create QP from " << rankId_ << " to " << rankId << " failed: " << ret);
                continue;
            }
            BM_LOG_INFO("create QP success from " << rankId_ << " to " << rankId);
            connections_[rankId].qpConnectCalled = true;
        }
    }
}

void JoinableRanksQpManager::WaitQpConnections(const std::set<uint32_t> &newRanks) noexcept
{
    if (newRanks.empty()) {
        return;
    }

    std::set<uint32_t> finishedRanks;
    for (auto rankId : newRanks) {
        if (connections_[rankId].qpHandle == nullptr || !connections_[rankId].qpConnectCalled) {
            continue;
        }

        if (connections_[rankId].qpStatus == 1) {
            finishedRanks.emplace(rankId);
            continue;
        }

        auto ret = DlHccpApi::RaGetQpStatus(connections_[rankId].qpHandle, connections_[rankId].qpStatus);
        if (ret != 0) {
            BM_LOG_ERROR("get QP status to " << rankId << " failed: " << ret);
            continue;
        }

        if (connections_[rankId].qpStatus == 1) {
            BM_LOG_INFO("from " << rankId_ << " to " << rankId << " query qp ready.");
            finishedRanks.emplace(rankId);
        }
    }

    std::unique_lock<std::mutex> uniqueLock{mutex_};
    for (auto rankId : finishedRanks) {
        if (rankId < rankId_) {
            newServers_.erase(rankId);
        } else {
            newClients_.erase(rankId);
        }
    }
}

int JoinableRanksQpManager::GenerateWhiteList(const std::set<uint32_t> &newClients) noexcept
{
    std::vector<HccpSocketWhiteListInfo> whitelist;
    for (auto rankId : newClients) {
        if (rankId <= rankId_ || rankId >= rankCount_) {
            BM_LOG_ERROR("new client rankId: " << rankId << "invalid. self: " << rankId_ << ", total: " << rankCount_);
            return BM_ERROR;
        }

        if (connections_[rankId].remoteNet.sin_addr.s_addr == 0) {
            BM_LOG_ERROR("rankId: " << rankId << ", no ip address.");
            return BM_ERROR;
        }

        if (connections_[rankId].socketHandle != nullptr) {
            continue;
        }

        HccpSocketWhiteListInfo info{};
        info.remoteIp.addr = connections_[rankId].remoteNet.sin_addr;
        info.connLimit = rankCount_;
        bzero(info.tag, sizeof(info.tag));
        whitelist.emplace_back(info);
        connections_[rankId].socketHandle = serverSocketHandle_;
    }

    if (whitelist.empty()) {
        return BM_OK;
    }

    uint32_t batchSize = 16;
    for (size_t i = 0; i < whitelist.size(); i += batchSize) {
        size_t currentBatchSize = (whitelist.size() - i) >= batchSize ? batchSize : (whitelist.size() - i);
        auto batchStart = whitelist.begin() + i;
        auto batchEnd = batchStart + currentBatchSize;
        std::vector<HccpSocketWhiteListInfo> currentBatch(batchStart, batchEnd);
        auto ret = DlHccpApi::RaSocketWhiteListAdd(serverSocketHandle_, currentBatch.data(), currentBatch.size());
        if (ret != 0) {
            BM_LOG_ERROR("RaSocketWhiteListAdd() with size=" << currentBatch.size() << " failed: " << ret);
            return BM_ERROR;
        }
    }

    return BM_OK;
}

int JoinableRanksQpManager::CreateConnectionToServers(const std::set<uint32_t> &newServers) noexcept
{
    std::vector<HccpSocketConnectInfo> connectInfos;
    std::set<uint32_t> rollbacks;
    for (auto rankId : newServers) {
        if (rankId >= rankId_) {
            BM_LOG_ERROR("new server rankId: " << rankId << "invalid. self: " << rankId_);
            return BM_ERROR;
        }

        if (connections_[rankId].remoteNet.sin_addr.s_addr == 0) {
            BM_LOG_ERROR("rankId: " << rankId << ", no ip address.");
            return BM_ERROR;
        }

        if (connections_[rankId].socketHandle != nullptr) {
            continue;
        }

        auto socketHandle = CreateLocalSocket();
        if (socketHandle == nullptr) {
            BM_LOG_ERROR("create local socket to connect to server: " << rankId << " failed");
            return BM_ERROR;
        }
        connections_[rankId].socketHandle = socketHandle;
        rollbacks.emplace(rankId);

        HccpSocketConnectInfo connectInfo;
        connectInfo.handle = socketHandle;
        connectInfo.remoteIp.addr = connections_[rankId].remoteNet.sin_addr;
        connectInfo.port = connections_[rankId].remoteNet.sin_port;
        bzero(connectInfo.tag, sizeof(connectInfo.tag));
        BM_LOG_INFO("add connecting server " << connectInfo);
        connectInfos.emplace_back(connectInfo);
    }

    if (connectInfos.empty()) {
        return BM_OK;
    }
    uint32_t batchSize = 16;
    for (size_t i = 0; i < connectInfos.size(); i += batchSize) {
        size_t currentBatchSize = (connectInfos.size() - i) >= batchSize ? batchSize : (connectInfos.size() - i);
        auto batchStart = connectInfos.begin() + i;
        auto batchEnd = batchStart + currentBatchSize;
        std::vector<HccpSocketConnectInfo> currentBatch(batchStart, batchEnd);

        auto ret = DlHccpApi::RaSocketBatchConnect(currentBatch.data(), currentBatch.size());
        if (ret != 0) {
            BM_LOG_ERROR("connect to all servers failed: " << ret << ", servers count = " << connectInfos.size());
            return BM_ERROR;
        }
    }
    return BM_OK;
}

void JoinableRanksQpManager::RemoveRanksProcess(const std::set<uint32_t> &ranks) noexcept
{
    std::map<uint32_t, ConnectionChannel> removedConnections;
    for (auto rank : ranks) {
        if (rank >= connections_.size() || rank == rankId_) {
            continue;
        }
        removedConnections.emplace(rank, connections_[rank]);
        bzero(&connections_[rank], sizeof(ConnectionChannel));
    }

    for (auto it = removedConnections.begin(); it != removedConnections.end(); ++it) {
        BM_LOG_INFO("close connection from " << rankId_ << " to " << it->first);
        if (it->second.qpHandle != nullptr) {
            WriteGuard guard(qpLock_);
            auto info = qpArray_[it->first];
            qpArray_[it->first] = nullptr;
            PutQpHandle(info);
        }

        HccpSocketCloseInfo closeInfo{};
        closeInfo.handle = it->second.socketHandle;
        closeInfo.fd = it->second.socketFd;
        closeInfo.linger = 0;
        auto ret = DlHccpApi::RaSocketBatchClose(&closeInfo, 1U);
        if (ret != 0) {
            BM_LOG_WARN("close socket from " << rankId_ << " to " << it->first << " failed: " << ret);
        } else {
            BM_LOG_INFO("close socket from " << rankId_ << " to " << it->first << " successful: " << ret);
        }
    }
}

} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock