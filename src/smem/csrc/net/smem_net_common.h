/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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