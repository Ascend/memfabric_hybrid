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

#include "smem_external_backend.h"

#include <exception>
#include <memory>
#include "smem.h"
#include "smem_config_store_logger.h"
#include "smem_external_backend_registry.h"

namespace ock {
namespace smem {
namespace {

constexpr const char *CONFIG_STORE_CLUSTER_ROOT = "/memfabric_hybrid/config_store/clusters/";
constexpr const char *EXTERNAL_BACKEND_NAME = "memfabric";
constexpr uint32_t BACKEND_FLAGS = 0;
constexpr size_t INITIAL_GET_BUFFER_SIZE = 1024U * 1024U;
constexpr uint32_t MAX_GET_BUFEX_RETRIES = 20U;
constexpr size_t EXIST_PROBE_BUFFER_SIZE = 1U;

std::string BuildClusterRoot(const std::string &instanceId)
{
    if (instanceId.empty()) {
        return CONFIG_STORE_CLUSTER_ROOT;
    }

    std::string clusterRoot = CONFIG_STORE_CLUSTER_ROOT;
    clusterRoot.append(instanceId);
    return clusterRoot;
}

} // namespace

SmemExternalBackend::SmemExternalBackend(const std::string &instanceId) noexcept
    : clusterRoot_(BuildClusterRoot(instanceId))
{}

SmemExternalBackend::~SmemExternalBackend() noexcept
{
    UnInitialize();
}

StoreErrorCode SmemExternalBackend::Initialize(const std::string &backendUrl, const std::string &userName,
                                               const std::string &password)
{
    (void)userName;
    (void)password;

    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        return StoreErrorCode::SUCCESS;
    }

    smem_conf_store_backend_op_t backendOp{};
    if (!SmemExternalBackendRegistry::GetExternalBackendOp(backendOp)) {
        STORE_LOG_ERROR("External backend op is not registered");
        return StoreErrorCode::ERROR;
    }

    void *handle = nullptr;
    const int32_t ret = backendOp.create(EXTERNAL_BACKEND_NAME, clusterRoot_.c_str(), BACKEND_FLAGS, &handle);
    if (ret != SMEM_STORE_BACKEND_CODE_OK || handle == nullptr) {
        STORE_LOG_ERROR("Failed to create external backend handle, url=" << backendUrl << ", ret=" << ret);
        return StoreErrorCode::ERROR;
    }

    backendOp_ = backendOp;
    handle_ = handle;
    initialized_ = true;
    return StoreErrorCode::SUCCESS;
}

void SmemExternalBackend::UnInitialize()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) {
        return;
    }

    if (backendOp_.destroy != nullptr && handle_ != nullptr) {
        backendOp_.destroy(handle_);
    }
    handle_ = nullptr;
    initialized_ = false;
    backendOp_ = {};
}

std::string SmemExternalBackend::BackendName() const noexcept
{
    return "External";
}

StoreErrorCode SmemExternalBackend::MapCommonResult(int32_t result) const noexcept
{
    switch (result) {
        case SMEM_STORE_BACKEND_CODE_OK:
            return StoreErrorCode::SUCCESS;
        case SMEM_STORE_BACKEND_CODE_NOENT:
        case SMEM_STORE_BACKEND_CODE_UNLOCKED:
            return StoreErrorCode::NOT_EXIST;
        case SMEM_STORE_BACKEND_CODE_LOCKED:
            return StoreErrorCode::TIMEOUT;
        case SMEM_STORE_BACKEND_CODE_INVAL:
            return StoreErrorCode::INVALID_MESSAGE;
        case SMEM_STORE_BACKEND_CODE_NORES:
            return StoreErrorCode::IO_ERROR;
        default:
            break;
    }

    if (result == SMEM_STORE_BACKEND_CODE_OK) {
        return StoreErrorCode::SUCCESS;
    }
    return StoreErrorCode::ERROR;
}

StoreErrorCode SmemExternalBackend::GetLocked(const std::string &key, std::vector<uint8_t> &outValue) const noexcept
{
    try {
        if (!initialized_ || handle_ == nullptr || backendOp_.get == nullptr) {
            STORE_LOG_ERROR("Get failed: backend not initialized");
            return StoreErrorCode::ERROR;
        }

        size_t bufferSize = INITIAL_GET_BUFFER_SIZE;
        auto buffer = std::make_unique<uint8_t[]>(bufferSize);
        uint64_t valueSize = 0;
        for (uint32_t retryCount = 0; retryCount <= MAX_GET_BUFEX_RETRIES; ++retryCount) {
            const int32_t ret =
                backendOp_.get(handle_, key.c_str(), buffer.get(), bufferSize, BACKEND_FLAGS, &valueSize);
            if (ret == SMEM_STORE_BACKEND_CODE_OK) {
                outValue.assign(buffer.get(), buffer.get() + static_cast<size_t>(valueSize));
                return StoreErrorCode::SUCCESS;
            }

            if (ret != SMEM_STORE_BACKEND_CODE_BUFEX || retryCount == MAX_GET_BUFEX_RETRIES) {
                return MapCommonResult(ret);
            }

            bufferSize *= 2U;
            buffer = std::make_unique<uint8_t[]>(bufferSize);
        }
        return StoreErrorCode::ERROR;
    } catch (const std::exception &e) {
        STORE_LOG_ERROR("Get failed: exception, key=" << key << ", what=" << e.what());
        return StoreErrorCode::ERROR;
    } catch (...) {
        STORE_LOG_ERROR("Get failed: unknown exception, key=" << key);
        return StoreErrorCode::ERROR;
    }
}

StoreErrorCode SmemExternalBackend::Get(const std::string &key, std::vector<uint8_t> &outValue) const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    return GetLocked(key, outValue);
}

StoreErrorCode SmemExternalBackend::PrefixGet(const std::string &key, PrefixGetMap &outValue) const noexcept
{
    // todo: external backend support prefix get
    for (uint32_t i = 0; i < SMEM_WORLD_SIZE_MAX; i++) {
        std::string k = key + std::to_string(i);
        std::vector<uint8_t> value;
        auto ret = GetLocked(k, value);
        if (ret == 0) {
            outValue[k] = value;
        }
    }
    return StoreErrorCode::SUCCESS;
}

StoreErrorCode SmemExternalBackend::Put(const std::string &key, const std::vector<uint8_t> &value,
                                        int64_t ttlSeconds) noexcept
{
    // external backend does not support TTL
    (void)ttlSeconds;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || handle_ == nullptr || backendOp_.put == nullptr) {
        STORE_LOG_ERROR("Put failed: backend not initialized");
        return StoreErrorCode::ERROR;
    }

    const void *data = value.empty() ? nullptr : value.data();
    return MapCommonResult(
        backendOp_.put(handle_, key.c_str(), data, static_cast<uint64_t>(value.size()), BACKEND_FLAGS));
}

StoreErrorCode SmemExternalBackend::Delete(const std::string &key) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || handle_ == nullptr || backendOp_.remove == nullptr) {
        STORE_LOG_ERROR("Delete failed: backend not initialized");
        return StoreErrorCode::ERROR;
    }
    const auto ret = backendOp_.remove(handle_, key.c_str(), BACKEND_FLAGS);
    if (ret == SMEM_STORE_BACKEND_CODE_NOENT) {
        return StoreErrorCode::SUCCESS;
    }
    return MapCommonResult(ret);
}

StoreErrorCode SmemExternalBackend::Exist(const std::string &key) const noexcept
{
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_ || handle_ == nullptr || backendOp_.get == nullptr) {
            STORE_LOG_ERROR("Exist failed: backend not initialized");
            return StoreErrorCode::ERROR;
        }

        std::vector<uint8_t> buffer(EXIST_PROBE_BUFFER_SIZE, 0);
        uint64_t valueSize = 0;
        const int32_t ret =
            backendOp_.get(handle_, key.c_str(), buffer.data(), buffer.size(), BACKEND_FLAGS, &valueSize);
        if (ret == SMEM_STORE_BACKEND_CODE_OK || ret == SMEM_STORE_BACKEND_CODE_BUFEX) {
            return StoreErrorCode::SUCCESS;
        }
        return MapCommonResult(ret);
    } catch (const std::exception &e) {
        STORE_LOG_ERROR("Exist failed: exception, key=" << key << ", what=" << e.what());
        return StoreErrorCode::ERROR;
    } catch (...) {
        STORE_LOG_ERROR("Exist failed: unknown exception, key=" << key);
        return StoreErrorCode::ERROR;
    }
}

void SmemExternalBackend::Clear() noexcept
{
    STORE_LOG_WARN("Clear() is not supported by external backend, operation skipped");
}

bool SmemExternalBackend::IsDistributed() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || handle_ == nullptr || backendOp_.distributed == nullptr) {
        return false;
    }
    return backendOp_.distributed(BACKEND_FLAGS);
}

bool SmemExternalBackend::SupportsTTL() const noexcept
{
    return false;
}

StoreErrorCode SmemExternalBackend::AcquireDistributedLock(const std::string &name) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || handle_ == nullptr || backendOp_.lock == nullptr) {
        STORE_LOG_ERROR("AcquireDistributedLock failed: backend not initialized");
        return StoreErrorCode::ERROR;
    }

    return MapCommonResult(backendOp_.lock(handle_, name.c_str(), BACKEND_FLAGS));
}

StoreErrorCode SmemExternalBackend::ReleaseDistributedLock(const std::string &name) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || handle_ == nullptr || backendOp_.unlock == nullptr) {
        STORE_LOG_ERROR("ReleaseDistributedLock failed: backend not initialized");
        return StoreErrorCode::ERROR;
    }

    return MapCommonResult(backendOp_.unlock(handle_, name.c_str(), BACKEND_FLAGS));
}

StoreErrorCode SmemExternalBackend::TryAcquireDistributedLock(const std::string &name, int64_t timeoutMs) noexcept
{
    if (timeoutMs > 0) {
        STORE_LOG_WARN("TryAcquireDistributedLock: timeoutMs=" << timeoutMs << " is ignored, try_lock is non-blocking");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || handle_ == nullptr || backendOp_.try_lock == nullptr) {
        STORE_LOG_ERROR("TryAcquireDistributedLock failed: backend not initialized");
        return StoreErrorCode::ERROR;
    }

    return MapCommonResult(backendOp_.try_lock(handle_, name.c_str(), BACKEND_FLAGS));
}

} // namespace smem
} // namespace ock
