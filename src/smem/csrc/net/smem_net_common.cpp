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

#include "smem_net_common.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <ifaddrs.h>
#include <net/if.h>
#include <vector>
#include <map>
#include <regex>
#include "mf_ipv4_validator.h"
#include "network_endpoint_util.h"
#include "mf_str_util.h"

namespace ock {
namespace smem {

const std::string PROTOCOL_TCP = "tcp://";

namespace {

constexpr char ETCD_URL_PREFIX[] = "etcd://";
constexpr char URL_FRAGMENT_DELIMITER = '#';
constexpr char CLUSTER_ID_HYPHEN = '-';
constexpr char CLUSTER_ID_UNDERSCORE = '_';
constexpr size_t ETCD_URL_PREFIX_LEN = sizeof(ETCD_URL_PREFIX) - 1;

inline bool IsValidClusterIdCharacter(char ch)
{
    const unsigned char clusterChar = static_cast<unsigned char>(ch);
    return std::isalnum(clusterChar) != 0 || ch == CLUSTER_ID_HYPHEN || ch == CLUSTER_ID_UNDERSCORE;
}

Result StripEtcdClusterFragment(const std::string &url, std::string &sanitizedUrl)
{
    sanitizedUrl = url;

    const size_t fragmentPos = url.find(URL_FRAGMENT_DELIMITER);
    if (fragmentPos == std::string::npos) {
        return SM_OK;
    }

    if (url.find(URL_FRAGMENT_DELIMITER, fragmentPos + 1) != std::string::npos) {
        SM_LOG_ERROR("invalid store url: multiple cluster fragments, url: " << url);
        return SM_INVALID_PARAM;
    }

    if (url.compare(0, ETCD_URL_PREFIX_LEN, ETCD_URL_PREFIX) != 0) {
        SM_LOG_ERROR("invalid store url: cluster fragment is only supported for etcd, url: " << url);
        return SM_INVALID_PARAM;
    }

    if (fragmentPos == url.size() - 1) {
        SM_LOG_ERROR("invalid store url: cluster id is empty, url: " << url);
        return SM_INVALID_PARAM;
    }

    const std::string clusterId = url.substr(fragmentPos + 1);
    const bool clusterIdValid = std::all_of(clusterId.begin(), clusterId.end(), IsValidClusterIdCharacter);
    if (!clusterIdValid) {
        SM_LOG_ERROR("invalid store url: cluster id contains unsupported characters, url: " << url);
        return SM_INVALID_PARAM;
    }

    sanitizedUrl = url.substr(0, fragmentPos);
    return SM_OK;
}

} // namespace

inline void Split(const std::string &src, const std::string &sep, std::vector<std::string> &out)
{
    if (sep.empty()) {
        out.emplace_back(src);
        return;
    }
    std::string::size_type pos1 = 0;
    std::string::size_type pos2 = src.find(sep);

    std::string tmpStr;
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
}

inline bool IsValidIpV4(const std::string &address)
{
    static const std::regex ipV4Pattern("^(?:(?:25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)($|(?!\\.$)\\.)){4}$");
    static const std::regex zeroPattern("^0+\\.0+\\.0+\\.0+$");
    // 校验输入长度，防止正则表达式栈溢出
    constexpr size_t maxIpLen = 15;
    if (address.size() > maxIpLen) {
        return false;
    }
    if (std::regex_match(address, zeroPattern)) {
        return false;
    }

    if (!std::regex_match(address, ipV4Pattern)) {
        return false;
    }
    return true;
}

Result UrlExtraction::ExtractIpPortFromUrl(const std::string &url)
{
    std::string sanitizedUrl;
    Result stripResult = StripEtcdClusterFragment(url, sanitizedUrl);
    SM_ASSERT_RETURN(stripResult == SM_OK, stripResult);

    auto tcpUrl = sanitizedUrl;
    NetworkEndpointUtil::ConvertToTcpUrl(tcpUrl);
    auto parser = mf::SocketAddressParserMgr::getInstance().CreateParser(tcpUrl);
    SM_ASSERT_RETURN(parser != nullptr, SM_ERROR);

    std::string ipStr = parser->GetIp();
    std::string portStr = std::to_string(parser->GetPort());

    /* covert port */
    long tmpPort = 0;
    if (!mf::StrUtil::String2Int<long>(portStr, tmpPort)) {
        SM_LOG_ERROR("Invalid url. ");
        return SM_INVALID_PARAM;
    }

    if (!parser->IsIpv6()) {
        if (!IsValidIpV4(ipStr) || tmpPort <= N1024 || tmpPort > UINT16_MAX) {
            SM_LOG_ERROR("Invalid url. ");
            return SM_INVALID_PARAM;
        }
    }

    /* set ip and port */
    ip = ipStr;
    port = tmpPort;
    return SM_OK;
}

static Result GetLocalIpV6WithTarget(const std::string &target, std::string &local)
{
    struct ifaddrs *ifaddr;
    char localResultIp[INET6_ADDRSTRLEN];
    Result result = SM_ERROR;
    struct in6_addr targetIp;
    if (inet_pton(AF_INET6, target.c_str(), &targetIp) != 1) {
        SM_LOG_ERROR("target IPv6 address invalid. " << target);
        return SM_INVALID_PARAM;
    }
    if (getifaddrs(&ifaddr) == -1) {
        SM_LOG_ERROR("get local net interfaces failed: " << errno << ": " << strerror(errno));
        return SM_ERROR;
    }
    for (auto ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if ((ifa->ifa_addr == nullptr) || (ifa->ifa_addr->sa_family != AF_INET6) || (ifa->ifa_netmask == nullptr)) {
            continue;
        }
        auto localIp = reinterpret_cast<struct sockaddr_in6 *>(ifa->ifa_addr)->sin6_addr;
        auto localMask = reinterpret_cast<struct sockaddr_in6 *>(ifa->ifa_netmask)->sin6_addr;
        bool sameSubnet = true;
        for (int i = 0; i < N16; ++i) {
            uint8_t localByte = localIp.s6_addr[i] & localMask.s6_addr[i];
            uint8_t targetByte = targetIp.s6_addr[i] & localMask.s6_addr[i];
            if (localByte != targetByte) {
                sameSubnet = false;
                break;
            }
        }
        if (!sameSubnet) {
            continue;
        }
        if (inet_ntop(AF_INET6, &localIp, localResultIp, sizeof(localResultIp)) == nullptr) {
            SM_LOG_ERROR("convert local IPv6 to string failed: " << errno << ": " << strerror(errno));
            result = SM_ERROR;
        } else {
            local = std::string(localResultIp);
            result = SM_OK;
        }
        break;
    }
    freeifaddrs(ifaddr);
    return result;
}

Result GetLocalIpWithTarget(const std::string &target, std::string &local)
{
    if (!IsValidIpV4(target)) {
        return GetLocalIpV6WithTarget(target, local);
    }
    struct ifaddrs *ifaddr;
    char localResultIp[64];
    Result result = SM_ERROR;

    struct in_addr targetIp;
    if (inet_aton(target.c_str(), &targetIp) == 0) {
        SM_LOG_ERROR("target ip address invalid. " << target);
        return SM_INVALID_PARAM;
    }

    if (getifaddrs(&ifaddr) == -1) {
        SM_LOG_ERROR("get local net interfaces failed: " << errno << ": " << strerror(errno));
        return SM_ERROR;
    }

    for (auto ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if ((ifa->ifa_addr == nullptr) || (ifa->ifa_addr->sa_family != AF_INET) || (ifa->ifa_netmask == nullptr)) {
            continue;
        }

        auto localIp = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr)->sin_addr;
        auto localMask = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_netmask)->sin_addr;
        if ((localIp.s_addr & localMask.s_addr) != (targetIp.s_addr & localMask.s_addr)) {
            continue;
        }

        if (inet_ntop(AF_INET, &localIp, localResultIp, sizeof(localResultIp)) == nullptr) {
            SM_LOG_ERROR("convert local ip to string failed. ");
            result = SM_ERROR;
        } else {
            local = std::string(localResultIp);
            result = SM_OK;
        }
        break;
    }

    freeifaddrs(ifaddr);
    return result;
}
} // namespace smem
} // namespace ock
