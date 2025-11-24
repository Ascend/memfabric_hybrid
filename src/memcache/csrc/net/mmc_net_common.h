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
    mmc_tls_config tlsOption;         /* TLS communication options */
    int32_t logLevel = 3;
    ExternalLog logFunc = nullptr;

    /* functions */
    std::string ToString() const;

    static Result ExtractIpPortFromUrl(const std::string &url, NetEngineOptions &option);
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
    auto result = SocketAddressParserMgr::getInstance().CreateParser(url);
    MMC_ASSERT_RETURN(result != nullptr, MMC_INVALID_PARAM);
    std::string ipStr = result->GetIp();
    std::string portStr = std::to_string(result->GetPort());
    if (!result->IsIpv6()) {
        /* verify ip and port */
        Ipv4PortValidator validator1("IndexServiceUrl");
        validator1.Initialize();
        if (!(validator1.Validate(ipStr + ":" + portStr))) {
            MMC_LOG_ERROR("Failed to extract url");
            return MMC_INVALID_PARAM;
        }
    }

    /* covert port */
    long tmpPort = 0;
    if (!StrUtil::String2Uint<long>(portStr, tmpPort)) {
        MMC_LOG_ERROR("Failed to extract url");
        return MMC_INVALID_PARAM;
    }

    /* set ip and port */
    option.ip = ipStr;
    option.port = tmpPort;
    return MMC_OK;
}
} // namespace mmc
} // namespace ock

#endif  // MEM_FABRIC_MOBS_NET_COMMON_H