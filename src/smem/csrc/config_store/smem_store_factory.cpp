/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#include "smem_logger.h"
#include "smem_tcp_config_store.h"
#include "smem_prefix_config_store.h"
#include "smem_store_factory.h"

namespace ock {
namespace smem {
StorePtr StoreFactory::CreateStore(const std::string &ip, uint16_t port, bool isServer, int32_t rankId,
    int32_t connMaxRetry) noexcept
{
    auto store = SmMakeRef<TcpConfigStore>(ip, port, isServer, rankId);
    SM_ASSERT_RETURN(store != nullptr, nullptr);

    auto ret = store->Startup(connMaxRetry);
    if (ret != 0) {
        SM_LOG_ERROR("Startup for store(url=" << ip << ":" << port << ", isSever=" << isServer << ", rank=" << rankId <<
            ") failed:" << ret);
        return nullptr;
    }

    return store.Get();
}

StorePtr StoreFactory::PrefixStore(const ock::smem::StorePtr &base, const std::string &prefix) noexcept
{
    SM_PARAM_VALIDATE(base == nullptr, "invalid param, base is nullptr", nullptr);

    auto store = SmMakeRef<PrefixConfigStore>(base, prefix);
    SM_ASSERT_RETURN(store != nullptr, nullptr);

    return store.Get();
}
} // namespace smem
} // namespace ock