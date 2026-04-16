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

#ifndef SMEM_ETCD_CLIENT_V3
#define SMEM_ETCD_CLIENT_V3

#include <string>
#include <string_view>
#include <cstdint>
#include <atomic>
#include <mutex>
#include "dl_etcd_api.h"
#include "smem_logger.h"

namespace ock::smem {

class EtcdClientV3 {
public:
    static EtcdClientV3 &GetInstance() noexcept
    {
        static EtcdClientV3 instance;
        return instance;
    }

    ~EtcdClientV3() noexcept
    {
        Close();
    }

    EtcdClientV3(const EtcdClientV3 &) = delete;
    EtcdClientV3 &operator=(const EtcdClientV3 &) = delete;
    EtcdClientV3(EtcdClientV3 &&) = delete;
    EtcdClientV3 &operator=(EtcdClientV3 &&) = delete;

    /**
     * @brief Initialize connection to Etcd cluster
     * Thread-safe: can be called multiple times, only first successful call takes effect
     * @return 0 on success, -1 on failure
     */
    [[nodiscard]] int32_t Initialize(const char *endpoints, const char *username = nullptr,
                                     const char *password = nullptr, int32_t timeoutSeconds = 5) noexcept
    {
        // Fast path: already initialized
        if (client_.load(std::memory_order_acquire) != nullptr) {
            // Warn if parameters differ from initial configuration
            if (endpoints != nullptr && endpoints_ != endpoints) {
                SM_LOG_WARN("Client already initialized with different endpoints. "
                            << "Current: " << endpoints_ << ", Requested: " << endpoints
                            << ". Using existing connection.");
            } else {
                SM_LOG_INFO("Client already initialized, skipping re-initialization");
            }
            return 0;
        }

        std::lock_guard<std::mutex> lock(initMutex_);

        // Double-check after acquiring lock
        if (client_.load(std::memory_order_relaxed) != nullptr) {
            // Warn if parameters differ from initial configuration
            if (endpoints != nullptr && endpoints_ != endpoints) {
                SM_LOG_WARN("Client already initialized after lock with different endpoints. "
                            << "Current: " << endpoints_ << ", Requested: " << endpoints
                            << ". Using existing connection.");
            } else {
                SM_LOG_INFO("Client already initialized after lock acquisition, skipping");
            }
            return 0;
        }

        SM_LOG_INFO("Initializing etcd client, endpoints: " << endpoints << ", timeoutSeconds: " << timeoutSeconds);

        EtcdClient *rawClient = EtcdApi::EtcdNew(endpoints, username, password, timeoutSeconds);
        if (rawClient == nullptr) {
            SM_LOG_ERROR("Failed to create etcd client, endpoints: " << endpoints
                                                                     << ", timeoutSeconds: " << timeoutSeconds);
            return -1;
        }

        // Store initial endpoints for comparison on re-initialization attempts
        if (endpoints != nullptr) {
            endpoints_ = endpoints;
        }
        client_.store(rawClient, std::memory_order_release);
        SM_LOG_INFO("Etcd client initialized successfully, endpoints: " << endpoints);
        return 0;
    }

    [[nodiscard]] bool IsInitialized() const noexcept
    {
        return client_.load(std::memory_order_acquire) != nullptr;
    }

    void Close() noexcept
    {
        EtcdClient *expected = client_.load(std::memory_order_acquire);
        if (expected != nullptr) {
            // Atomic exchange prevents double-close race condition
            if (client_.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel)) {
                smem::EtcdApi::EtcdClose(expected);
                SM_LOG_INFO("Etcd client closed successfully");
            }
        }
    }

    /**
     * @brief Set key-value pair
     * @param ttlSeconds Time-to-live in seconds (0 = no expiration)
     * @return 0 on success, -1 on failure
     */
    [[nodiscard]] int32_t SetValue(std::string_view key, std::string_view value, int64_t ttlSeconds = 0) noexcept
    {
        EtcdClient *cli = client_.load(std::memory_order_relaxed);
        if (cli == nullptr) {
            SM_LOG_ERROR("SetValue failed: client not initialized, key: " << key);
            return -1;
        }
        int32_t ret = EtcdApi::EtcdPut(cli, key.data(), value.data(), value.size(), ttlSeconds);
        if (ret != 0) {
            SM_LOG_ERROR("SetValue failed: EtcdPut returned "
                         << ret << ", key: " << key << ", valueSize: " << value.size() << ", ttlSeconds: " << ttlSeconds
                         << ", error: " << smem::EtcdApi::EtcdGetLastError(cli));
        }
        return ret;
    }

    /**
     * @brief Get value by key
     * @return 0 on success, -1 on failure or key not found
     */
    [[nodiscard]] int32_t GetValue(std::string_view key, std::string &outValue) noexcept
    {
        EtcdClient *cli = client_.load(std::memory_order_relaxed);
        if (cli == nullptr) {
            SM_LOG_ERROR("GetValue failed: client not initialized, key: " << key);
            return -1;
        }
        char *rawBuf = nullptr;
        size_t rawLen = 0;
        int32_t ret = EtcdApi::EtcdGet(cli, key.data(), &rawBuf, &rawLen);
        if (ret != 0) {
            SM_LOG_WARN("GetValue failed: EtcdGet returned " << ret << ", key: " << key
                                                              << ", error: " << smem::EtcdApi::EtcdGetLastError(cli));
            return -1;
        }
        // Handle empty value case (valid scenario in etcd)
        if (rawBuf != nullptr) {
            outValue.assign(rawBuf, rawLen);
            EtcdApi::EtcdFreeValue(rawBuf);
        } else {
            outValue.clear();
        }
        return 0;
    }

    /**
     * @brief Get key-values by prefix and marker(range begin)
     * @return 0 on success, -1 on failure or key not found
     */
    [[nodiscard]] int32_t PrefixGet(const smem_store_prefix_get_ctx_t *ctx, int flags) noexcept
    {
        EtcdClient *cli = client_.load(std::memory_order_relaxed);
        if (cli == nullptr) {
            SM_LOG_ERROR("PrefixGet failed: client not initialized, prefix: " << ctx->prefix);
            return -1;
        }
        int32_t ret = EtcdApi::EtcdPrefixGet(cli, ctx, flags);
        if (ret != 0) {
            SM_LOG_WARN("PrefixGet failed: EtcdGet returned " << ret << ", prefix: " << ctx->prefix
                                                            << ", marker:" << ctx->marker
                                                            << ", error: " << smem::EtcdApi::EtcdGetLastError(cli));
            return -1;
        }
        return 0;
    }

    [[nodiscard]] int32_t DeleteKey(std::string_view key) noexcept
    {
        EtcdClient *cli = client_.load(std::memory_order_relaxed);
        if (cli == nullptr) {
            SM_LOG_ERROR("DeleteKey failed: client not initialized, key: " << key);
            return -1;
        }
        int32_t ret = smem::EtcdApi::EtcdRemove(cli, key.data());
        if (ret != 0) {
            SM_LOG_ERROR("DeleteKey failed: EtcdRemove returned "
                         << ret << ", key: " << key << ", error: " << smem::EtcdApi::EtcdGetLastError(cli));
        }
        return ret;
    }

    /**
     * @brief Get last error message from underlying client
     */
    [[nodiscard]] const char *GetLastError() const noexcept
    {
        EtcdClient *cli = client_.load(std::memory_order_relaxed);
        return (cli != nullptr) ? smem::EtcdApi::EtcdGetLastError(cli) : "Client not initialized";
    }

    /**
     * @brief Acquire distributed lock (for internal use by EtcdLockGuard)
     */
    [[nodiscard]] int32_t RawLock() noexcept
    {
        EtcdClient *cli = client_.load(std::memory_order_relaxed);
        if (cli == nullptr) {
            SM_LOG_ERROR("RawLock failed: client not initialized");
            return -1;
        }
        int32_t ret = smem::EtcdApi::EtcdLock(cli);
        if (ret != 0) {
            SM_LOG_ERROR("RawLock failed: EtcdLock returned " << ret
                                                              << ", error: " << smem::EtcdApi::EtcdGetLastError(cli));
        }
        return ret;
    }

    [[nodiscard]] int32_t RawLockNamed(const std::string &lockName) noexcept
    {
        EtcdClient *cli = client_.load(std::memory_order_relaxed);
        if (cli == nullptr) {
            SM_LOG_ERROR("RawLockNamed failed: client not initialized");
            return -1;
        }
        if (lockName.empty()) {
            SM_LOG_ERROR("RawLockNamed failed: lockName is empty");
            return -1;
        }
        if (!smem::EtcdApi::SupportsNamedLock()) {
            SM_LOG_ERROR("RawLockNamed failed: named-lock symbol is not available in current etcd client library");
            return -1;
        }
        int32_t ret = smem::EtcdApi::EtcdLockNamed(cli, lockName.c_str());
        if (ret != 0) {
            SM_LOG_ERROR("RawLockNamed failed: EtcdLockNamed returned "
                         << ret << ", lockName: " << lockName << ", error: " << smem::EtcdApi::EtcdGetLastError(cli));
        }
        return ret;
    }

    /**
     * @brief Release distributed lock (for internal use by EtcdLockGuard)
     */
    [[nodiscard]] int32_t RawUnlock() noexcept
    {
        EtcdClient *cli = client_.load(std::memory_order_relaxed);
        if (cli == nullptr) {
            SM_LOG_ERROR("RawUnlock failed: client not initialized");
            return -1;
        }
        int32_t ret = smem::EtcdApi::EtcdUnLock(cli);
        if (ret != 0) {
            SM_LOG_ERROR("RawUnlock failed: EtcdUnLock returned "
                         << ret << ", error: " << smem::EtcdApi::EtcdGetLastError(cli));
        }
        return ret;
    }

private:
    EtcdClientV3() noexcept : client_(nullptr) {}

    std::atomic<EtcdClient *> client_;
    std::mutex initMutex_;
    std::string endpoints_;  // Store initial endpoints for comparison
};

class EtcdLockGuard {
public:
    explicit EtcdLockGuard(EtcdClientV3 &client) noexcept : client_(client), isLocked_(false)
    {
        if (client_.RawLock() == 0) {
            isLocked_ = true;
            SM_LOG_INFO("Distributed lock acquired successfully");
        } else {
            SM_LOG_ERROR("Failed to acquire distributed lock");
        }
    }

    ~EtcdLockGuard() noexcept
    {
        if (isLocked_) {
            if (client_.RawUnlock() != 0) {
                SM_LOG_ERROR("Failed to release distributed lock in EtcdLockGuard destructor");
            }
        }
    }

    [[nodiscard]] bool IsLocked() const noexcept
    {
        return isLocked_;
    }

    EtcdLockGuard(const EtcdLockGuard &) = delete;
    EtcdLockGuard &operator=(const EtcdLockGuard &) = delete;

private:
    EtcdClientV3 &client_;
    bool isLocked_;
};

} // namespace ock::smem

#endif // SMEM_ETCD_CLIENT_V3
