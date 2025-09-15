/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_MOBS_NET_COMMON_H
#define MEM_FABRIC_MOBS_NET_COMMON_H

#include "mmc_common_includes.h"
#include "mf_ipv4_validator.h"
#include "mmc_def.h"

namespace ock {
namespace mmc {
const std::string PROTOCOL_TCP = "tcp://";

enum NetProtoVersion : int16_t {
    VERSION_1 = 1,
};

enum NetProtocol {
    NET_NONE = 1,
    NET_RPC_TCP,
    NET_RPC_RDMA,
    NET_RPC_URMA,
    NET_IPC_UDS,
    NET_IPC_SHM,
};
using ExternalLog = void (*)(int, const char *);
struct NetEngineOptions {
    std::string name;             /* name of engine */
    std::string ip;               /* ip */
    uint16_t port = 9980L;        /* listen port */
    uint16_t threadCount = 2;     /* worker thread count */
    uint16_t rankId = UINT16_MAX; /* rank id */
    bool startListener = false;   /* start listener or not */
    tls_config tlsOption;         /* TLS communication options */
    int32_t logLevel = 3;
    ExternalLog logFunc = nullptr;

    /* functions */
    std::string ToString() const;

    static Result ExtractIpPortFromUrl(const std::string &url, NetEngineOptions &option);

private:
    static Result ExtractURL(const std::string &url, NetProtocol &p, std::map<std::string, std::string> &details);

    static Result ExtractTcpURL(const std::string &url, NetProtocol &p, std::map<std::string, std::string> &details);
};

class NetContext;
class NetLink;
class NetEngine;
using NetContextPtr = MmcRef<NetContext>;
using NetLinkPtr = MmcRef<NetLink>;
using NetEnginePtr = MmcRef<NetEngine>;

/* inline function for NetEngineOption */
inline std::string NetEngineOptions::ToString() const
{
    std::ostringstream oss;
    oss << "NetEngineOptions [name " << name
        << ", ip: " << ip << ", port: " << port
        << ", threadCount: " << threadCount
        << ", rankId " << rankId
        << ", startListener: " << startListener
        << ", tlsEnables: " << tlsOption.tlsEnable
        << "]";
    return oss.str();
}

inline Result NetEngineOptions::ExtractIpPortFromUrl(const std::string &url, NetEngineOptions &option)
{
    using namespace mf;

    NetProtocol p;
    std::map<std::string, std::string> details;
    /* extract to vector */
    auto result = ExtractURL(url, p, details);
    MMC_RETURN_ERROR(result, "Failed to extract url " << url << ", which is invalid");

    std::string ipStr = details["ip"];
    std::string portStr = details["port"];

    /* verify ip and port */
    Ipv4PortValidator validator1("IndexServiceUrl");
    validator1.Initialize();
    if (!(validator1.Validate(ipStr + ":" + portStr))) {
        MMC_LOG_ERROR("Invalid url " << url);
        return MMC_INVALID_PARAM;
    }

    /* covert port */
    long tmpPort = 0;
    if (!StrUtil::StrToLong(portStr, tmpPort)) {
        MMC_LOG_ERROR("Invalid url " << url);
        return MMC_INVALID_PARAM;
    }

    /* set ip and port */
    option.ip = ipStr;
    option.port = tmpPort;
    return MMC_OK;
}

inline Result NetEngineOptions::ExtractURL(const std::string &url, NetProtocol &p,
                                           std::map<std::string, std::string> &details)
{
    using namespace mf;

    p = NetProtocol::NET_NONE;
    details.clear();
    /* trim spaces */
    std::string tmpUrl = url;
    StrUtil::StrTrim(tmpUrl);
    /* case1: start with tcp:// */
    if (StrUtil::StartWith(url, PROTOCOL_TCP)) {
        return ExtractTcpURL(url, p, details);
    }

    return MMC_INVALID_PARAM;
}

inline Result NetEngineOptions::ExtractTcpURL(const std::string &url, NetProtocol &p,
                                              std::map<std::string, std::string> &details)
{
    using namespace mf;

    p = NetProtocol::NET_RPC_TCP;
    /* remove tcp:// */
    std::string tmpUrl = url.substr(PROTOCOL_TCP.length(), url.length() - PROTOCOL_TCP.length());

    /* split */
    std::vector<std::string> splits;
    StrUtil::Split(tmpUrl, ":", splits);
    if (splits.size() != UN2) {
        return MMC_INVALID_PARAM;
    }

    /* assign port */
    details["port"] = splits[1];
    if (splits[0].find('/') == std::string::npos) {
        /* assign ip */
        details["ip"] = splits[0];
        return MMC_OK;
    }

    /* get ip mask */
    tmpUrl = splits[0];
    StrUtil::Split(tmpUrl, "/", splits);
    if (splits.size() != UN2) {
        MMC_LOG_ERROR("Get mismatched splits' size (" << splits.size() << "), should get size (" << UN2 << ").");
        return MMC_INVALID_PARAM;
    }

    details["ip"] = splits[0];
    details["mask"] = splits[1];
    return MMC_OK;
}

}
}

#endif  // MEM_FABRIC_MOBS_NET_COMMON_H
