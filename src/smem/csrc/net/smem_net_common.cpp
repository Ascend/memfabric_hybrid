/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "smem_net_common.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <vector>
#include <map>
#include <regex>
#include "mf_ipv4_validator.h"
#include "mf_str_util.h"

namespace ock {
namespace smem {

const std::string PROTOCOL_TCP = "tcp://";

inline void Split(const std::string &src, const std::string &sep, std::vector<std::string> &out)
{
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
    // 校验输入长度，防止正则表达式栈溢出
    constexpr size_t maxIpLen = 15;
    if (address.size() > maxIpLen) {
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
    return true;
}

Result UrlExtraction::ExtractIpPortFromUrl(const std::string &url)
{
    std::map<std::string, std::string> details;
    auto parser = mf::SocketAddressParserMgr::getInstance().CreateParser(url);
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

Result GetLocalIpWithTarget(const std::string &target, std::string &local, uint32_t &ipv4)
{
    struct ifaddrs *ifaddr;
    char localResultIp[64];
    Result result = SM_ERROR;

    struct in_addr targetIp;
    if (inet_aton(target.c_str(), &targetIp) == 0) {
        SM_LOG_ERROR("target ip address invalid. ");
        return SM_INVALID_PARAM;
    }

    if (getifaddrs(&ifaddr) == -1) {
        SM_LOG_ERROR("get local net interfaces failed: " << errno << ": " << strerror(errno));
        return SM_ERROR;
    }

    for (auto ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if ((ifa->ifa_addr == nullptr) || (ifa->ifa_addr->sa_family != AF_INET) ||
          (ifa->ifa_netmask == nullptr)) {
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
            ipv4 = ntohl(localIp.s_addr);
            local = std::string(localResultIp);
            result = SM_OK;
        }
        break;
    }

    freeifaddrs(ifaddr);
    return result;
}
}
}