/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <ifaddrs.h>

#include <algorithm>
#include <cerrno>
#include <cstring>

#include "hybm_logger.h"
#include "hybm_networks_common.h"

namespace ock {
namespace mf {
std::vector<uint32_t> NetworkGetIpAddresses() noexcept
{
    std::vector<uint32_t> addresses;
    struct ifaddrs *ifa;
    struct ifaddrs *p;
    if (getifaddrs(&ifa) < 0) {
        BM_LOG_ERROR("getifaddrs() failed: " << errno << " : " << strerror(errno));
        return addresses;
    }

    for (p = ifa; p != nullptr; p = p->ifa_next) {
        if (p->ifa_addr == nullptr) {
            continue;
        }

        if (p->ifa_addr->sa_family != AF_INET && p->ifa_addr->sa_family != AF_INET6) {
            continue;
        }

        if (p->ifa_flags & IFF_LOOPBACK) {
            continue;
        }

        std::string ip_address {};
        if (p->ifa_addr->sa_family == AF_INET) {
            auto sin = reinterpret_cast<struct sockaddr_in *>(p->ifa_addr);
            addresses.emplace_back(ntohl(sin->sin_addr.s_addr));
            ip_address = inet_ntoa(sin->sin_addr);
        } else if (p->ifa_addr->sa_family == AF_INET6) {
            auto sin = reinterpret_cast<struct sockaddr_in6 *>(p->ifa_addr);
            char addr_str[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &(sin->sin6_addr), addr_str, INET6_ADDRSTRLEN);
            addresses.emplace_back(addr_str);
            ip_address = addr_str;
        }

        BM_LOG_INFO("find ip address: " << p->ifa_name << " -> " << ip_address);
    }

    freeifaddrs(ifa);
    std::sort(addresses.begin(), addresses.end(), std::less<uint32_t>());
    return addresses;
}
}
}
