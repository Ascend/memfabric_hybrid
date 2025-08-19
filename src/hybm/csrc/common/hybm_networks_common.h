/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MEM_FABRIC_HYBRID_HYBM_NETWORKS_COMMON_H
#define MEM_FABRIC_HYBRID_HYBM_NETWORKS_COMMON_H

#include <cstdint>
#include <vector>

namespace ock {
namespace mf {
std::vector<uint32_t> NetworkGetIpAddresses() noexcept;
}
}

#endif // MEM_FABRIC_HYBRID_HYBM_NETWORKS_COMMON_H
