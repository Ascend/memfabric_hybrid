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

#ifndef SMEM_CONFIG_STORE_BACKEND_H
#define SMEM_CONFIG_STORE_BACKEND_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "smem_ref.h"
#include "config_store_errno.h"
#include "smem_types.h"

namespace ock {
namespace smem {

/**
 * @brief Abstract interface for configuration store backend implementations
 *
 * Provides unified CRUD operations, distributed locking, and capability queries
 * for different storage backends (e.g., local memory, etcd, redis).
 *
 * @note Not thread-safe. Caller must ensure external synchronization if needed.
 */
class ConfigStoreBackend : public SmReferable {
public:
    ~ConfigStoreBackend() noexcept override = default;

    /**
     * @brief Get backend implementation name
     *
     * @return Backend type identifier (e.g., "LocalMemory", "Etcd")
     */
    [[nodiscard]] virtual std::string BackendName() const noexcept = 0;

    // -------------------------------------------------------------------------
    // Basic CRUD operations
    // -------------------------------------------------------------------------

    /**
     * @brief Retrieve value associated with key
     *
     * @param key       [in]  Key to look up
     * @param outValue  [out] Retrieved value if found
     * @return SUCCESS if key exists, NOT_EXIST otherwise
     */
    [[nodiscard]] virtual StoreErrorCode Get(const std::string &key, std::vector<uint8_t> &outValue) const noexcept = 0;

    /**
     * @brief Retrieve value associated with key (string variant)
     *
     * @param key       [in]  Key to look up
     * @param outValue  [out] Retrieved value as string if found
     * @return SUCCESS if key exists, NOT_EXIST otherwise
    */
    [[nodiscard]] StoreErrorCode Get(const std::string &key, std::string &outValue) const noexcept
    {
        std::vector<uint8_t> buffer;
        StoreErrorCode result = Get(key, buffer);
        if (result == StoreErrorCode::SUCCESS) {
            outValue.assign(buffer.begin(), buffer.end());
        }
        return result;
    }

    /**
     * @brief Store key-value pair with optional time-to-live
     *
     * @param key         [in] Key to store
     * @param value       [in] Value to store
     * @param ttlSeconds  [in] Time-to-live in seconds (0 = no expiration)
     * @return SUCCESS on success, ERROR on failure
     * @note Not all backends support TTL. Check SupportsTTL() before using non-zero ttlSeconds.
     */
    [[nodiscard]] virtual StoreErrorCode Put(const std::string &key, const std::vector<uint8_t> &value,
                                             int64_t ttlSeconds) noexcept = 0;

    [[nodiscard]] StoreErrorCode Put(const std::string &key, const std::string &value, int64_t ttlSeconds) noexcept
    {
        const std::vector<uint8_t> data(value.begin(), value.end());
        return Put(key, data, ttlSeconds);
    }
    /**
     * @brief Delete key-value pair
     *
     * @param key  [in] Key to delete
     * @return SUCCESS if deleted, NOT_EXIST if key doesn't exist
     */
    [[nodiscard]] virtual StoreErrorCode Delete(const std::string &key) noexcept = 0;

    /**
     * @brief Check if key exists in store
     *
     * @param key  [in] Key to check
     * @return SUCCESS if exists, NOT_EXIST otherwise
     */
    [[nodiscard]] virtual StoreErrorCode Exist(const std::string &key) const noexcept = 0;

    /**
     * @brief Remove all data from backend
     */
    virtual void Clear() noexcept = 0;

    // -------------------------------------------------------------------------
    // Capability queries
    // -------------------------------------------------------------------------

    /**
     * @brief Check if backend is distributed
     *
     * @return true if backend supports distributed lock operations, false otherwise
     */
    [[nodiscard]] virtual bool IsDistributed() const noexcept = 0;

    /**
     * @brief Check if backend supports TTL for Put operations
     *
     * @return true if TTL is supported, false otherwise
     */
    [[nodiscard]] virtual bool SupportsTTL() const noexcept = 0;

    // -------------------------------------------------------------------------
    // Distributed Lock
    // Note: These are logical locks for distributed coordination, not mutex-style locks.
    // For local backends, these track lock names without providing actual mutual exclusion.
    // -------------------------------------------------------------------------

    /**
     * @brief Acquire a distributed lock
     *
     * @param name  [in] Lock name/identifier
     * @return SUCCESS if acquired, ERROR if already locked or operation failed
     */
    [[nodiscard]] virtual StoreErrorCode AcquireDistributedLock(const std::string &name) noexcept = 0;

    /**
     * @brief Release a distributed lock
     *
     * @param name  [in] Lock name/identifier
     * @return SUCCESS if released, NOT_EXIST if lock doesn't exist
     */
    virtual StoreErrorCode ReleaseDistributedLock(const std::string &name) noexcept = 0;

    /**
     * @brief Attempt to acquire distributed lock with timeout
     *
     * @param name       [in] Lock name/identifier
     * @param timeoutMs  [in] Maximum wait time in milliseconds (0 = non-blocking)
     * @return SUCCESS if acquired, TIMEOUT if not acquired within timeout, ERROR on other failures
     * @note Not all backends support timeout. Some may fall back to immediate attempt.
     */
    [[nodiscard]] virtual StoreErrorCode TryAcquireDistributedLock(const std::string &name,
                                                                   int64_t timeoutMs) noexcept = 0;

    [[nodiscard]] virtual StoreErrorCode Initialize(const std::string &backendUrl, const std::string &userName,
                                                    const std::string &password) = 0;

    virtual void UnInitialize() = 0;
};

using StoreBackendPtr = SmRef<ConfigStoreBackend>;

/**
 * @brief RAII-style guard for distributed locks
 *
 * Automatically acquires lock on construction and releases on destruction.
 * Prevents lock leaks in exception-safe manner.
 *
 * @note Move-only type (non-copyable to prevent accidental double-release)
 */
class DistributedLockGuard {
public:
    /**
     * @brief Acquire lock immediately (blocking)
     *
     * @param backend  Backend instance providing lock operations
     * @param name     Lock identifier
     * @note isLocked_ is false if lock acquisition fails
     */
    explicit DistributedLockGuard(StoreBackendPtr backend, std::string name) noexcept
        : backend_(std::move(backend)), lockName_(std::move(name)), isLocked_(false)
    {
        isLocked_ = false;
        if (backend_ == nullptr) {
            return;
        }

        StoreErrorCode result = backend_->AcquireDistributedLock(lockName_);
        if (result != StoreErrorCode::SUCCESS) {
            return;
        }
        isLocked_ = true;
    }

    /**
     * @brief Attempt to acquire lock with timeout
     *
     * @param backend    Backend instance providing lock operations
     * @param name       Lock identifier
     * @param timeout    Maximum wait duration
     * @param outResult  [out] Actual acquisition result code
     * @note Does NOT throw on timeout. Check outResult == SUCCESS to verify lock ownership.
     */
    DistributedLockGuard(StoreBackendPtr backend, std::string name, std::chrono::milliseconds timeout,
                         StoreErrorCode &outResult) noexcept
        : backend_(std::move(backend)), lockName_(std::move(name)), isLocked_(false)
    {
        if (backend_ == nullptr) {
            outResult = StoreErrorCode::ERROR;
            return;
        }

        outResult = backend_->TryAcquireDistributedLock(lockName_, timeout.count());
        if (outResult == StoreErrorCode::SUCCESS) {
            isLocked_ = true;
        }
    }

    ~DistributedLockGuard() noexcept
    {
        if (isLocked_ && backend_ != nullptr) {
            (void)backend_->ReleaseDistributedLock(lockName_);
        }
    }

    // Disable copy to prevent double-release
    DistributedLockGuard(const DistributedLockGuard &) = delete;
    DistributedLockGuard &operator=(const DistributedLockGuard &) = delete;

    // Enable move
    DistributedLockGuard(DistributedLockGuard &&other) noexcept
        : backend_(std::move(other.backend_)), lockName_(std::move(other.lockName_)), isLocked_(other.isLocked_)
    {
        other.isLocked_ = false;
    }

    DistributedLockGuard &operator=(DistributedLockGuard &&other) noexcept
    {
        if (this != &other) {
            // Release current lock if held
            if (isLocked_ && backend_ != nullptr) {
                backend_->ReleaseDistributedLock(lockName_);
            }

            backend_ = std::move(other.backend_);
            lockName_ = std::move(other.lockName_);
            isLocked_ = other.isLocked_;
            other.isLocked_ = false;
        }
        return *this;
    }

    /**
     * @brief Check if lock is currently held
     *
     * @return true if lock was successfully acquired and not yet released
     */
    [[nodiscard]] bool IsLocked() const noexcept
    {
        return isLocked_;
    }

    /**
     * @brief Manually release lock before destruction
     *
     * @note Safe to call multiple times (idempotent)
     */
    void Unlock() noexcept
    {
        if (isLocked_ && backend_ != nullptr) {
            backend_->ReleaseDistributedLock(lockName_);
            isLocked_ = false;
        }
    }

private:
    StoreBackendPtr backend_;
    std::string lockName_;
    bool isLocked_;
};

} // namespace smem
} // namespace ock

#endif