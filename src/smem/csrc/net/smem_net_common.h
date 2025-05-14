/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#ifndef MEM_FABRIC_HYBRID_SMEM_NET_COMMON_H
#define MEM_FABRIC_HYBRID_SMEM_NET_COMMON_H

#include "smem_common_includes.h"

namespace ock {
namespace smem {

struct UrlExtraction {
    std::string ip;           /* ip */
    uint16_t port = 9980L;    /* listen port */

    Result ExtractIpPortFromUrl(const std::string &url);
};

Result GetLocalIpWithTarget(const std::string &target, std::string &local, uint32_t &ipv4);
}
}
#endif // MEM_FABRIC_HYBRID_SMEM_NET_COMMON_H