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
#include <ifaddrs.h>
#include <net/if.h>

#include <vector>
#include <map>
#include <regex>
#include "smem_net_common.h"

namespace ock {
namespace smem {

const std::string PROTOCOL_TCP4 = "tcp://";
const std::string PROTOCOL_TCP6 = "tcp6://";
enum class PROTOCOLTYPE {
    PROTOCOLV4,
    PROTOCOLV6,
    IPNONE,
};
static PROTOCOLTYPE type = PROTOCOLTYPE::IPNONE;

inline void Split(const std::string &src, const std::string &sep, std::vector<std::string> &out)
{
    int COUNT = 1;
    std::string::size_type pos1 = 0;
    std::string::size_type pos2 = src.find_last_of(sep);

    std::string tmpStr;
    if (src[0] != '[') {
        while (pos2 != std::string::npos) {
            tmpStr = src.substr(pos1, pos2 - pos1);
            out.emplace_back(tmpStr);
            pos1 = pos2 + sep.size();
            pos2 = src.find(sep, pos1);
        }

        if (pos1 != src.length()) {
            tmpStr = src.substr(pos1);
            out.emplace_back(tmpStr);
        }
    } else {
        if (std::count(src.begin(), src.end(), sep[0]) > COUNT) {
            const int diff = 2;
            tmpStr = src.substr(pos1 + 1, pos2 - pos1 - diff);
            out.emplace_back(tmpStr);
            pos1 = pos2 + sep.size();
            pos2 = src.find(sep, pos1);
            if (pos1 != src.length()) {
                tmpStr = src.substr(pos1);
                out.emplace_back(tmpStr);
            }
        }
    }
}

bool IsValidIp(const std::string &address)
{
    // 校验输入长度，防止正则表达式栈溢出
    if (type == PROTOCOLTYPE::PROTOCOLV4) {
        constexpr size_t maxIpLenV4 = 15;
        if (address.size() > maxIpLenV4) {
            return false;
        }
        std::regex ipV4Pattern("^(?:(?:25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)($|(?!\\.$)\\.)){4}$");
        std::regex zeroPattern("^0+\\.0+\\.0+\\.0+$");
        if (std::regex_match(address, zeroPattern)) {
            return false;
        }

        if (!std::regex_match(address, ipV4Pattern)) {
            return false;
        }
    } else if (type == PROTOCOLTYPE::PROTOCOLV6) {
        constexpr size_t maxIpLenV6 = 39;
        if (address.size() > maxIpLenV6) {
            return false;
        }
        const std::string ipv6_common_core =
            R"((?:[0-9a-fA-F]{1,4}(?::[0-9a-fA-F]{1,4}){7})|)"
            R"((?:[0-9a-fA-F]{1,4}(?::[0-9a-fA-F]{1,4}){0,6})?::)"
            R"((?:[0-9a-fA-F]{1,4}(?::[0-9a-fA-F]{1,4}){0,6})?|)"
            R"((?:(?:[0-9a-fA-F]{1,4}(?::[0-9a-fA-F]{1,4}){0,4}:)?)"
            R"((?:25[0-5]|2[0-4]\d|1\d{2}|[1-9]?\d))"
            R"((?:\.(?:25[0-5]|2[0-4]\d|1\d{2}|[1-9]?\d)){3}))";

        const std::regex ipV6Pattern(
            "^" + ipv6_common_core + "$"
        );

        if (!std::regex_match(address, ipV6Pattern)) {
            return false;
        }
    } else {
        return false;
    }
    return true;
}

Result ExtractTcpURL(const std::string &url, std::map<std::string, std::string> &details)
{
    /* remove tcp:// or tcp6:// */
    std::string tmpUrl;
    if (url.compare(0, PROTOCOL_TCP6.size(), PROTOCOL_TCP6) == 0) {
        type = PROTOCOLTYPE::PROTOCOLV6;
        tmpUrl = url.substr(PROTOCOL_TCP6.length(), url.length() - PROTOCOL_TCP6.length());
    } else if (url.compare(0, PROTOCOL_TCP4.size(), PROTOCOL_TCP4) == 0) {
        type = PROTOCOLTYPE::PROTOCOLV4;
        tmpUrl = url.substr(PROTOCOL_TCP4.length(), url.length() - PROTOCOL_TCP4.length());
    } else {
        return SM_INVALID_PARAM;
    }

    /* split */
    std::vector<std::string> splits;
    Split(tmpUrl, ":", splits);
    if (splits.size() != UN2) {
        return SM_INVALID_PARAM;
    }

    /* assign port */
    details["port"] = splits[1];
    if (splits[0].find('/') == std::string::npos) {
        /* assign ip */
        details["ip"] = splits[0];
        return SM_OK;
    }

    /* get ip mask */
    tmpUrl = splits[0];
    splits.clear();
    Split(tmpUrl, "/", splits);
    if (splits.size() != UN2) {
        return SM_INVALID_PARAM;
    }

    details["ip"] = splits[0];
    details["mask"] = splits[1];
    return SM_OK;
}

Result UrlExtraction::ExtractIpPortFromUrl(const std::string &url)
{
    std::map<std::string, std::string> details;
    /* extract to vector */
    auto result = ExtractTcpURL(url, details);
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(result, "Failed to extract url , which is invalid");

    auto iterMask = details.find("mask");
    std::string ipStr = details["ip"];
    std::string portStr = details["port"];

    /* covert port */
    long tmpPort = 0;
    if (!StrToLong(portStr, tmpPort)) {
        SM_LOG_ERROR("Invalid url. ");
        return SM_INVALID_PARAM;
    }

    if (!IsValidIp(ipStr) || tmpPort <= N1024 || tmpPort > UINT16_MAX) {
        SM_LOG_ERROR("Invalid url. ");
        return SM_INVALID_PARAM;
    }

    /* set ip and port */
    ip = ipStr;
    port = tmpPort;
    return SM_OK;
}

static Result GetLocalIpWithTargetWhenIpv6(struct in6_addr &localIp, char *localResultIp, int size,
                                           mf_ip_addr &ipaddr, std::string &local)
{
    Result result = SM_ERROR;
    if (inet_ntop(AF_INET6, &localIp, localResultIp, size) == nullptr) {
        SM_LOG_ERROR("convert local ipv6 to string failed. ");
        result = SM_ERROR;
    } else {
        ipaddr.type = IpV6;
        std::copy(std::begin(localIp.s6_addr), std::end(localIp.s6_addr), std::begin(ipaddr.addr.addrv6));
        local = std::string(localResultIp);
        result = SM_OK;
    }
    return result;
}

static Result GetLocalIpWithTargetWhenIpv4(struct in_addr &localIp, char *localResultIp, int size,
                                           mf_ip_addr &ipaddr, std::string &local)
{
    Result result = SM_ERROR;
    if (inet_ntop(AF_INET, &localIp, localResultIp, size) == nullptr) {
        SM_LOG_ERROR("convert local ipv4 to string failed. ");
        result = SM_ERROR;
    } else {
        ipaddr.type = IpV4;
        ipaddr.addr.addrv4 = ntohl(localIp.s_addr);
        local = std::string(localResultIp);
        result = SM_OK;
    }
    return result;
}

static Result DetermineTargetIpType(const std::string &target, struct in_addr &targetIpV4,
                                    struct in6_addr &targetIpV6, bool &isTargetV6)
{
    if (inet_pton(AF_INET, target.c_str(), &targetIpV4) == 1) {
        isTargetV6 = false;
    } else if (inet_pton(AF_INET6, target.c_str(), &targetIpV6) == 1) {
        isTargetV6 = true;
    } else {
        SM_LOG_ERROR("target ip address invalid.");
        return SM_INVALID_PARAM;
    }
    return SM_OK;
}

static bool IsSameNetwork(const struct in6_addr &localIp, const struct in6_addr &localMask,
                          const struct in6_addr &targetIp)
{
    constexpr int SIZE = 16;
    for (int i = 0; i < SIZE; i++) {
        if ((localIp.s6_addr[i] & localMask.s6_addr[i]) != (targetIp.s6_addr[i] & localMask.s6_addr[i])) {
            return false;
        }
    }
    return true;
}

Result GetLocalIpWithTarget(const std::string &target, std::string &local, mf_ip_addr &ipaddr)
{
    struct ifaddrs *ifaddr;
    const int SIZE = 64;
    char localResultIp[SIZE];
    Result result = SM_ERROR;

    bool isTargetV6 = false;
    struct in_addr targetIpV4;
    struct in6_addr targetIpV6;
    if (DetermineTargetIpType(target, targetIpV4, targetIpV6, isTargetV6) != SM_OK) {
        return SM_INVALID_PARAM;
    }

    if (getifaddrs(&ifaddr) == -1) {
        SM_LOG_ERROR("get local net interfaces failed: " << errno << ": " << strerror(errno));
        return SM_ERROR;
    }

    for (auto ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if ((ifa->ifa_addr == nullptr) || ((ifa->ifa_addr->sa_family != AF_INET) &&
            (ifa->ifa_addr->sa_family != AF_INET6)) || (ifa->ifa_netmask == nullptr)) {
            continue;
        }

        if (!isTargetV6 && ifa->ifa_addr->sa_family == AF_INET) {
            auto localIp = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr)->sin_addr;
            auto localMask = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_netmask)->sin_addr;
            if ((localIp.s_addr & localMask.s_addr) != (targetIpV4.s_addr & localMask.s_addr)) {
                continue;
            }
            result = GetLocalIpWithTargetWhenIpv4(localIp, localResultIp, SIZE, ipaddr, local);
            break;
        } else if (isTargetV6 && ifa->ifa_addr->sa_family == AF_INET6) {
            auto localIp = reinterpret_cast<struct sockaddr_in6 *>(ifa->ifa_addr)->sin6_addr;
            auto localMask = reinterpret_cast<struct sockaddr_in6 *>(ifa->ifa_netmask)->sin6_addr;

            if (!IsSameNetwork(localIp, localMask, targetIpV6)) {
                continue;
            }
            result = GetLocalIpWithTargetWhenIpv6(localIp, localResultIp, SIZE, ipaddr, local);
            break;
        }
    }

    freeifaddrs(ifaddr);
    return result;
}
}
}
