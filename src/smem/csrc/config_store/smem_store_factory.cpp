/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#include "smem_logger.h"
#include "smem_tcp_config_store.h"
#include "smem_prefix_config_store.h"
#include "smem_store_factory.h"

namespace ock {
namespace smem {

StorePtr StoreFactory::CreateStore(const std::string &ip, uint16_t port, bool isServer, int32_t rankId) noexcept
{
    auto store = std::make_shared<TcpConfigStore>(ip, port, isServer, rankId);
    auto ret = store->Startup();
    if (ret != 0) {
        SM_LOG_ERROR("startup for store(url=" << ip << ":" << port << ", isSever=" << isServer << ", rank=" << rankId
                                              << ") failed:" << ret);
        return nullptr;
    }

    return store;
}

StorePtr StoreFactory::PrefixStore(const ock::smem::StorePtr &base, const std::string &prefix) noexcept
{
    if (base == nullptr) {
        return nullptr;
    }

    return std::make_shared<PrefixConfigStore>(base, prefix);
}
}
}