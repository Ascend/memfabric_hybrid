/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
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

const std::string PROTOCOL_TCP = "tcp://";

inline void Split(const std::string &src, const std::string &sep, std::vector<std::string> &out)
{
    std::string::size_type pos1 = 0;
    std::string::size_type pos2 = src.find(sep);

    std::string tmpStr;
    while (std::string::npos != pos2) {
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

Result ExtractTcpURL(const std::string &url, std::map<std::string, std::string> &details)
{
    if (url.compare(0, PROTOCOL_TCP.size(), PROTOCOL_TCP) != 0) {
        return SM_INVALID_PARAM;
    }

    /* remove tcp:// */
    std::string tmpUrl = url.substr(PROTOCOL_TCP.length(), url.length() - PROTOCOL_TCP.length());

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

    if (!IsValidIpV4(ipStr) || tmpPort <= N1024 || tmpPort > UINT16_MAX) {
        SM_LOG_ERROR("Invalid url. ");
        return SM_INVALID_PARAM;
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
        if (ifa->ifa_addr == nullptr) {
            continue;
        }

        if (ifa->ifa_addr->sa_family != AF_INET) { // only IPV4
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
