/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "mmc_meta_lease_manager.h"
#include "mmc_montotonic.h"

namespace ock {
namespace mmc {
Result MmcMetaLeaseManager::Add(uint32_t id, uint32_t requestId, uint64_t ttl)
{
    MMC_LOG_DEBUG("MmcMetaLeaseManager ADD " << " id " << id << " requestId " << requestId << " ttl " << ttl);
    std::unique_lock<std::mutex> lockGuard(lock_);
    const uint64_t nowMs = ock::dagger::Monotonic::TimeNs() / 1000U;
    lease_ = std::max(lease_, nowMs + ttl);
    useClient.insert(GenerateClientId(id, requestId));
    return MMC_OK;
}

Result MmcMetaLeaseManager::Remove(uint32_t id, uint32_t requestId)
{
    MMC_LOG_DEBUG("MmcMetaLeaseManager Remove id " << id << " requestId " << requestId);
    std::unique_lock<std::mutex> lockGuard(lock_);
    useClient.erase(GenerateClientId(id, requestId));
    cv_.notify_all();
    return MMC_OK;
}
Result MmcMetaLeaseManager::Extend(uint64_t ttl)
{
    MMC_LOG_DEBUG("MmcMetaLeaseManager Extend " << " ttl " << ttl);
    std::unique_lock<std::mutex> lockGuard(lock_);
    const uint64_t nowMs = ock::dagger::Monotonic::TimeNs() / 1000U;
    lease_ = std::max(lease_, nowMs + ttl);
    return MMC_OK;
}
void MmcMetaLeaseManager::Wait()
{
    std::unique_lock<std::mutex> lockGuard(lock_);
    const uint64_t nowMs = ock::dagger::Monotonic::TimeNs() / 1000U;
    if (lease_ < nowMs) {
        return;
    }
    uint64_t waitTime = lease_ - nowMs;
    if (cv_.wait_for(lockGuard, std::chrono::milliseconds(), [this] { return !useClient.size(); })) {
        MMC_LOG_DEBUG("blob released one, useClient size" << useClient.size());
    } else {
        MMC_LOG_WARN("blob time out, wite time " << waitTime << " ms, used client size " << useClient.size());
    }
}
}
}