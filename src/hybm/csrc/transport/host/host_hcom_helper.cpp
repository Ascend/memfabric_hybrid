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
#include <arpa/inet.h>
#include <ifaddrs.h>
#include "mf_ipv4_validator.h"
#include "mf_str_util.h"
#include "hybm_logger.h"

using namespace ock::mf;
using namespace ock::mf::transport::host;

constexpr int MIN_VALID_PORT = 1024;
constexpr int MAX_VALID_PORT = 65535;
constexpr int MIN_VALID_MASK = 0;
constexpr int MAX_VALID_MASK = 32;

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
    if (ock::mf::NetValidator::IsValidUbcEid(eid)) {
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
    if (StrUtil::StartWith(nic, UBC_PROTOCOL_PREFIX)) {
        return AnalysisUBNic(nic, protocol, ipStr, port);
    }
    // Parse tcp://<ip>:<port> or tcp://<ip>/<mask>:<port>
    std::string mask;
    std::string portStr;
    if (!ock::mf::NetValidator::ParseNicUrl(nic, protocol, ipStr, mask, portStr)) {
        BM_LOG_ERROR("Failed to parse nic, nic: " << nic);
        return BM_INVALID_PARAM;
    }
    if (protocol != "tcp://") {
        BM_LOG_ERROR("Failed to parse nic, nic: " << nic);
        return BM_INVALID_PARAM;
    }
    if (!mask.empty()) {
        return AnalysisNicWithMask(nic, protocol, ipStr, port);
    }
    // Validate port
    auto ret = StrUtil::String2Uint<uint32_t>(portStr, port);
    if (!ret || port < MIN_VALID_PORT || port > MAX_VALID_PORT) {
        BM_LOG_ERROR("Failed to check port, portStr: " << portStr << " nic: " << nic);
        return BM_INVALID_PARAM;
    }
    // Validate IP with strict checking
    if (!ock::mf::NetValidator::IsValidIpV4Strict(ipStr)) {
        BM_LOG_ERROR("Failed to check ip, nic: " << nic << " ipStr: " << ipStr);
        return BM_INVALID_PARAM;
    }
    return BM_OK;
}

Result HostHcomHelper::AnalysisNicWithMask(const std::string &nic, std::string &protocol, std::string &ipStr,
                                           uint32_t &port)
{
    std::string mask;
    std::string portStr;
    if (!ock::mf::NetValidator::ParseNicUrl(nic, protocol, ipStr, mask, portStr) || mask.empty()) {
        BM_LOG_ERROR("Failed to parse nic with mask, nic: " << nic);
        return BM_INVALID_PARAM;
    }
    if (protocol != "tcp://") {
        BM_LOG_ERROR("Failed to parse nic with mask, nic: " << nic);
        return BM_INVALID_PARAM;
    }

    int maskVal = MIN_VALID_MASK;
    auto ret = StrUtil::String2Int<int>(mask, maskVal);
    if (!ret || maskVal < MIN_VALID_MASK || maskVal > MAX_VALID_MASK) {
        BM_LOG_ERROR("Failed to analysis nic mask is invalid: " << nic);
        return BM_INVALID_PARAM;
    }
    ret = StrUtil::String2Uint<uint32_t>(portStr, port);
    if (!ret || port < MIN_VALID_PORT || port > MAX_VALID_PORT) {
        BM_LOG_ERROR("Failed to analysis nic port is invalid: " << nic);
        return BM_INVALID_PARAM;
    }
    if (!ock::mf::NetValidator::IsValidIpV4Strict(ipStr)) {
        BM_LOG_ERROR("Failed to analysis nic ip is invalid: " << nic);
        return BM_INVALID_PARAM;
    }
    return SelectLocalIpByIpMask(ipStr, maskVal, ipStr);
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

    struct ifaddrs *ifAddsPtr = nullptr;
    if (getifaddrs(&ifAddsPtr) != 0) {
        BM_LOG_ERROR("Failed to get local ip list, ip: " << ipStr << " mask: " << mask);
        return BM_ERROR;
    }

    bool found = false;
    for (struct ifaddrs *ifa = ifAddsPtr; ifa != nullptr; ifa = ifa->ifa_next) {
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