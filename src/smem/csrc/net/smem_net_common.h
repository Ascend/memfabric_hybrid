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

Result GetLocalIpWithTarget(const std::string &target, std::string &local);
}
}
#endif // MEM_FABRIC_HYBRID_SMEM_NET_COMMON_H