/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_DEVICE_RDMA_HELPER_H
#define MF_HYBRID_DEVICE_RDMA_HELPER_H

#include <netinet/in.h>

#include <cstdint>
#include <string>

#include "hybm_types.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
Result ParseDeviceNic(const std::string &nic, uint16_t &port);
Result ParseDeviceNic(const std::string &nic, sockaddr_in &address);
std::string GenerateDeviceNic(in_addr ip, uint16_t port);
}
}
}
}
#endif  // MF_HYBRID_DEVICE_RDMA_HELPER_H
