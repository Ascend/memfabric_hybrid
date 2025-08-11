/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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

inline bool CharToLong(const char* src, long &value)
{
    char *remain = nullptr;
    errno = 0;
    value = std::strtol(src, &remain, 10L);  // 10 is decimal digits
    if ((value == 0 && std::strcmp(src, "0") != 0) || remain == nullptr || strlen(remain) > 0 || errno == ERANGE) {
        return false;
    }
    return true;
}

inline bool StrToLong(const std::string &src, long &value)
{
    return CharToLong(src.c_str(), value);
}
}
}
#endif // MEM_FABRIC_HYBRID_SMEM_NET_COMMON_H