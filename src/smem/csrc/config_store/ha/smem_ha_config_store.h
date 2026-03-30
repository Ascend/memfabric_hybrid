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
#ifndef SMEM_SMEM_HA_CONFIG_STORE_H
#define SMEM_SMEM_HA_CONFIG_STORE_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "smem_config_store_logger.h"
#include "smem_config_store.h"
#include "smem_logger.h"
#include "smem_tcp_config_store.h"
#include "smem_types.h"

namespace ock {
namespace smem {

constexpr uint32_t MAX_RETRY_COUNT = 10;
constexpr uint32_t RETRY_INTERVAL_MS = 500;

// Network connectivity check timeout
constexpr int32_t CONNECTION_TIMEOUT_SEC = 3;

// Random sleep range for leader election backoff
constexpr int32_t MIN_SLEEP_MS = 100;
constexpr int32_t MAX_SLEEP_MS = 1000;

// Backend lease TTL
constexpr int32_t PUT_LEASE_TTL_SEC = 5;

// Health check interval
constexpr uint32_t HEALTH_CHECK_INTERVAL_SEC = 4;

/**
 * @brief HaConfigStore manages leader election via backend and delegates operations
 * to TCP client.
 */
class HaConfigStore : public ConfigStoreManager {
public:
    HaConfigStore(StoreBackendPtr backend, TcpConfigStorePtr clientDelegate, const std::string &endpoints,
                  uint32_t worldSize, std::string instanceId = "");
    ~HaConfigStore() override;

    HaConfigStore(const HaConfigStore &) = delete;
    HaConfigStore &operator=(const HaConfigStore &) = delete;

    /**
     * @brief Initialize the hybrid store and start the election state machine.
     */
    [[nodiscard]] Result Startup(const smem_tls_config &tlsConfig) noexcept;

    // --- ConfigStore Interface Implementation (Forwarding to clientDelegate_) ---
    Result PrefixGet(const std::string &key, std::unordered_map<std::string, std::string> &value) noexcept override;
    [[nodiscard]] Result Set(const std::string &key, const std::vector<uint8_t> &value) noexcept override;
    [[nodiscard]] Result Add(const std::string &key, int64_t increment, int64_t &value) noexcept override;
    [[nodiscard]] Result Remove(const std::string &key, bool printKeyNotExist) noexcept override;
    [[nodiscard]] Result Append(const std::string &key, const std::vector<uint8_t> &value,
                                uint64_t &newSize) noexcept override;
    [[nodiscard]] Result Cas(const std::string &key, const std::vector<uint8_t> &expect,
                             const std::vector<uint8_t> &value, std::vector<uint8_t> &exists) noexcept override;
    [[nodiscard]] Result
    Watch(const std::string &key,
          const std::function<void(int result, const std::string &, const std::vector<uint8_t> &)> &notify,
          uint32_t &wid) noexcept override;
    [[nodiscard]] Result Watch(WatchRankType type, const std::function<void(WatchRankType, uint32_t)> &notify,
                               uint32_t &wid) noexcept override;
    [[nodiscard]] Result Unwatch(uint32_t wid) noexcept override;
    [[nodiscard]] Result Write(const std::string &key, const std::vector<uint8_t> &value,
                               uint32_t offset) noexcept override;
    [[nodiscard]] Result QueryAlive(uint32_t rank, uint32_t &alive) noexcept override;
    void SetRankId(const int32_t &rankId) noexcept override;
    std::string GetCompleteKey(const std::string &key) noexcept override;
    std::string GetCommonPrefix() noexcept override;
    StorePtr GetCoreStore() noexcept override;

    // --- Connection and Lifecycle Management ---
    void RegisterReconnectHandler(ConfigStoreReconnectHandler callback) noexcept override;
    [[nodiscard]] Result ReConnectAfterBroken(int reconnectRetryTimes) noexcept override;
    [[nodiscard]] bool GetConnectStatus() noexcept override;
    void SetConnectStatus(bool status) noexcept override;
    void RegisterClientBrokenHandler(const ConfigStoreClientBrokenHandler &handler) noexcept override;
    void RegisterServerBrokenHandler(const ConfigStoreServerBrokenHandler &handler) noexcept override;
    void RegisterServerOpHandler(int16_t opCode, const ConfigStoreServerOpHandler &handler) noexcept override;

protected:
    [[nodiscard]] Result GetReal(const std::string &key, std::vector<uint8_t> &value,
                                 int64_t timeoutMs) noexcept override;

private:
    // Internal State Machine Logic
    void RandomBackoff() noexcept;
    void RunElectionLoop() noexcept;
    Result BecomeFollower(const std::string &leaderIpPort) noexcept;
    void StartServer() noexcept;
    void StopServer() noexcept;
    Result ConnectClient(const std::string &ip, uint16_t port) noexcept;
    void HealthCheckThreadFunc() noexcept;
    void Uninitialize() noexcept;

    // Helper methods
    [[nodiscard]] bool InitBackendConnection() noexcept;
    [[nodiscard]] Result TryBecomeLeader() noexcept;
    [[nodiscard]] bool IsLeaderAlive(std::string &leaderAddr) noexcept;
    void StartHealthCheckThread() noexcept;
    void TriggerReElectionAsync() noexcept;
    void ReElectionThreadFunc();

private:
    // Configuration Attributes
    const std::string endpoints_;
    const uint32_t worldSize_;
    const std::string backendLockName_;
    std::string leaderBindIp_;    // Only meaningful on leader node
    uint16_t leaderBindPort_ = 0; // Only meaningful on leader node
    bool isFirstLeader_ = true;
    smem_tls_config tlsConfig_{};

    mutable std::shared_mutex delegateRwLock_;
    StoreBackendPtr backend_;
    std::mutex backendMutex_;
    TcpConfigStorePtr clientDelegate_{nullptr};
    AccStoreServerPtr serverDelegate_{nullptr}; // Only valid on leader node

    std::mutex stateMutex_;
    std::atomic<bool> isLeader_{false};
    std::atomic<bool> brokenHandlerRegistered_{false};

    // Lifecycle flags
    std::atomic<bool> stopFlag_{false};
    std::atomic<bool> reElectionInProgress_{false};
    std::mutex reElectionThreadMutex_;
    std::thread reElectionThread_;

    ConfigStoreServerBrokenHandler cachedServerBrokenHandler_{nullptr};
    std::unordered_map<int16_t, ConfigStoreServerOpHandler> cachedServerOpHandlers_;

    std::atomic<bool> healthCheckRunning_{false};
    std::thread healthCheckThread_;
};

using HaConfigStorePtr = SmRef<HaConfigStore>;

} // namespace smem
} // namespace ock

#endif
