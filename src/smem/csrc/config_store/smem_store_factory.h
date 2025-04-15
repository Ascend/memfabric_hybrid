/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef SMEM_SMEM_STORE_FACTORY_H
#define SMEM_SMEM_STORE_FACTORY_H

#include "smem_config_store.h"

namespace ock {
namespace smem {

class StoreFactory {
public:
    static StorePtr CreateStore(const std::string &ip, uint16_t port, bool isServer, int32_t rankId = 0) noexcept;
    static StorePtr PrefixStore(const StorePtr &base, const std::string &prefix) noexcept;
};

}
}

#endif  //SMEM_SMEM_STORE_FACTORY_H
