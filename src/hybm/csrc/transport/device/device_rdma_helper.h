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
} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock
#endif // MF_HYBRID_DEVICE_RDMA_HELPER_H