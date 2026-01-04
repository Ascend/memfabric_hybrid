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
#include "host_hcom_helper.h"
#include <regex>
#include <sstream>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include "mf_str_util.h"
#include "hybm_logger.h"

using namespace ock::mf;
using namespace ock::mf::transport::host;

constexpr int MIN_VALID_PORT = 1024;
constexpr int MAX_VALID_PORT = 65535;
constexpr int MIN_VALID_MASK = 0;
constexpr int MAX_VALID_MASK = 32;
constexpr int PROT_MATCH_NUM = 1;
constexpr int IP_MATCH_NUM = 2;
constexpr int PORT_MATCH_NUM = 3;

static Result AnalysisUBNic(const std::string &nic, std::string &protocol, std::string &ipStr, uint32_t &port)
{
    static constexpr uint32_t UBC_START_PORT = 512UL;
    auto input = StrUtil::StrTrim(nic);
    if (input.size() < strlen(UBC_PROTOCOL_PREFIX)) {
        BM_LOG_ERROR("Failed to match nic, nic: " << input);
        return BM_INVALID_PARAM;
    }
    protocol.clear();
    ipStr.clear();
    port = 0;
    std::string eid = input.substr(strlen(UBC_PROTOCOL_PREFIX));
    std::regex pattern(R"(([0-9a-fA-F]{4}:){7}[0-9a-fA-F]{4})");
    if (std::regex_match(eid, pattern)) {
        ipStr = eid;
        protocol = UBC_PROTOCOL_PREFIX;
        port = UBC_START_PORT;
    } else {
        BM_LOG_ERROR("Failed to match nic, nic: " << input);
        return BM_INVALID_PARAM;
    }
    return BM_OK;
}

Result HostHcomHelper::AnalysisNic(const std::string &nic, std::string &protocol, std::string &ipStr, uint32_t &port)
{
    static const std::regex ipPortPattern(R"(^(tcp://)(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}):(\d{1,5})$)");
    static const std::regex ipPortMaskPattern(R"(^(tcp://)(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})/(\d{1,2}):(\d{1,5})$)");
    if (StrUtil::StartWith(nic, UBC_PROTOCOL_PREFIX)) {
        return AnalysisUBNic(nic, protocol, ipStr, port);
    }
    if (std::regex_match(nic, ipPortMaskPattern)) {
        return AnalysisNicWithMask(nic, protocol, ipStr, port);
    }

    std::smatch match;
    if (!std::regex_match(nic, match, ipPortPattern)) {
        BM_LOG_ERROR("Failed to match nic, nic: " << nic);
        return BM_INVALID_PARAM;
    }
    protocol = match[PROT_MATCH_NUM].str();
    ipStr = match[IP_MATCH_NUM].str();
    std::string portStr = match[PORT_MATCH_NUM].str();
    auto ret = StrUtil::String2Uint<uint32_t>(portStr, port);
    if (!ret || port < MIN_VALID_PORT || port > MAX_VALID_PORT) {
        BM_LOG_ERROR("Failed to check port, portStr: " << portStr << " nic: " << nic);
        return BM_INVALID_PARAM;
    }
    in_addr ip{};
    if (inet_aton(ipStr.c_str(), &ip) == 0) {
        BM_LOG_ERROR("Failed to check ip, nic: " << nic << " ipStr: " << ipStr);
        return BM_INVALID_PARAM;
    }
    return BM_OK;
}

Result HostHcomHelper::AnalysisNicWithMask(const std::string &nic,
    std::string &protocol, std::string &ipStr, uint32_t &port)
{
    static const std::regex ipPortMaskPattern(R"(^(tcp://)(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})/(\d{1,2}):(\d{1,5})$)");
    std::smatch match;
    if (!std::regex_match(nic, match, ipPortMaskPattern)) {
        BM_LOG_ERROR("Failed to match nic, nic: " << nic);
        return BM_INVALID_PARAM;
    }

    protocol = match[1].str();
    std::string ip = match[2].str();
    std::string maskStr = match[3].str();
    std::string portStr = match[4].str();

    std::istringstream iss(ipStr);
    std::string token;

    int mask = MIN_VALID_MASK;
    auto ret = StrUtil::String2Int<int>(maskStr, mask);
    if (!ret || mask < MIN_VALID_MASK || mask > MAX_VALID_MASK) {
        BM_LOG_ERROR("Failed to analysis nic mask is invalid: " << nic);
        return BM_INVALID_PARAM;
    }

    ret = StrUtil::String2Uint<uint32_t>(portStr, port);
    if (!ret || port < MIN_VALID_PORT || port > MAX_VALID_PORT) {
        BM_LOG_ERROR("Failed to analysis nic port is invalid: " << nic);
        return BM_INVALID_PARAM;
    }

    return SelectLocalIpByIpMask(ip, mask, ipStr); // 成功
}

Result HostHcomHelper::SelectLocalIpByIpMask(const std::string &ipStr, const int32_t &mask, std::string &localIp)
{
    in_addr_t targetNet = inet_addr(ipStr.c_str());
    if (targetNet == INADDR_NONE) {
        BM_LOG_ERROR("Invalid ip: " << ipStr << " mask: " << mask);
        return BM_INVALID_PARAM;
    }

    uint32_t netMask = htonl((0xFFFFFFFF << (32 - mask)) & 0xFFFFFFFF);
    uint32_t targetNetwork = targetNet & netMask;

    struct ifaddrs* ifAddsPtr = nullptr;
    if (getifaddrs(&ifAddsPtr) != 0) {
        BM_LOG_ERROR("Failed to get local ip list, ip: " << ipStr << " mask: " << mask);
        return BM_ERROR;
    }

    bool found = false;
    for (struct ifaddrs* ifa = ifAddsPtr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        auto *addr = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
        in_addr_t localIpAddr = addr->sin_addr.s_addr;
        uint32_t localNetwork = localIpAddr & netMask;
        if (localNetwork == targetNetwork) {
            char str[INET_ADDRSTRLEN];
            auto ret = inet_ntop(AF_INET, &addr->sin_addr, str, INET_ADDRSTRLEN);
            if (ret == nullptr) {
                BM_LOG_ERROR("Failed to get local ip ip.");
                freeifaddrs(ifAddsPtr);
                return BM_ERROR;
            }
            localIp = str;
            found = true;
            BM_LOG_DEBUG("Success to find ip: " << localIp);
            break;
        }
    }

    freeifaddrs(ifAddsPtr);
    return found ? BM_OK : BM_ERROR;
}