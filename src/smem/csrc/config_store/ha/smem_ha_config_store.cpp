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
#include "config_store_log.h"

namespace ock {
namespace smem {

HaConfigStore::HaConfigStore(TcpConfigStorePtr clientDelegate) noexcept : clientDelegate_(std::move(clientDelegate)) {}

HaConfigStore::~HaConfigStore() noexcept
{
    Shutdown();
}

Result HaConfigStore::Startup(const smem_tls_config &tlsConfig, int reconnectRetryTimes) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    STORE_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Startup(tlsConfig, reconnectRetryTimes);
}

Result HaConfigStore::ClientStart(const smem_tls_config &tlsConfig, int reconnectRetryTimes) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    STORE_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->ClientStart(tlsConfig, reconnectRetryTimes);
}

Result HaConfigStore::ServerStart(const smem_tls_config &tlsConfig, int reconnectRetryTimes) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    STORE_ASSERT_RETURN(serverDelegate_ != nullptr, SM_ERROR);
    return serverDelegate_->Startup(tlsConfig);
}

void HaConfigStore::Shutdown(bool afterFork) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    if (clientDelegate_ != nullptr) {
        clientDelegate_->Shutdown(afterFork);
    }
    if (serverDelegate_ != nullptr) {
        serverDelegate_->Shutdown(afterFork);
    }
}

Result HaConfigStore::Set(const std::string &key, const std::vector<uint8_t> &value) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    STORE_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Set(key, value);
}

Result HaConfigStore::Add(const std::string &key, int64_t increment, int64_t &value) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    STORE_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Add(key, increment, value);
}

Result HaConfigStore::Remove(const std::string &key, bool printKeyNotExist) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    STORE_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Remove(key, printKeyNotExist);
}

Result HaConfigStore::Append(const std::string &key, const std::vector<uint8_t> &value, uint64_t &newSize) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    STORE_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Append(key, value, newSize);
}

Result HaConfigStore::Cas(const std::string &key, const std::vector<uint8_t> &expect, const std::vector<uint8_t> &value,
                          std::vector<uint8_t> &exists) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    STORE_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Cas(key, expect, value, exists);
}

Result
HaConfigStore::Watch(const std::string &key,
                     const std::function<void(int result, const std::string &, const std::vector<uint8_t> &)> &notify,
                     uint32_t &wid) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    STORE_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Watch(key, notify, wid);
}

Result HaConfigStore::Watch(WatchRankType type, const std::function<void(WatchRankType, uint32_t)> &notify,
                            uint32_t &wid) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    STORE_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Watch(type, notify, wid);
}

Result HaConfigStore::Unwatch(uint32_t wid) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    STORE_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Unwatch(wid);
}

Result HaConfigStore::Write(const std::string &key, const std::vector<uint8_t> &value, const uint32_t offset) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    STORE_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Write(key, value, offset);
}

std::string HaConfigStore::GetCompleteKey(const std::string &key) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    STORE_ASSERT_RETURN(clientDelegate_ != nullptr, "");
    return clientDelegate_->GetCompleteKey(key);
}

std::string HaConfigStore::GetCommonPrefix() noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    STORE_ASSERT_RETURN(clientDelegate_ != nullptr, "");
    return clientDelegate_->GetCommonPrefix();
}

StorePtr HaConfigStore::GetCoreStore() noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    STORE_ASSERT_RETURN(clientDelegate_ != nullptr, nullptr);
    return clientDelegate_->GetCoreStore();
}

void HaConfigStore::RegisterReconnectHandler(ConfigStoreReconnectHandler callback) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    if (clientDelegate_ != nullptr) {
        clientDelegate_->RegisterReconnectHandler(callback);
    }
}

Result HaConfigStore::ReConnectAfterBroken(int reconnectRetryTimes) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    STORE_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->ReConnectAfterBroken(reconnectRetryTimes);
}

bool HaConfigStore::GetConnectStatus() noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    STORE_ASSERT_RETURN(clientDelegate_ != nullptr, false);
    return clientDelegate_->GetConnectStatus();
}

void HaConfigStore::SetConnectStatus(bool status) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    if (clientDelegate_ != nullptr) {
        clientDelegate_->SetConnectStatus(status);
    }
}

void HaConfigStore::RegisterClientBrokenHandler(const ConfigStoreClientBrokenHandler &handler) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    if (clientDelegate_ != nullptr) {
        clientDelegate_->RegisterClientBrokenHandler(handler);
    }
}

void HaConfigStore::RegisterServerBrokenHandler(const ConfigStoreServerBrokenHandler &handler) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    if (serverDelegate_ != nullptr) {
        serverDelegate_->RegisterBrokenLinkCHandler(handler);
    }
}

void HaConfigStore::RegisterServerOpHandler(int16_t opCode, const ConfigStoreServerOpHandler &handler) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    if (serverDelegate_ != nullptr) {
        serverDelegate_->RegisterOpHandler(opCode, handler);
    }
}

void HaConfigStore::SetRankId(const int32_t &rankId) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    if (clientDelegate_ != nullptr) {
        clientDelegate_->SetRankId(rankId);
    }
}

Result HaConfigStore::GetReal(const std::string &key, std::vector<uint8_t> &value, int64_t timeoutMs) noexcept
{
    std::shared_lock<std::shared_mutex> lock(delegateRwLock_);
    STORE_ASSERT_RETURN(clientDelegate_ != nullptr, SM_ERROR);
    return clientDelegate_->Get(key, value, timeoutMs);
}

} // namespace smem
} // namespace ock