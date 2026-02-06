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

#include <mutex>
#include "config_store_log.h"
#include "smem_local_memory_backend.h"

namespace ock {
namespace smem {

SmemLocalMemoryBackend::SmemLocalMemoryBackend() noexcept = default;

SmemLocalMemoryBackend::~SmemLocalMemoryBackend() noexcept = default;

StoreErrorCode SmemLocalMemoryBackend::Initialize(const std::string &backendUrl, const std::string &userName,
                                                  const std::string &password)
{
    (void)backendUrl;
    (void)userName;
    (void)password;
    return StoreErrorCode::SUCCESS;
}

std::string SmemLocalMemoryBackend::BackendName() const noexcept
{
    return "LocalMemory";
}

StoreErrorCode SmemLocalMemoryBackend::Get(const std::string &key, std::vector<uint8_t> &outValue) const noexcept
{
    auto iter = kvStore_.find(key);
    if (iter == kvStore_.end()) {
        return StoreErrorCode::NOT_EXIST;
    }
    outValue = iter->second;
    return StoreErrorCode::SUCCESS;
}

StoreErrorCode SmemLocalMemoryBackend::Put(const std::string &key, const std::vector<uint8_t> &value,
                                           int64_t ttlSeconds) noexcept
{
    (void)ttlSeconds; // TTL not supported in local memory backend
    kvStore_[key] = value;
    return StoreErrorCode::SUCCESS;
}

StoreErrorCode SmemLocalMemoryBackend::Delete(const std::string &key) noexcept
{
    auto erased = kvStore_.erase(key);
    return erased > 0 ? StoreErrorCode::SUCCESS : StoreErrorCode::NOT_EXIST;
}

StoreErrorCode SmemLocalMemoryBackend::Exist(const std::string &key) const noexcept
{
    return kvStore_.find(key) != kvStore_.end() ? StoreErrorCode::SUCCESS : StoreErrorCode::NOT_EXIST;
}

bool SmemLocalMemoryBackend::IsDistributed() const noexcept
{
    return false;
}

bool SmemLocalMemoryBackend::SupportsTTL() const noexcept
{
    return false;
}

StoreErrorCode SmemLocalMemoryBackend::AcquireDistributedLock(const std::string &name) noexcept
{
    STORE_LOG_ERROR("Not implemented yet");
    return StoreErrorCode::ERROR;
}

StoreErrorCode SmemLocalMemoryBackend::ReleaseDistributedLock(const std::string &name) noexcept
{
    STORE_LOG_ERROR("Not implemented yet");
    return StoreErrorCode::ERROR;
}

StoreErrorCode SmemLocalMemoryBackend::TryAcquireDistributedLock(const std::string &name, int64_t timeoutMs) noexcept
{
    STORE_LOG_ERROR("Not implemented yet");
    return StoreErrorCode::ERROR;
}

void SmemLocalMemoryBackend::UnInitialize() {}

void SmemLocalMemoryBackend::Clear() noexcept
{
    kvStore_.clear();
}

} // namespace smem
} // namespace ock