/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "host_hcom_helper.h"
#include <regex>
#include <sstream>
#include <arpa/inet.h>
#include <ifaddrs.h>

#include "hybm_logger.h"
#include "mf_num_util.h"

using namespace ock::mf;
using namespace ock::mf::transport::host;

namespace {
const std::regex ipPortPattern(R"(^(tcp://)(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}):(\d{1,5})$)");
const std::regex ipPortMaskPattern(R"(^(tcp://)(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})/(\d{1,2}):(\d{1,5})$)");

const std::string ipv6_common_core =
    R"((?:[0-9a-fA-F]{1,4}(?::[0-9a-fA-F]{1,4}){7})|)"
    R"((?:[0-9a-fA-F]{1,4}(?::[0-9a-fA-F]{1,4}){0,6})?::)"
    R"((?:[0-9a-fA-F]{1,4}(?::[0-9a-fA-F]{1,4}){0,6})?|)"
    R"((?:(?:[0-9a-fA-F]{1,4}(?::[0-9a-fA-F]{1,4}){0,4}:)?)"
    R"((?:25[0-5]|2[0-4]\d|1\d{2}|[1-9]?\d))"
    R"((?:\.(?:25[0-5]|2[0-4]\d|1\d{2}|[1-9]?\d)){3}))";

const std::regex ipv6PortPattern(
    R"(^(tcp6://)\[()" + ipv6_common_core + R"()\]:(\d{1,5})$)"
);

const std::regex ipv6PortMaskPattern(
    R"(^(tcp6://)\[()" + ipv6_common_core + R"()\]/(\d{1,3}):(\d{1,5})$)"
);
}

const int MIN_PORT = 1024;
const int MAX_PORT = 65535;
const int MAX_MASK_VALUE = 32;
const int MAX_MASK_V6_VALUE = 128;

Result HostHcomHelper::AnalysisNic(const std::string &nic, std::string &protocol, std::string &ipStr, int32_t &port)
{
    bool is_ipv6 {false};
    if (nic.find('.') != std::string::npos) {
        is_ipv6 = false;
        if (std::regex_match(nic, ipPortMaskPattern)) {
            return AnalysisNicWithMask(nic, protocol, ipStr, port);
        }
    } else if (nic.find('[') != std::string::npos) {
        is_ipv6 = true;
        if (std::regex_match(nic, ipv6PortMaskPattern)) {
            return AnalysisNicWithMask(nic, protocol, ipStr, port);
        }
    }

    std::smatch match;
    std::regex ip_pattern = is_ipv6 ? ipv6PortPattern : ipPortPattern;
    if (!std::regex_match(nic, match, ip_pattern)) {
        BM_LOG_ERROR("Failed to match nic, nic: " << nic);
        return BM_INVALID_PARAM;
    }
    protocol = match[INDEX_1].str();
    ipStr = match[INDEX_2].str();
    std::string portStr = match[INDEX_3].str();
    port = std::stoi(portStr);
    if (port < MIN_PORT || port > MAX_PORT) {
        BM_LOG_ERROR("Failed to check port, portStr: " << portStr << " nic: " << nic);
        return BM_INVALID_PARAM;
    }
    if (!is_ipv6) {
        in_addr ip{};
        if (inet_aton(ipStr.c_str(), &ip) == 0) {
            BM_LOG_ERROR("Failed to check ip, nic: " << nic << " ipStr: " << ipStr);
            return BM_INVALID_PARAM;
        }
        return BM_OK;
    } else {
        in6_addr ipv6{};
        if (inet_pton(AF_INET6, ipStr.c_str(), &ipv6) != 1) {
            BM_LOG_ERROR("Failed to check ip, nic: " << nic << " ipStr: " << ipStr);
            return BM_INVALID_PARAM;
        }
        return BM_OK;
    }
}

Result HostHcomHelper::AnalysisNicWithMask(const std::string &nic, std::string &protocol,
    std::string &ipStr, int32_t &port)
{
    std::smatch match;
    if (!std::regex_match(nic, match, ipPortMaskPattern) && !std::regex_match(nic, match, ipv6PortMaskPattern)) {
        BM_LOG_ERROR("Failed to match nic, nic: " << nic);
        return BM_INVALID_PARAM;
    }

    protocol = match[INDEX_1].str();
    std::string ip = match[INDEX_2].str();
    std::string maskStr = match[INDEX_3].str();
    std::string portStr = match[INDEX_4].str();

    std::istringstream iss(ipStr);
    std::string token;

    int mask = std::stoi(maskStr);
    if ((ip.find('.') != std::string::npos && (mask < 0 || mask > MAX_MASK_VALUE)) ||
        (ip.find(':') != std::string::npos && (mask < 0 || mask > MAX_MASK_V6_VALUE))) {
        BM_LOG_ERROR("Failed to analysis nic mask is invalid: " << nic);
        return BM_INVALID_PARAM;
    }

    port = std::stoi(portStr);
    if (port < MIN_PORT || port > MAX_PORT) {
        BM_LOG_ERROR("Failed to analysis nic port is invalid: " << nic);
        return BM_INVALID_PARAM;
    }

    return SelectLocalIpByIpMask(ip, mask, ipStr); // 成功
}

static Result SelectLocalIpByIpMaskWhenIpv6(const std::string &ipStr, const int32_t &mask,
                                            std::string &localIp, bool &found, struct ifaddrs* ifAddsPtr)
{
    // ipv6
    struct in6_addr targetNetV6;
    if (inet_pton(AF_INET6, ipStr.c_str(), &targetNetV6) <= 0) {
        BM_LOG_ERROR("Invalid ipv6: " << ipStr << " mask: " << mask);
        return BM_INVALID_PARAM;
    }

    struct in6_addr netMaskV6 {};
    struct in6_addr targetNetworkV6 {};
    for (int i = 0; i < mask; i++) {
        netMaskV6.s6_addr[i / 8] |= (1 << (7 - (i % 8)));
    }
    for (int i = 0; i < 16; i++) {
        targetNetworkV6.s6_addr[i] = targetNetV6.s6_addr[i] & netMaskV6.s6_addr[i];
    }

    char localIpTemp[INET_ADDRSTRLEN];
    for (struct ifaddrs* ifa = ifAddsPtr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET6) {
            continue;
        }
        auto *addr = reinterpret_cast<struct sockaddr_in6 *>(ifa->ifa_addr);
        struct in6_addr localIpAddr = addr->sin6_addr;
        struct in6_addr localNetworkV6;
        for (int i = 0; i < 16; i++) {
            localNetworkV6.s6_addr[i] = localIpAddr.s6_addr[i] & netMaskV6.s6_addr[i];
        }
        if (memcmp(&localNetworkV6, &targetNetworkV6, sizeof(struct in6_addr)) == 0) {
            inet_ntop(AF_INET6, &localIpAddr, localIpTemp, INET6_ADDRSTRLEN);
            localIp = localIpTemp;
            found = true;
            BM_LOG_DEBUG("Success to find ip: " << localIp);
            break;
        }
    }
    return BM_OK;
}

Result HostHcomHelper::SelectLocalIpByIpMask(const std::string &ipStr, const int32_t &mask, std::string &localIp)
{
    bool found = false;
    struct ifaddrs* ifAddsPtr = nullptr;
    if (getifaddrs(&ifAddsPtr) != 0) {
        BM_LOG_ERROR("Failed to get local ip list, ip: " << ipStr << " mask: " << mask);
        return BM_ERROR;
    }
    if (ipStr.find('.') != std::string::npos) {
        // ipv4
        in_addr_t targetNet = inet_addr(ipStr.c_str());
        if (targetNet == INADDR_NONE) {
            BM_LOG_ERROR("Invalid ip: " << ipStr << " mask: " << mask);
            return BM_INVALID_PARAM;
        }

        uint32_t netMask = htonl((0xFFFFFFFF << (MAX_MASK_VALUE - mask)) & 0xFFFFFFFF);
        uint32_t targetNetwork = targetNet & netMask;

        for (struct ifaddrs* ifa = ifAddsPtr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
                continue;
            }
            auto *addr = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
            in_addr_t localIpAddr = addr->sin_addr.s_addr;
            uint32_t localNetwork = localIpAddr & netMask;
            if (localNetwork == targetNetwork) {
                localIp = inet_ntoa(addr->sin_addr);
                found = true;
                BM_LOG_DEBUG("Success to find ip: " << localIp);
                break;
            }
        }
    } else {
        // ipv6
        Result ret = SelectLocalIpByIpMaskWhenIpv6(ipStr, mask, localIp, found, ifAddsPtr);
        if (ret != BM_OK) {
            return ret;
        }
    }

    freeifaddrs(ifAddsPtr);
    return found ? BM_OK : BM_ERROR;
}