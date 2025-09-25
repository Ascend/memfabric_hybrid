/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
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

        if (p->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        if ((p->ifa_flags & IFF_LOOPBACK) != 0) {
            continue;
        }

        if ((p->ifa_flags & IFF_UP) == 0 || (p->ifa_flags & IFF_RUNNING) == 0) {
            continue;
        }

        auto sin = reinterpret_cast<struct sockaddr_in *>(p->ifa_addr);
        addresses.emplace_back(ntohl(sin->sin_addr.s_addr));
        char ip_str[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &sin->sin_addr, ip_str, sizeof(ip_str)) != nullptr) {
            BM_LOG_INFO("find ip address: " << p->ifa_name << " -> " << ip_str);
        }
    }

    freeifaddrs(ifa);
    std::sort(addresses.begin(), addresses.end(), std::less<uint32_t>());
    return addresses;
}
}
}