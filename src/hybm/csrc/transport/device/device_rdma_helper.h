/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef MF_HYBRID_DEVICE_RDMA_HELPER_H
#define MF_HYBRID_DEVICE_RDMA_HELPER_H

#include <netinet/in.h>

#include <cstdint>
#include <string>
#include "mf_net.h"
#include "hybm_types.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
Result ParseDeviceNic(const std::string &nic, uint16_t &port);
Result ParseDeviceNic(const std::string &nic, mf_sockaddr &address);
std::string GenerateDeviceNic(net_addr_t ip, uint16_t port);
}
}
}
}
#endif  // MF_HYBRID_DEVICE_RDMA_HELPER_H
