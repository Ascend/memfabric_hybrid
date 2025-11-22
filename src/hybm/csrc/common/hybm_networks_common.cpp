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

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include "hybm_functions.h"
#include "hybm_logger.h"
#include "hybm_networks_common.h"

namespace ock {
namespace mf {
namespace {
std::string GetDefaultRouteNetwork()
{
    std::string routeFileName{"/proc/net/route"};
    std::ifstream input(routeFileName);
    if (!input.is_open()) {
        BM_LOG_ERROR("open route file failed: " << strerror(errno));
        return "";
    }

    std::string ifname;
    uint32_t destination;
    uint32_t temp;
    uint32_t mask;
    std::string line;
    std::getline(input, line); // skip header line
    while (std::getline(input, line)) {
        std::stringstream ss{line};
        ss >> ifname >> std::hex; // Iface
        ss >> destination;        // Destination
        ss >> temp;               // Gateway
        ss >> temp;               // Flags
        ss >> temp;               // RefCnt
        ss >> temp;               // Use
        ss >> temp;               // Metric
        ss >> mask;               // Mask
        if (destination == 0U && mask == 0U) {
            BM_LOG_INFO("default route network : " << ifname);
            return ifname;
        }
    }
    return "";
}
} // namespace
std::vector<uint32_t> NetworkGetIpAddresses() noexcept
{
    std::vector<uint32_t> addresses;
    struct ifaddrs *ifa;
    struct ifaddrs *p;
    if (getifaddrs(&ifa) < 0) {
        BM_LOG_ERROR("getifaddrs() failed: " << errno << " : " << SafeStrError(errno));
        return addresses;
    }

    uint32_t routeIp = 0;
    auto routeName = GetDefaultRouteNetwork();
    for (p = ifa; p != nullptr; p = p->ifa_next) {
        if (p->ifa_addr == nullptr) {
            continue;
        }

        if (p->ifa_addr->sa_family != AF_INET && p->ifa_addr->sa_family != AF_INET6) {
            continue;
        }

        if ((p->ifa_flags & IFF_LOOPBACK) != 0) {
            continue;
        }

        if ((p->ifa_flags & IFF_UP) == 0 || (p->ifa_flags & IFF_RUNNING) == 0) {
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
            ip_address = addr_str;

            // IPv6
            const uint64_t* latter_ptr = reinterpret_cast<const uint64_t*>(&(sin->sin6_addr.s6_addr[8]));
            uint64_t latter_id = *latter_ptr;
            std::hash<uint64_t> hasher;
            uint32_t ipv6_derived_id = static_cast<uint32_t>(hasher(latter_id));
            addresses.emplace_back(ipv6_derived_id);
        }

        BM_LOG_INFO("find ip address: " << p->ifa_name << " -> " << ip_address);
    }

    freeifaddrs(ifa);
    std::sort(addresses.begin(), addresses.end(), std::less<uint32_t>());
    if (routeIp != 0) {
        addresses.insert(addresses.begin(), routeIp);
    }
    return addresses;
}
} // namespace mf
} // namespace ock