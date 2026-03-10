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

#include "smem_etcd_store_backend.h"

#include <chrono>
#include <mutex>
#include <thread>

#include "smem_config_store_logger.h"
#include "network_endpoint_util.h"
#include "dl_etcd_api.h"
#include "smem_etcd_client.h"

namespace ock {
namespace smem {

// ETCD lease TTL
constexpr int32_t PUT_LEASE_TTL_SEC = 5;

SmemEtcdStoreBackend::SmemEtcdStoreBackend() noexcept = default;

SmemEtcdStoreBackend::~SmemEtcdStoreBackend() noexcept
{
    std::unique_lock<std::mutex> uniqueLock(mutex_);
    if (initialized_) {
        EtcdClientV3::GetInstance().Close();
        initialized_ = false;
    }
}

StoreErrorCode SmemEtcdStoreBackend::Initialize(const std::string &backendUrl, const std::string &userName,
                                                const std::string &password)
{
    std::unique_lock<std::mutex> uniqueLock(mutex_);
    if (initialized_) {
        return SUCCESS;
    }
    STORE_ASSERT_RETURN(EtcdApi::LoadLibrary() == SM_OK, StoreErrorCode::ERROR);

    std::string ip;
    uint16_t port = 0;
    STORE_ASSERT_RETURN(NetworkEndpointUtil::ExtractIpAndPort(backendUrl, ip, port), StoreErrorCode::ERROR);

    auto endPoint = NetworkEndpointUtil::BuildEndpoint("http", ip, port);
    STORE_ASSERT_RETURN(!endPoint.empty(), StoreErrorCode::ERROR);

    STORE_LOG_INFO("[ETCD] Initializing connection: " << endPoint);

    int ret =
        EtcdClientV3::GetInstance().Initialize(endPoint.c_str(), userName.c_str(), password.c_str(), PUT_LEASE_TTL_SEC);
    if (ret != 0) {
        STORE_LOG_ERROR("[ETCD] Failed to init etcd client, endpoints=" << endPoint << ", ret=" << ret << ", error="
                                                                        << EtcdClientV3::GetInstance().GetLastError());
        return StoreErrorCode::ERROR;
    }

    STORE_LOG_INFO("[ETCD] Connection initialized successfully, endPoint: " << endPoint);
    initialized_ = true;
    return StoreErrorCode::SUCCESS;
}

void SmemEtcdStoreBackend::UnInitialize()
{
    std::unique_lock<std::mutex> uniqueLock(mutex_);
    if (!initialized_) {
        return;
    }
    EtcdClientV3::GetInstance().Close();
    initialized_ = false;
}

std::string SmemEtcdStoreBackend::BackendName() const noexcept
{
    return "Etcd";
}

StoreErrorCode SmemEtcdStoreBackend::Get(const std::string &key, std::vector<uint8_t> &outValue) const noexcept
{
    if (!initialized_) {
        STORE_LOG_ERROR("[ETCD] Get failed: backend not initialized");
        return StoreErrorCode::ERROR;
    }

    std::string valueStr;
    int32_t ret = EtcdClientV3::GetInstance().GetValue(key, valueStr);
    if (ret != 0) {
        STORE_LOG_WARN("[ETCD] Get key failed: " << key << ", msg: " << EtcdClientV3::GetInstance().GetLastError());
        return StoreErrorCode::NOT_EXIST;
    }

    outValue.assign(valueStr.begin(), valueStr.end());
    STORE_LOG_DEBUG("[ETCD] Get key success: " << key << ", size=" << outValue.size());
    return StoreErrorCode::SUCCESS;
}

StoreErrorCode SmemEtcdStoreBackend::Put(const std::string &key, const std::vector<uint8_t> &value,
                                         int64_t ttlSeconds) noexcept
{
    if (!initialized_) {
        STORE_LOG_ERROR("[ETCD] Put failed: backend not initialized");
        return StoreErrorCode::ERROR;
    }

    std::string valueStr(value.begin(), value.end());
    int32_t ret = EtcdClientV3::GetInstance().SetValue(key, valueStr, ttlSeconds);
    if (ret != 0) {
        STORE_LOG_ERROR("[ETCD] Put key failed: " << key << ", size=" << value.size() << ", ttl=" << ttlSeconds
                                                  << ", error=" << EtcdClientV3::GetInstance().GetLastError());
        return StoreErrorCode::ERROR;
    }

    STORE_LOG_DEBUG("[ETCD] Put key success: " << key << ", size=" << value.size() << ", value=" << valueStr
                                               << ", ttl=" << ttlSeconds);
    return StoreErrorCode::SUCCESS;
}

StoreErrorCode SmemEtcdStoreBackend::Delete(const std::string &key) noexcept
{
    if (!initialized_) {
        STORE_LOG_ERROR("[ETCD] Delete failed: backend not initialized");
        return StoreErrorCode::ERROR;
    }

    int32_t ret = EtcdClientV3::GetInstance().DeleteKey(key);
    if (ret != 0) {
        STORE_LOG_WARN("[ETCD] Delete key failed: " << key << ", error=" << EtcdClientV3::GetInstance().GetLastError());
        return StoreErrorCode::ERROR;
    }

    STORE_LOG_DEBUG("[ETCD] Delete key success: " << key);
    return StoreErrorCode::SUCCESS;
}

StoreErrorCode SmemEtcdStoreBackend::Exist(const std::string &key) const noexcept
{
    if (!initialized_) {
        STORE_LOG_ERROR("[ETCD] Exist check failed: backend not initialized");
        return StoreErrorCode::ERROR;
    }

    std::string dummyValue;
    int32_t ret = EtcdClientV3::GetInstance().GetValue(key, dummyValue);
    return (ret == 0) ? StoreErrorCode::SUCCESS : StoreErrorCode::NOT_EXIST;
}

bool SmemEtcdStoreBackend::IsDistributed() const noexcept
{
    return true;
}

bool SmemEtcdStoreBackend::SupportsTTL() const noexcept
{
    return true;
}

StoreErrorCode SmemEtcdStoreBackend::AcquireDistributedLock(const std::string &name) noexcept
{
    if (!initialized_) {
        STORE_LOG_ERROR("[ETCD] AcquireDistributedLock failed: backend not initialized");
        return StoreErrorCode::ERROR;
    }

    // Note: The 'name' parameter is logged for debugging but not used in the actual lock operation.
    // The current etcd lock implementation only supports a global lock (not named locks).
    // All callers share the same distributed lock regardless of the 'name' parameter.
    auto ret = EtcdClientV3::GetInstance().RawLock();
    if (ret != 0) {
        auto err = EtcdClientV3::GetInstance().GetLastError();
        STORE_LOG_ERROR("[ETCD] Failed to acquire lock: " << name << ", error=" << err);
        return StoreErrorCode::ERROR;
    }
    STORE_LOG_DEBUG("[ETCD] Acquired distributed lock: " << name);
    return StoreErrorCode::SUCCESS;
}

StoreErrorCode SmemEtcdStoreBackend::ReleaseDistributedLock(const std::string &name) noexcept
{
    if (!initialized_) {
        STORE_LOG_ERROR("[ETCD] ReleaseDistributedLock failed: backend not initialized");
        return StoreErrorCode::ERROR;
    }

    // Note: The 'name' parameter is logged for debugging but not used in the actual unlock operation.
    // The current etcd lock implementation only supports a global lock (not named locks).
    auto ret = EtcdClientV3::GetInstance().RawUnlock();
    if (ret != 0) {
        auto err = EtcdClientV3::GetInstance().GetLastError();
        STORE_LOG_ERROR("[ETCD] Failed to release lock: " << name << ", error=" << err);
        return StoreErrorCode::ERROR;
    }
    STORE_LOG_DEBUG("[ETCD] Released distributed lock: " << name);
    return StoreErrorCode::SUCCESS;
}

StoreErrorCode SmemEtcdStoreBackend::TryAcquireDistributedLock(const std::string &name, int64_t timeoutMs) noexcept
{
    (void)timeoutMs;
    STORE_LOG_WARN("[ETCD] TryAcquireDistributedLock not implemented yet, lock name: " << name);
    return StoreErrorCode::ERROR;
}

void SmemEtcdStoreBackend::Clear() noexcept
{
    STORE_LOG_WARN("[ETCD] Clear() not implemented yet");
}

} // namespace smem
} // namespace ock