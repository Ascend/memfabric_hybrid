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
#include <cctype>
#include <utility>

#include "smem.h"
#include "smem_config_store_logger.h"
#include "network_endpoint_util.h"
#include "dl_etcd_api.h"
#include "smem_etcd_client.h"

namespace ock {
namespace smem {

namespace {

constexpr int32_t PUT_LEASE_TTL_SEC = 5;
constexpr char CONFIG_STORE_CLUSTER_ROOT[] = "/memfabric_hybrid/config_store/clusters/";

[[nodiscard]] std::string BuildClusterRoot(const std::string &instanceId)
{
    if (instanceId.empty()) {
        return "";
    }

    std::string clusterRoot = CONFIG_STORE_CLUSTER_ROOT;
    clusterRoot.append(instanceId);
    return clusterRoot;
}

[[nodiscard]] std::string BuildClusterQualifiedName(const std::string &clusterRoot, const std::string &name)
{
    if (clusterRoot.empty()) {
        return name;
    }

    if (name.compare(0, clusterRoot.size(), clusterRoot) == 0) {
        return name;
    }

    std::string qualifiedName = clusterRoot;
    qualifiedName.push_back('/');
    if (!name.empty() && name.front() == '/') {
        qualifiedName.append(name.substr(1));
        return qualifiedName;
    }

    qualifiedName.append(name);
    return qualifiedName;
}

} // namespace

SmemEtcdStoreBackend::SmemEtcdStoreBackend(std::string instanceId) noexcept
    : clusterId_(std::move(instanceId)), clusterRoot_(BuildClusterRoot(clusterId_))
{}

SmemEtcdStoreBackend::~SmemEtcdStoreBackend() noexcept
{
    std::unique_lock<std::mutex> uniqueLock(mutex_);
    if (initialized_) {
        initialized_ = false;
        uniqueLock.unlock();
        EtcdClientV3::GetInstance().Close();
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
    initialized_ = false;
    uniqueLock.unlock();
    EtcdClientV3::GetInstance().Close();
}

std::string SmemEtcdStoreBackend::BackendName() const noexcept
{
    return "Etcd";
}

bool PrefixGetFill(const char *key, const void *value, uint64_t size, void *context)
{
    if (key == nullptr) {
        STORE_LOG_ERROR("[ETCD] failed to do prefix get, key is null");
        return false;
    }
    if (value == nullptr) {
        STORE_LOG_ERROR("[ETCD] failed to do prefix get, value is null");
        return false;
    }
    if (size == 0) {
        STORE_LOG_ERROR("[ETCD] failed to do prefix get, size is 0");
        return false;
    }

    auto *prefixGetMap = static_cast<PrefixGetMap *>(context);
    if (prefixGetMap == nullptr) {
        return false;
    }
    std::vector<uint8_t> outValue(static_cast<const uint8_t *>(value), static_cast<const uint8_t *>(value) + size);
    prefixGetMap->emplace(key, outValue);
    return true;
}

StoreErrorCode SmemEtcdStoreBackend::PrefixGet(const std::string &key, PrefixGetMap &outValue) const noexcept
{
    if (!initialized_) {
        STORE_LOG_ERROR("[ETCD] PrefixGet failed: backend not initialized");
        return StoreErrorCode::ERROR;
    }
    const std::string qualifiedKey = BuildClusterQualifiedName(clusterRoot_, key);
    const smem_store_prefix_get_ctx_t prefixGetCtx = {
        .prefix = qualifiedKey.c_str(),
        .marker = nullptr,
        .context = &outValue,
        .fill = &PrefixGetFill,
    };
    int32_t ret = EtcdClientV3::GetInstance().PrefixGet(&prefixGetCtx, 0);
    if (ret != 0) {
        STORE_LOG_ERROR("[ETCD] PrefixGet failed: EtcdClientV3 API failed");
        return StoreErrorCode::ERROR;
    }
    return StoreErrorCode::SUCCESS;
}

StoreErrorCode SmemEtcdStoreBackend::Get(const std::string &key, std::vector<uint8_t> &outValue) const noexcept
{
    if (!initialized_) {
        STORE_LOG_ERROR("[ETCD] Get failed: backend not initialized");
        return StoreErrorCode::ERROR;
    }

    const std::string qualifiedKey = BuildClusterQualifiedName(clusterRoot_, key);
    std::string valueStr;
    int32_t ret = EtcdClientV3::GetInstance().GetValue(qualifiedKey, valueStr);
    if (ret != 0) {
        STORE_LOG_WARN("[ETCD] Get key failed: " << qualifiedKey
                                                 << ", msg: " << EtcdClientV3::GetInstance().GetLastError());
        return StoreErrorCode::NOT_EXIST;
    }

    outValue.assign(valueStr.begin(), valueStr.end());
    STORE_LOG_DEBUG("[ETCD] Get key success: " << qualifiedKey << ", size=" << outValue.size());
    return StoreErrorCode::SUCCESS;
}

StoreErrorCode SmemEtcdStoreBackend::Put(const std::string &key, const std::vector<uint8_t> &value,
                                         int64_t ttlSeconds) noexcept
{
    if (!initialized_) {
        STORE_LOG_ERROR("[ETCD] Put failed: backend not initialized");
        return StoreErrorCode::ERROR;
    }

    const std::string qualifiedKey = BuildClusterQualifiedName(clusterRoot_, key);
    std::string valueStr(value.begin(), value.end());
    int32_t ret = EtcdClientV3::GetInstance().SetValue(qualifiedKey, valueStr, ttlSeconds);
    if (ret != 0) {
        STORE_LOG_ERROR("[ETCD] Put key failed: " << qualifiedKey << ", size=" << value.size() << ", ttl=" << ttlSeconds
                                                  << ", error=" << EtcdClientV3::GetInstance().GetLastError());
        return StoreErrorCode::ERROR;
    }

    STORE_LOG_DEBUG("[ETCD] Put key success: " << qualifiedKey << ", size=" << value.size() << ", value=" << valueStr
                                               << ", ttl=" << ttlSeconds);
    return StoreErrorCode::SUCCESS;
}

StoreErrorCode SmemEtcdStoreBackend::Delete(const std::string &key) noexcept
{
    if (!initialized_) {
        STORE_LOG_ERROR("[ETCD] Delete failed: backend not initialized");
        return StoreErrorCode::ERROR;
    }

    const std::string qualifiedKey = BuildClusterQualifiedName(clusterRoot_, key);
    int32_t ret = EtcdClientV3::GetInstance().DeleteKey(qualifiedKey);
    if (ret != 0) {
        STORE_LOG_WARN("[ETCD] Delete key failed: " << qualifiedKey
                                                    << ", error=" << EtcdClientV3::GetInstance().GetLastError());
        return StoreErrorCode::ERROR;
    }

    STORE_LOG_DEBUG("[ETCD] Delete key success: " << qualifiedKey);
    return StoreErrorCode::SUCCESS;
}

StoreErrorCode SmemEtcdStoreBackend::Exist(const std::string &key) const noexcept
{
    if (!initialized_) {
        STORE_LOG_ERROR("[ETCD] Exist check failed: backend not initialized");
        return StoreErrorCode::ERROR;
    }

    const std::string qualifiedKey = BuildClusterQualifiedName(clusterRoot_, key);
    std::string dummyValue;
    int32_t ret = EtcdClientV3::GetInstance().GetValue(qualifiedKey, dummyValue);
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

    const std::string qualifiedLockName = BuildClusterQualifiedName(clusterRoot_, name);
    int32_t ret = EtcdClientV3::GetInstance().RawLockNamed(qualifiedLockName);
    if (ret != 0 && !EtcdApi::SupportsNamedLock()) {
        STORE_LOG_ERROR(
            "[ETCD] Failed to acquire named lock because current libetcd_client_v3.so is too old, lock name: "
            << qualifiedLockName);
        return StoreErrorCode::ERROR;
    }
    if (ret != 0) {
        auto err = EtcdClientV3::GetInstance().GetLastError();
        STORE_LOG_ERROR("[ETCD] Failed to acquire lock: " << qualifiedLockName << ", error=" << err);
        return StoreErrorCode::ERROR;
    }
    STORE_LOG_DEBUG("[ETCD] Acquired distributed lock: " << qualifiedLockName);
    return StoreErrorCode::SUCCESS;
}

StoreErrorCode SmemEtcdStoreBackend::ReleaseDistributedLock(const std::string &name) noexcept
{
    if (!initialized_) {
        STORE_LOG_ERROR("[ETCD] ReleaseDistributedLock failed: backend not initialized");
        return StoreErrorCode::ERROR;
    }
    const std::string qualifiedLockName = BuildClusterQualifiedName(clusterRoot_, name);
    // The underlying etcd client keeps the acquired lock object in its shared wrapper state,
    // so unlock releases that stored mutex directly and does not need lockName as an input.
    auto ret = EtcdClientV3::GetInstance().RawUnlock();
    if (ret != 0) {
        auto err = EtcdClientV3::GetInstance().GetLastError();
        STORE_LOG_ERROR("[ETCD] Failed to release lock: " << qualifiedLockName << ", error=" << err);
        return StoreErrorCode::ERROR;
    }
    STORE_LOG_DEBUG("[ETCD] Released distributed lock: " << qualifiedLockName);
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
