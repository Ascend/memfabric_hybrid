/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <arpa/inet.h>
#include <cstdlib>
#include <cerrno>
#include <sstream>
#include <regex>
#include <limits>
#include "hybm_logger.h"
#include "mf_string_util.h"
#include "mf_num_util.h"
#include "device_rdma_helper.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
Result ParseDeviceNic(const std::string &nic, uint16_t &port)
{
    if (!StringUtil::String2Uint(nic, port) || port == 0) {
        BM_LOG_ERROR("failed to convert nic : " << nic << " to uint16_t, or port is 0.");
        return BM_INVALID_PARAM;
    }
    return BM_OK;
}

Result ParseDeviceNic(const std::string &nic, mf_sockaddr &address)
{
    static std::regex pattern_ipv4(R"(^[a-zA-Z0-9_]{1,16}://([0-9.]{1,24}):(\d{1,5})$)");
    static std::regex pattern_ipv6(R"(^[a-zA-Z0-9_]{1,16}://\[([0-9a-fA-F:]{1,45})\]:(\d{1,5})$)");
    std::smatch match;
    if (std::regex_search(nic, match, pattern_ipv4)) {
        address.type = IpV4;
    } else if (std::regex_search(nic, match, pattern_ipv6)) {
        address.type = IpV6;
    } else {
        BM_LOG_ERROR("input nic(" << nic << ") not matches.");
        return BM_INVALID_PARAM;
    }

    if (address.type == IpV4) {
        if (inet_aton(match[INDEX_1].str().c_str(), &address.ip.ipv4.sin_addr) == 0) {
            BM_LOG_ERROR("parse ip for nic: " << nic << " failed.");
            return BM_INVALID_PARAM;
        }

        auto caught = match[INDEX_2].str();
        if (!StringUtil::String2Uint(caught, address.ip.ipv4.sin_port)) {
            BM_LOG_ERROR("failed to convert str : " << caught << " to uint16_t, or sin_port is 0.");
            return BM_INVALID_PARAM;
        }

        address.ip.ipv4.sin_family = AF_INET;
    } else if (address.type == IpV6) {
        if (inet_pton(AF_INET6, match[INDEX_1].str().c_str(), &address.ip.ipv6.sin6_addr) != 1) {
            BM_LOG_ERROR("parse ip for nic: " << nic << " failed.");
            return BM_INVALID_PARAM;
        }

        auto caught = match[INDEX_2].str();
        if (!StringUtil::String2Uint(caught, address.ip.ipv6.sin6_port)) {
            BM_LOG_ERROR("failed to convert str : " << caught << " to uint16_t, or sin_port is 0.");
            return BM_INVALID_PARAM;
        }

        address.ip.ipv6.sin6_family = AF_INET6;
    }
    return BM_OK;
}

std::string GenerateDeviceNic(net_addr_t ip, uint16_t port)
{
    std::stringstream ss;
    if (ip.type == IpV4) {
        ss << "tcp://" << inet_ntoa(ip.ip.ipv4) << ":" << port;
    } else {
        char ipv6Str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &ip.ip.ipv6, ipv6Str, INET6_ADDRSTRLEN);
        ss << "tcp6://[" << ipv6Str << "]:" << port;
    }
    return ss.str();
}
}
}
}
}