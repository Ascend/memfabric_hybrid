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

#include "smem_store_factory.h"
#include "smem_config_store_logger.h"
#include "smem_tcp_config_store.h"
#include "smem_prefix_config_store.h"
#include "smem_local_memory_backend.h"
#include "network_endpoint_util.h"
#include "smem_etcd_store_backend.h"
#include "smem_ha_config_store.h"

namespace ock {
namespace smem {
static __thread int failedReason_ = 0;
std::mutex StoreFactory::storesMutex_;
std::unordered_map<std::string, StorePtr> StoreFactory::storesMap_;
smem_tls_config StoreFactory::tlsOption_{};

[[nodiscard]] static StoreBackendPtr CreateBackend(BackendType type, const std::string &backendUrl,
                                                   const std::string &userName, const std::string &password)
{
    StoreBackendPtr result = nullptr;

    switch (type) {
        case BackendType::TCP: {
            auto backend = SmMakeRef<SmemLocalMemoryBackend>();
            result = Convert<SmemLocalMemoryBackend, ConfigStoreBackend>(backend);
            break;
        }
        case BackendType::ETCD: {
            auto backend = SmMakeRef<SmemEtcdStoreBackend>();
            result = Convert<SmemEtcdStoreBackend, ConfigStoreBackend>(backend);
            break;
        }
        default:
            return nullptr;
    }
    if (result == nullptr) {
        return nullptr;
    }

    if (result->Initialize(backendUrl, userName, password) != SUCCESS) {
        STORE_LOG_ERROR("Backend init failed, url: " << backendUrl);
        return nullptr;
    }

    return result;
}

static std::string BuildTcpUrl(const std::string &ip, uint16_t port)
{
    return NetworkEndpointUtil::BuildEndpoint("tcp", ip, port);
}

// --- CreateStoreByUrl (ip, port) overload: TCP only ---
StorePtr StoreFactory::CreateStore(const std::string &ip, uint16_t port, bool isServer, uint32_t worldSize,
                                   int32_t rankId, int32_t connMaxRetry) noexcept
{
    std::string storeUrl = BuildTcpUrl(ip, port);
    STORE_ASSERT_RETURN(!storeUrl.empty(), nullptr);
    std::string storeKey = storeUrl;

    std::unique_lock<std::mutex> lockGuard{storesMutex_};
    auto pos = storesMap_.find(storeKey);
    if (pos != storesMap_.end()) {
        return pos->second;
    }
    auto backend = CreateBackend(BackendType::TCP, storeUrl, "", "");
    STORE_ASSERT_RETURN(backend != nullptr, nullptr);

    auto store = SmMakeRef<TcpConfigStore>(backend, ip, port, isServer, worldSize, rankId);
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

// --- CreateStoreByUrl (storeUrl) overload: TCP / ETCD / etc. ---
StorePtr StoreFactory::CreateStoreByUrl(const std::string &storeUrl, bool isServer, uint32_t worldSize, int32_t rankId,
                                        int32_t connMaxRetry) noexcept
{
    std::string storeKey = storeUrl;
    uint16_t port;
    std::string ip;
    BackendType type;
    STORE_ASSERT_RETURN(NetworkEndpointUtil::ExtractIpAndPort(storeUrl, ip, port, type), nullptr);

    std::unique_lock<std::mutex> lockGuard{storesMutex_};
    auto pos = storesMap_.find(storeKey);
    if (pos != storesMap_.end()) {
        return pos->second;
    }
    auto backend = CreateBackend(type, storeUrl, "", "");
    STORE_ASSERT_RETURN(backend != nullptr, nullptr);
    if (backend->IsDistributed()) {
        return CreateHaStore(backend, storeUrl, worldSize);
    }
    auto store = SmMakeRef<TcpConfigStore>(backend, ip, port, isServer, worldSize, rankId);
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

// --- CreateStoreServer (ip, port): TCP only ---
StorePtr StoreFactory::CreateStoreServer(const std::string &ip, uint16_t port, uint32_t worldSize, int32_t rankId,
                                         int32_t connMaxRetry) noexcept
{
    std::string storeUrl = BuildTcpUrl(ip, port);
    std::string storeKey = storeUrl;

    std::unique_lock<std::mutex> lockGuard{storesMutex_};
    auto pos = storesMap_.find(storeKey);
    if (pos != storesMap_.end()) {
        return pos->second;
    }
    auto backend = CreateBackend(BackendType::TCP, storeUrl, "", "");
    STORE_ASSERT_RETURN(backend != nullptr, nullptr);

    auto store = SmMakeRef<TcpConfigStore>(backend, ip, port, true, worldSize, rankId);
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

// --- CreateStoreClient (ip, port): TCP only ---
StorePtr StoreFactory::CreateStoreClient(const std::string &ip, uint16_t port, uint32_t worldSize, int32_t rankId,
                                         int32_t connMaxRetry) noexcept
{
    std::string storeUrl = BuildTcpUrl(ip, port);
    std::string storeKey = storeUrl;

    std::unique_lock<std::mutex> lockGuard{storesMutex_};
    auto pos = storesMap_.find(storeKey);
    if (pos != storesMap_.end()) {
        return pos->second;
    }
    auto backend = CreateBackend(BackendType::TCP, storeUrl, "", "");
    STORE_ASSERT_RETURN(backend != nullptr, nullptr);

    auto store = SmMakeRef<TcpConfigStore>(backend, ip, port, false, worldSize, rankId);
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

StorePtr StoreFactory::CreateHaStore(const StoreBackendPtr &backend, const std::string &storeUrl,
                                     uint32_t worldSize) noexcept
{
    STORE_ASSERT_RETURN(backend != nullptr, nullptr);
    auto clientDelegate = SmMakeRef<TcpConfigStore>(backend, "", 0, false, worldSize);
    STORE_ASSERT_RETURN(clientDelegate != nullptr, nullptr);
    const auto store = SmMakeRef<HaConfigStore>(backend, clientDelegate, storeUrl, worldSize);
    STORE_ASSERT_RETURN(store != nullptr, nullptr);

    const auto ret = store->Startup(tlsOption_);
    if (ret != SM_OK) {
        STORE_LOG_ERROR("Startup for ha store failed:" << ret);
        failedReason_ = ret;
        return nullptr;
    }

    storesMap_.emplace(storeUrl, store.Get());
    return store.Get();
}

// --- DestroyStore (ip, port) overload ---
void StoreFactory::DestroyStore(const std::string &ip, uint16_t port) noexcept
{
    std::string storeKey = BuildTcpUrl(ip, port);
    std::unique_lock<std::mutex> lockGuard{storesMutex_};
    storesMap_.erase(storeKey);
}

// --- DestroyStore (storeUrl) overload ---
void StoreFactory::DestroyStore(const std::string &storeUrl) noexcept
{
    const std::string &storeKey = storeUrl;
    std::unique_lock<std::mutex> lockGuard{storesMutex_};
    storesMap_.erase(storeKey);
}

void StoreFactory::DestroyStoreAll(bool afterFork) noexcept
{
    std::unordered_map<std::string, StorePtr> localStores;
    {
        if (afterFork) {
            localStores.swap(storesMap_);
        } else {
            std::unique_lock<std::mutex> lockGuard{storesMutex_};
            localStores.swap(storesMap_);
        }
    }
    for (auto &e : localStores) {
        SmRef<TcpConfigStore> tcp = Convert<ConfigStore, TcpConfigStore>(e.second);
        if (tcp != nullptr) {
            tcp->Shutdown(afterFork);
            continue;
        }
        auto ha = Convert<ConfigStore, HaConfigStore>(e.second);
        if (ha != nullptr) {
            ha.Set(nullptr);
        }
    }
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
