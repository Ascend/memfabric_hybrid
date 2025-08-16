/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#include "smem_logger.h"
#include "smem_tcp_config_store.h"
#include "smem_prefix_config_store.h"
#include "smem_store_factory.h"

namespace ock {
namespace smem {
static __thread int failedReason_ = 0;
std::mutex StoreFactory::storesMutex_;
std::unordered_map<std::string, StorePtr> StoreFactory::storesMap_;

StorePtr StoreFactory::CreateStore(const std::string &ip, uint16_t port, bool isServer,
                                   uint32_t worldSize, int32_t rankId, int32_t connMaxRetry) noexcept
{
    std::string storeKey = std::string(ip).append(":").append(std::to_string(port));

    std::unique_lock<std::mutex> lockGuard{storesMutex_};
    auto pos = storesMap_.find(storeKey);
    if (pos != storesMap_.end()) {
        return pos->second;
    }

    auto store = SmMakeRef<TcpConfigStore>(ip, port, isServer, worldSize, rankId);
    SM_ASSERT_RETURN(store != nullptr, nullptr);

    auto ret = store->Startup(connMaxRetry);
    if (ret == SM_RESOURCE_IN_USE) {
        SM_LOG_INFO("Startup for store(url=" << ip << ":" << port << ", isSever=" << isServer << ", rank=" << rankId
                                             << ") address in use");
        failedReason_ = SM_RESOURCE_IN_USE;
        return nullptr;
    }
    if (ret != 0) {
        SM_LOG_ERROR("Startup for store(url=" << ip << ":" << port << ", isSever=" << isServer << ", rank=" << rankId
                                              << ") failed:" << ret);
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

StorePtr StoreFactory::PrefixStore(const ock::smem::StorePtr &base, const std::string &prefix) noexcept
{
    SM_PARAM_VALIDATE(base == nullptr, "invalid param, base is nullptr", nullptr);

    auto store = SmMakeRef<PrefixConfigStore>(base, prefix);
    SM_ASSERT_RETURN(store != nullptr, nullptr);

    return store.Get();
}

int StoreFactory::GetFailedReason() noexcept
{
    return failedReason_;
}
}  // namespace smem
}  // namespace ock