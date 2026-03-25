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
#include "smem_ha_config_store.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <cerrno>
#include <cstring>

#include <array>
#include <chrono>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

#include "smem_config_store_logger.h"
#include "network_endpoint_util.h"
#include "mf_ipv4_validator.h"
#include "mf_str_util.h"
#include "smem_message_packer.h"
#include "smem_tcp_config_store_ssl_helper.h"

namespace ock {
namespace smem {

using namespace ock::mf;

namespace {
constexpr char BACKEND_LOCK_NAME[] = "backend";
}

// ============================================================================
// HaConfigStore - Construction / Destruction
// ============================================================================

HaConfigStore::HaConfigStore(StoreBackendPtr backend, TcpConfigStorePtr clientDelegate, const std::string &endpoints,
                             uint32_t worldSize, std::string instanceId)
    : endpoints_(endpoints), worldSize_(worldSize), backendLockName_(BACKEND_LOCK_NAME), backend_(std::move(backend)),
      clientDelegate_(std::move(clientDelegate))
{
    (void)instanceId;
    SM_LOG_DEBUG("HaConfigStore constructing, endpoints: "
                 << endpoints << ", worldSize: " << worldSize << ", backendLockName: " << backendLockName_);
}

void HaConfigStore::Uninitialize() noexcept
{
    stopFlag_.store(true, std::memory_order_release);

    // Join health check thread
    if (healthCheckThread_.joinable()) {
        healthCheckThread_.join();
    }

    // Join re-election thread (guaranteed to terminate since stopFlag_ is set)
    {
        std::lock_guard<std::mutex> lock(reElectionThreadMutex_);
        if (reElectionThread_.joinable()) {
            reElectionThread_.join();
        }
    }

    if (clientDelegate_ != nullptr) {
        SM_LOG_DEBUG("Shutting down clientDelegate_");
        clientDelegate_->Shutdown();
        clientDelegate_ = nullptr;
    }

    {
        std::unique_lock<std::shared_mutex> lock(delegateRwLock_);
        if (serverDelegate_ != nullptr) {
            SM_LOG_DEBUG("Shutting down serverDelegate_");
            serverDelegate_->Shutdown();
            serverDelegate_ = nullptr;
        }
        isLeader_.store(false, std::memory_order_release);
    }

    // Close backend connection after all threads/delegates are stopped
    {
        std::lock_guard<std::mutex> bLock(backendMutex_);
        backend_->UnInitialize();
    }
    SM_LOG_DEBUG("Backend connection closed");
}

HaConfigStore::~HaConfigStore()
{
    SM_LOG_DEBUG("HaConfigStore destructor called, endpoints: " << endpoints_);
    Uninitialize();
    SM_LOG_DEBUG("HaConfigStore destroyed");
}

// ============================================================================
// Startup / Election
// ============================================================================

Result HaConfigStore::Startup(const smem_tls_config &tlsConfig) noexcept
{
    SM_LOG_INFO("HaConfigStore starting, endpoints: " << endpoints_);

    if (clientDelegate_ == nullptr) {
        SM_LOG_ERROR("clientDelegate_ is null, cannot start");
        return SM_NOT_INITIALIZED;
    }

    tlsConfig_ = tlsConfig;

    SM_LOG_INFO("Entering election loop");
    RunElectionLoop();
    StartHealthCheckThread();
    SM_LOG_INFO("HaConfigStore started successfully");
    return SM_OK;
}

bool HaConfigStore::InitBackendConnection() noexcept
{
    SM_LOG_INFO("Initializing backend connection: " << endpoints_);
    int backendRet = backend_->Initialize(endpoints_, "", "");
    if (backendRet != 0) {
        SM_LOG_ERROR("Failed to init backend client, endpoints: " << endpoints_ << ", ret: " << backendRet);
        return false;
    }

    SM_LOG_INFO("Backend connection initialized, endpoints: " << endpoints_);
    return true;
}

void HaConfigStore::RandomBackoff() noexcept
{
    static thread_local std::mt19937 generator(static_cast<unsigned>(
        std::chrono::steady_clock::now().time_since_epoch().count() ^ reinterpret_cast<uintptr_t>(&generator)));
    static thread_local std::uniform_int_distribution<int> distribution(MIN_SLEEP_MS, MAX_SLEEP_MS);

    int sleepDuration = distribution(generator);
    SM_LOG_DEBUG("Random backoff: " << sleepDuration << "ms");
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepDuration));
}

bool HaConfigStore::IsLeaderAlive(std::string &leaderAddr) noexcept
{
    int backendRet = backend_->Get(KEY_LEADER, leaderAddr);
    if (backendRet != SUCCESS) {
        SM_LOG_WARN("Backend Get failed, key: " << KEY_LEADER << ", ret: " << backendRet);
        return false;
    }

    if (leaderAddr.empty()) {
        SM_LOG_DEBUG("No leader registered in backend");
        return false;
    }

    SM_LOG_DEBUG("Backend leader: " << leaderAddr << ", checking connectivity");

    std::string fullUrl = "tcp://" + leaderAddr;
    std::string ip;
    uint16_t port = 0;
    if (!NetworkEndpointUtil::ExtractIpAndPort(fullUrl, ip, port)) {
        SM_LOG_ERROR("Invalid leader address format: " << leaderAddr);
        return false;
    }

    bool reachable = NetworkEndpointUtil::CheckConnectivity(ip, port);
    SM_LOG_INFO("Leader " << leaderAddr << " reachable: " << (reachable ? "true" : "false"));
    return reachable;
}

Result HaConfigStore::TryBecomeLeader() noexcept
{
    constexpr size_t kTcpSchemeLen = 6;
    SM_ASSERT_RETURN(NetworkEndpointUtil::ExtractIpAndPort(endpoints_, leaderBindIp_, leaderBindPort_), SM_ERROR);
    SM_ASSERT_RETURN(NetworkEndpointUtil::FindAvailablePort(leaderBindPort_, endpoints_.find('[') != std::string::npos),
                     SM_ERROR);
    SM_ASSERT_RETURN(NetworkEndpointUtil::GetLocalIpWithTarget(leaderBindIp_, leaderBindIp_), SM_ERROR);
    SM_LOG_INFO("Attempting to become leader, addr: " << leaderBindIp_ << ":" << leaderBindPort_);

    // Start server
    StartServer();
    if (!isLeader_.load(std::memory_order_acquire)) {
        SM_LOG_ERROR("StartServer failed");
        return SM_ERROR;
    }
    SM_LOG_INFO("AccStoreServer started successfully");

    // Register as leader in backend
    const std::string myEndpoint = NetworkEndpointUtil::BuildEndpoint("tcp", leaderBindIp_, leaderBindPort_);
    SM_ASSERT_RETURN(!myEndpoint.empty(), SM_ERROR);
    std::string myAddr = myEndpoint.substr(kTcpSchemeLen);
    auto registerRet = backend_->Put(KEY_LEADER, myAddr, PUT_LEASE_TTL_SEC);
    if (registerRet != 0) {
        SM_LOG_ERROR("Failed to register leader address in backend: " << myAddr << ", ret: " << registerRet);
        StopServer();
        return SM_ERROR;
    }
    SM_LOG_INFO("Registered in backend: " << myAddr << ", TTL: " << PUT_LEASE_TTL_SEC << "s");

    // Connect client delegate to self
    auto clientRet = ConnectClient(leaderBindIp_, leaderBindPort_);
    if (clientRet != SM_OK) {
        StopServer();
        (void)backend_->Delete(KEY_LEADER);
        isLeader_.store(false, std::memory_order_release);
        return SM_ERROR;
    }
    SM_LOG_INFO("Self-connection established");

    return SM_OK;
}

void HaConfigStore::RunElectionLoop() noexcept
{
    pthread_setname_np(pthread_self(), "election-loop");
    uint32_t electionAttempt = 0;
    while (!stopFlag_.load(std::memory_order_acquire)) {
        ++electionAttempt;
        SM_LOG_INFO("Election attempt #" << electionAttempt << " starting, endpoints: " << endpoints_);
        RandomBackoff();
        if (stopFlag_.load(std::memory_order_acquire)) {
            SM_LOG_INFO("Stop flag detected, exiting election loop");
            break;
        }
        std::lock_guard<std::mutex> bLock(backendMutex_);
        if (!InitBackendConnection()) {
            SM_LOG_ERROR("Backend connection failed, will retry");
            continue;
        }
        std::string leaderAddr;
        // Check if there's an alive leader
        if (IsLeaderAlive(leaderAddr)) {
            SM_LOG_INFO("Found alive leader: " << leaderAddr << ", becoming follower");
            if (BecomeFollower(leaderAddr) != SM_OK) {
                SM_LOG_ERROR("Becoming follower failed, leader: " << leaderAddr);
                backend_->UnInitialize();
                continue;
            }
            backend_->UnInitialize();
            SM_LOG_INFO("Election loop exiting: became follower of " << leaderAddr);
            return;
        }
        SM_LOG_INFO("No alive leader found, attempting to acquire lock");
        {
            std::unique_lock<std::shared_mutex> lock(delegateRwLock_);
            isLeader_.store(false, std::memory_order_release);
        }
        // Attempt to acquire distributed lock
        bool becameLeader = false;
        {
            DistributedLockGuard lockGuard(backend_, backendLockName_);
            if (!lockGuard.IsLocked()) {
                SM_LOG_ERROR("Failed to acquire distributed lock, will retry");
                backend_->UnInitialize();
                continue;
            }
            SM_LOG_INFO("Distributed lock acquired, double-checking leader");
            // Double-check after acquiring lock
            if (IsLeaderAlive(leaderAddr)) {
                SM_LOG_INFO("Leader appeared during lock acquisition: " << leaderAddr);
                if (BecomeFollower(leaderAddr) != SM_OK) {
                    SM_LOG_ERROR("Becoming follower failed after lock, leader: " << leaderAddr);
                    backend_->UnInitialize();
                    continue;
                }
                backend_->UnInitialize();
                SM_LOG_INFO("Election loop exiting: became follower of " << leaderAddr << " after lock");
                return;
            }
            SM_LOG_INFO("Proceeding to become leader");
            if (TryBecomeLeader() == SM_OK) {
                SM_LOG_INFO("Became leader after " << electionAttempt << " attempts");
                becameLeader = true;
            } else {
                SM_LOG_ERROR("TryBecomeLeader failed, lock will be released automatically");
            }
        }
        if (becameLeader) {
            // Keep long connection with backend as leader
            break;
        }
        backend_->UnInitialize();
    }
    SM_LOG_INFO("Election loop exiting: stop flag set after " << electionAttempt << " attempts");
}

// ============================================================================
// Server Management
// ============================================================================

void HaConfigStore::StartServer() noexcept
{
    SM_LOG_DEBUG("Starting AccStoreServer");
    std::unique_lock<std::shared_mutex> lock(delegateRwLock_);

    if (isLeader_.load(std::memory_order_acquire)) {
        SM_LOG_WARN("Already a leader, skipping server startup");
        return;
    }

    SM_LOG_INFO("ip: " << leaderBindIp_ << ", port: " << leaderBindPort_ << ", worldSize: " << worldSize_);

    // Recover world size from backend
    std::string worldSizeStr;
    uint32_t recoveredWorldSize = worldSize_;
    int backendRet = backend_->Get(KEY_WORLD_SIZE, worldSizeStr);
    bool recovered = false;
    if (backendRet == 0 && !worldSizeStr.empty()) {
        if (StrUtil::String2Uint(worldSizeStr, recoveredWorldSize)) {
            if (recoveredWorldSize == 0) {
                SM_LOG_WARN("Recovered worldSize is 0, using default: " << worldSize_);
            } else {
                SM_LOG_INFO("Recovered worldSize from backend: " << recoveredWorldSize);
                recovered = true;
            }
        } else {
            SM_LOG_WARN("Failed to parse worldSize: " << worldSizeStr);
        }
    }
    if (!recovered) {
        recoveredWorldSize = worldSize_;
        SM_LOG_DEBUG("No valid worldSize in backend, using default: " << worldSize_);
    }

    // Create and configure server
    serverDelegate_ = SmMakeRef<AccStoreServer>(leaderBindIp_, leaderBindPort_, recoveredWorldSize, backend_);
    SM_ASSERT_RET_VOID(serverDelegate_ != nullptr);
    SM_LOG_DEBUG("AccStoreServer created, ip: " << leaderBindIp_ << ", port: " << leaderBindPort_
                                                << ", worldSize: " << recoveredWorldSize);
    // Update status in backend to not-ready
    backendRet = serverDelegate_->UpdateStatus(false);
    SM_ASSERT_RET_VOID(backendRet == 0);
    SM_LOG_DEBUG("Backend status set to not-ready");
    // Restore metadata from backend
    SM_ASSERT_RET_VOID(serverDelegate_->RestoreFromBackend() == SM_OK);
    // Re-register cached handlers
    {
        std::unique_lock<std::mutex> stateLock(stateMutex_);
        if (cachedServerBrokenHandler_ != nullptr) {
            SM_LOG_DEBUG("Registering cached broken handler");
            serverDelegate_->RegisterBrokenLinkCHandler(cachedServerBrokenHandler_);
        }
        SM_LOG_DEBUG("Registering " << cachedServerOpHandlers_.size() << " cached op handlers");
        for (const auto &[opCode, handler] : cachedServerOpHandlers_) {
            serverDelegate_->RegisterOpHandler(opCode, handler);
        }
    }

    // Validate server address
    const std::string serverEndpoint = NetworkEndpointUtil::BuildEndpoint("tcp", leaderBindIp_, leaderBindPort_);
    SM_ASSERT_RET_VOID(!serverEndpoint.empty());
    auto parser = SocketAddressParserMgr::getInstance().CreateParser(serverEndpoint);
    SM_ASSERT_RET_VOID(parser);
    // Start the server
    Result startRet = serverDelegate_->Startup(tlsConfig_);
    if (startRet != SM_OK) {
        SM_LOG_ERROR("Startup failed at " << leaderBindIp_ << ":" << leaderBindPort_ << ", ret: " << startRet);
        serverDelegate_ = nullptr;
        return;
    }
    isLeader_.store(true, std::memory_order_release);
    SM_LOG_DEBUG("AccStoreServer started successfully");
}

void HaConfigStore::StopServer() noexcept
{
    SM_LOG_DEBUG("StopServer called");
    std::unique_lock<std::shared_mutex> lock(delegateRwLock_);
    if (!isLeader_.load(std::memory_order_acquire)) {
        SM_LOG_DEBUG("Not a leader, skip");
        return;
    }
    SM_LOG_DEBUG("Stopping AccStoreServer");
    if (serverDelegate_ != nullptr) {
        SM_LOG_DEBUG("Shutting down serverDelegate_");
        serverDelegate_->Shutdown();
        serverDelegate_ = nullptr;
    }
    isLeader_.store(false, std::memory_order_release);
    SM_LOG_DEBUG("AccStoreServer stopped");
}

// ============================================================================
// Re-election
// ============================================================================
void HaConfigStore::ReElectionThreadFunc()
{
    SM_LOG_INFO("Re-election thread started");
    if (!stopFlag_.load(std::memory_order_acquire)) {
        RunElectionLoop();
    }
    reElectionInProgress_.store(false, std::memory_order_release);
    SM_LOG_INFO("Re-election thread finished");
}

void HaConfigStore::TriggerReElectionAsync() noexcept
{
    if (stopFlag_.load(std::memory_order_acquire)) {
        SM_LOG_WARN("Stop flag set, skipping re-election");
        return;
    }

    bool expected = false;
    if (!reElectionInProgress_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        SM_LOG_INFO("Re-election already in progress, skip duplicate trigger");
        return;
    }

    SM_LOG_INFO("Triggering async re-election");

    std::lock_guard<std::mutex> lock(reElectionThreadMutex_);
    // Move previous thread out so the new thread can join it asynchronously
    std::thread prev = std::move(reElectionThread_);

    reElectionThread_ = std::thread([this, prev = std::move(prev)]() mutable {
        if (prev.joinable()) {
            prev.join();
        }
        ReElectionThreadFunc();
    });
}

// ============================================================================
// Client Connection
// ============================================================================

Result HaConfigStore::ConnectClient(const std::string &ip, uint16_t port) noexcept
{
    SM_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    SM_LOG_INFO("Target: " << ip << ":" << port);

    auto parser =
        SocketAddressParserMgr::getInstance().CreateParser(NetworkEndpointUtil::BuildEndpoint("tcp", ip, port));
    if (!parser) {
        SM_LOG_ERROR("Failed to create address parser for " << ip << ":" << port);
        return SM_ERROR;
    }

    // Check if already started
    if (clientDelegate_->SetServerInfo(ip, port)) {
        SM_LOG_INFO("Reconnecting to: " << ip << ":" << port);
        Result reconnectRet = clientDelegate_->ReConnectAfterBroken(-1);
        if (reconnectRet != SM_OK) {
            SM_LOG_ERROR("ReConnectAfterBroken failed, ret: " << reconnectRet);
        } else {
            SM_LOG_INFO("Reconnection initiated successfully");
        }
        return reconnectRet;
    }
    // First time connection
    SM_LOG_INFO("First time connection to: " << ip << ":" << port);
    Result clientStartRet = clientDelegate_->ClientStart(tlsConfig_);
    if (clientStartRet != SM_OK) {
        SM_LOG_ERROR("ClientStart failed, ret: " << clientStartRet);
        return clientStartRet;
    }
    SM_LOG_INFO("ClientStart succeeded");
    // Register broken link handler only once
    if (!brokenHandlerRegistered_.exchange(true, std::memory_order_acq_rel)) {
        SM_LOG_DEBUG("Registering broken link handler");
        clientDelegate_->RegisterClientBrokenHandler([this]() -> int {
            SM_LOG_INFO("Connection broken, triggering async re-election");
            TriggerReElectionAsync();
            return 0;
        });
    }
    return SM_OK;
}

Result HaConfigStore::BecomeFollower(const std::string &leaderIpPort) noexcept
{
    SM_LOG_INFO("Leader: " << leaderIpPort);
    std::string fullUrl = "tcp://" + leaderIpPort;
    std::string ip;
    uint16_t port = 0;
    if (!NetworkEndpointUtil::ExtractIpAndPort(fullUrl, ip, port)) {
        SM_LOG_ERROR("Invalid leader address format: " << leaderIpPort);
        return SM_ERROR;
    }

    SM_LOG_INFO("Connecting to leader, ip: " << ip << ", port: " << port);
    auto connectRet = ConnectClient(ip, port);
    SM_ASSERT_RETURN(connectRet == SM_OK, connectRet);
    SM_LOG_INFO("Connection initiated to leader");
    return SM_OK;
}

// ============================================================================
// Health Check
// ============================================================================

void HaConfigStore::HealthCheckThreadFunc() noexcept
{
    constexpr auto CHECK_INTERVAL = std::chrono::seconds(HEALTH_CHECK_INTERVAL_SEC);
    pthread_setname_np(pthread_self(), "ha_health_chk");
    SM_LOG_INFO("Health check thread started, interval: " << HEALTH_CHECK_INTERVAL_SEC << "s");
    while (!stopFlag_.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(CHECK_INTERVAL);
        if (stopFlag_.load(std::memory_order_acquire)) {
            break;
        }
        // Only check if current node is leader
        if (!isLeader_.load(std::memory_order_acquire)) {
            continue;
        }
        SM_LOG_DEBUG("Performing backend connection check");

        std::string testValue;
        int backendRet;
        {
            std::lock_guard<std::mutex> bLock(backendMutex_);
            backendRet = backend_->Get(KEY_LEADER, testValue);
        }
        if (backendRet != SUCCESS) {
            SM_LOG_ERROR("Backend connection timeout, ret: " << backendRet << ", triggering re-election");
            StopServer();
            TriggerReElectionAsync();
        } else {
            SM_LOG_DEBUG("Backend connection healthy");
        }
    }
    SM_LOG_INFO("Health check thread exiting");
    healthCheckRunning_.store(false, std::memory_order_release);
}

void HaConfigStore::StartHealthCheckThread() noexcept
{
    if (healthCheckRunning_.exchange(true, std::memory_order_acq_rel)) {
        SM_LOG_WARN("Health check thread already running");
        return;
    }
    SM_LOG_INFO("Starting health check thread");
    healthCheckThread_ = std::thread(&HaConfigStore::HealthCheckThreadFunc, this);
}

// ============================================================================
// ConfigStore Interface Forwarding (moved from header)
// ============================================================================

Result HaConfigStore::Set(const std::string &key, const std::vector<uint8_t> &value) noexcept
{
    SM_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Set(key, value);
}

Result HaConfigStore::Add(const std::string &key, int64_t increment, int64_t &value) noexcept
{
    SM_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Add(key, increment, value);
}

Result HaConfigStore::Remove(const std::string &key, bool printKeyNotExist) noexcept
{
    SM_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Remove(key, printKeyNotExist);
}

Result HaConfigStore::Append(const std::string &key, const std::vector<uint8_t> &value, uint64_t &newSize) noexcept
{
    SM_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Append(key, value, newSize);
}

Result HaConfigStore::Cas(const std::string &key, const std::vector<uint8_t> &expect, const std::vector<uint8_t> &value,
                          std::vector<uint8_t> &exists) noexcept
{
    SM_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Cas(key, expect, value, exists);
}

Result
HaConfigStore::Watch(const std::string &key,
                     const std::function<void(int result, const std::string &, const std::vector<uint8_t> &)> &notify,
                     uint32_t &wid) noexcept
{
    SM_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Watch(key, notify, wid);
}

Result HaConfigStore::Watch(WatchRankType type, const std::function<void(WatchRankType, uint32_t)> &notify,
                            uint32_t &wid) noexcept
{
    SM_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Watch(type, notify, wid);
}

Result HaConfigStore::Unwatch(uint32_t wid) noexcept
{
    SM_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Unwatch(wid);
}

Result HaConfigStore::Write(const std::string &key, const std::vector<uint8_t> &value, uint32_t offset) noexcept
{
    SM_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Write(key, value, offset);
}

void HaConfigStore::SetRankId(const int32_t &rankId) noexcept
{
    SM_ASSERT_RET_VOID(clientDelegate_ != nullptr);
    clientDelegate_->SetRankId(rankId);
}

std::string HaConfigStore::GetCompleteKey(const std::string &key) noexcept
{
    SM_ASSERT_RETURN(clientDelegate_ != nullptr, "");
    return clientDelegate_->GetCompleteKey(key);
}

std::string HaConfigStore::GetCommonPrefix() noexcept
{
    SM_ASSERT_RETURN(clientDelegate_ != nullptr, "");
    return clientDelegate_->GetCommonPrefix();
}

StorePtr HaConfigStore::GetCoreStore() noexcept
{
    return this;
}

void HaConfigStore::RegisterReconnectHandler(ConfigStoreReconnectHandler callback) noexcept
{
    SM_ASSERT_RET_VOID(clientDelegate_ != nullptr);
    clientDelegate_->RegisterReconnectHandler(callback);
}

Result HaConfigStore::ReConnectAfterBroken(int reconnectRetryTimes) noexcept
{
    // Note: This function triggers async re-election rather than synchronous reconnection.
    // The reconnectRetryTimes parameter is not used as re-election is async.
    (void)reconnectRetryTimes;
    SM_LOG_INFO("ReConnectAfterBroken: triggering async re-election");
    TriggerReElectionAsync();
    SM_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return SM_OK;
}

bool HaConfigStore::GetConnectStatus() noexcept
{
    SM_ASSERT_RETURN(clientDelegate_ != nullptr, false);
    return clientDelegate_->GetConnectStatus();
}

void HaConfigStore::SetConnectStatus(bool status) noexcept
{
    SM_ASSERT_RET_VOID(clientDelegate_ != nullptr);
    clientDelegate_->SetConnectStatus(status);
}

void HaConfigStore::RegisterClientBrokenHandler(const ConfigStoreClientBrokenHandler &handler) noexcept
{
    SM_ASSERT_RET_VOID(clientDelegate_ != nullptr);
    clientDelegate_->RegisterClientBrokenHandler(handler);
}

void HaConfigStore::RegisterServerBrokenHandler(const ConfigStoreServerBrokenHandler &handler) noexcept
{
    std::unique_lock<std::mutex> lock(stateMutex_);
    cachedServerBrokenHandler_ = handler;
    lock.unlock();

    std::shared_lock<std::shared_mutex> rwLock(delegateRwLock_);
    if (serverDelegate_ == nullptr) {
        SM_LOG_DEBUG("ServerDelegate is null, handler cached for future leader promotion");
        return;
    }
    serverDelegate_->RegisterBrokenLinkCHandler(handler);
}

void HaConfigStore::RegisterServerOpHandler(int16_t opCode, const ConfigStoreServerOpHandler &handler) noexcept
{
    std::unique_lock<std::mutex> lock(stateMutex_);
    cachedServerOpHandlers_[opCode] = handler;
    lock.unlock();

    std::shared_lock<std::shared_mutex> rwLock(delegateRwLock_);
    if (serverDelegate_ == nullptr) {
        SM_LOG_DEBUG("ServerDelegate is null, opCode: " << opCode << " handler cached");
        return;
    }
    serverDelegate_->RegisterOpHandler(opCode, handler);
}

Result HaConfigStore::GetReal(const std::string &key, std::vector<uint8_t> &value, int64_t timeoutMs) noexcept
{
    SM_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Get(key, value, timeoutMs);
}

} // namespace smem
} // namespace ock
