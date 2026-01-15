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

#include "config_store_log.h"
#include "smem_tcp_config_store.h"
#include "smem_prefix_config_store.h"
#include "smem_store_factory.h"

namespace ock {
namespace smem {
static __thread int failedReason_ = 0;
std::mutex StoreFactory::storesMutex_;
std::unordered_map<std::string, StorePtr> StoreFactory::storesMap_;
smem_tls_config StoreFactory::tlsOption_{};

StorePtr StoreFactory::CreateStore(const std::string &ip, uint16_t port, bool isServer, uint32_t worldSize,
                                   int32_t rankId, int32_t connMaxRetry) noexcept
{
    std::string storeKey = std::string(ip).append(":").append(std::to_string(port));

    std::unique_lock<std::mutex> lockGuard{storesMutex_};
    auto pos = storesMap_.find(storeKey);
    if (pos != storesMap_.end()) {
        return pos->second;
    }

    auto store = SmMakeRef<TcpConfigStore>(ip, port, isServer, worldSize, rankId);
    STORE_ASSERT_RETURN(store != nullptr, nullptr);

    auto ret = store->Startup(tlsOption_, connMaxRetry);
    if (ret == SM_RESOURCE_IN_USE) {
        STORE_LOG_INFO("Startup for store(url=" << ip << ":" << port << ", isServer=" << isServer << ", rank=" << rankId
                                                << ") address in use");
        failedReason_ = SM_RESOURCE_IN_USE;
        return nullptr;
    }
    if (ret != 0) {
        STORE_LOG_ERROR("Startup for store(url=" << ip << ":" << port << ", isServer=" << isServer
                                                 << ", rank=" << rankId << ") failed:" << ret);
        failedReason_ = ret;
        return nullptr;
    }

    storesMap_.emplace(storeKey, store.Get());
    lockGuard.unlock();

    return store.Get();
}

StorePtr StoreFactory::CreateStoreServer(const std::string &ip, uint16_t port, uint32_t worldSize, int32_t rankId,
                                         int32_t connMaxRetry) noexcept
{
    std::string storeKey = std::string(ip).append(":").append(std::to_string(port));

    std::unique_lock<std::mutex> lockGuard{storesMutex_};
    auto pos = storesMap_.find(storeKey);
    if (pos != storesMap_.end()) {
        return pos->second;
    }

    auto store = SmMakeRef<TcpConfigStore>(ip, port, true, worldSize, rankId);
    STORE_ASSERT_RETURN(store != nullptr, nullptr);

    auto ret = store->ServerStart(tlsOption_, connMaxRetry);
    if (ret == SM_RESOURCE_IN_USE) {
        STORE_LOG_ERROR("Failed to start config store server, ip:" << ip << " port:" << port << " rankId:" << rankId
                                                                   << " is in use, ret:" << ret);
        failedReason_ = SM_RESOURCE_IN_USE;
        return nullptr;
    }
    if (ret != 0) {
        STORE_LOG_ERROR("Failed to start config store server, ip:" << ip << " port:" << port << " rankId:" << rankId
                                                                   << " ret:" << ret);
        failedReason_ = ret;
        return nullptr;
    }

    storesMap_.emplace(storeKey, store.Get());
    lockGuard.unlock();

    return store.Get();
}

StorePtr StoreFactory::CreateStoreClient(const std::string &ip, uint16_t port, uint32_t worldSize, int32_t rankId,
                                         int32_t connMaxRetry) noexcept
{
    std::string storeKey = std::string(ip).append(":").append(std::to_string(port));

    std::unique_lock<std::mutex> lockGuard{storesMutex_};
    auto pos = storesMap_.find(storeKey);
    if (pos != storesMap_.end()) {
        return pos->second;
    }

    auto store = SmMakeRef<TcpConfigStore>(ip, port, false, worldSize, rankId);
    STORE_ASSERT_RETURN(store != nullptr, nullptr);

    auto ret = store->ClientStart(tlsOption_, connMaxRetry);
    if (ret != 0) {
        STORE_LOG_ERROR("Failed to start config store client, ip:" << ip << " port:" << port << " rankId:" << rankId
                                                                   << " ret:" << ret);
        failedReason_ = ret;
        return nullptr;
    }

    storesMap_.emplace(storeKey, store.Get());
    lockGuard.unlock();

    return store.Get();
}

void StoreFactory::DestroyStore(const std::string &ip, uint16_t port) noexcept
{
    std::string storeKey = std::string(ip).append(":").append(std::to_string(port));
    std::unique_lock<std::mutex> lockGuard{storesMutex_};
    storesMap_.erase(storeKey);
}

void StoreFactory::DestroyStoreAll(bool afterFork) noexcept
{
    if (afterFork) {
        for (auto &e : storesMap_) {
            Convert<ConfigStore, TcpConfigStore>(e.second)->Shutdown(afterFork);
        }
    } else {
        std::unique_lock<std::mutex> lockGuard{storesMutex_};
        for (auto &e : storesMap_) {
            Convert<ConfigStore, TcpConfigStore>(e.second)->Shutdown(afterFork);
        }
    }
    storesMap_.clear();
}

StorePtr StoreFactory::PrefixStore(const ock::smem::StorePtr &base, const std::string &prefix) noexcept
{
    STORE_VALIDATE_RETURN(base != nullptr, "invalid param, base is nullptr", nullptr);

    auto store = SmMakeRef<PrefixConfigStore>(base, prefix);
    STORE_ASSERT_RETURN(store != nullptr, nullptr);

    return store.Get();
}

int StoreFactory::GetFailedReason() noexcept
{
    return failedReason_;
}

void StoreFactory::SetTlsInfo(const smem_tls_config &tlsOption) noexcept
{
    tlsOption_ = tlsOption;
}

} // namespace smem
} // namespace ock