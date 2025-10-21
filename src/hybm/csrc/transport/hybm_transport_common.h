/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef MF_HYBRID_HYBM_TRANSPORT_COMMON_H
#define MF_HYBRID_HYBM_TRANSPORT_COMMON_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <ostream>
#include <iomanip>
#include <unordered_map>
#include "mf_net.h"
#include "hybm_def.h"

namespace ock {
namespace mf {
namespace transport {
constexpr uint32_t REG_MR_FLAG_DRAM = 0x1U;
constexpr uint32_t REG_MR_FLAG_HBM = 0x2U;

constexpr int32_t REG_MR_ACCESS_FLAG_LOCAL_WRITE = 0x1;
constexpr int32_t REG_MR_ACCESS_FLAG_REMOTE_WRITE = 0x2;
constexpr int32_t REG_MR_ACCESS_FLAG_REMOTE_READ = 0x4;
constexpr int32_t REG_MR_ACCESS_FLAG_BOTH_READ_WRITE = 0x7;

enum TransportType {
    TT_HCCP = 0,
    TT_HCOM,
    TT_COMPOSE,
    TT_BUTT,
};

struct TransportOptions {
    uint32_t rankId;
    uint32_t rankCount;
    uint32_t protocol;
    hybm_role_type role;
    std::string nic;
    IpType type {IpV4};

    friend std::ostream& operator<<(std::ostream& output, const TransportOptions& options)
    {
        output << "TransportOptions(rankId=" << options.rankId
               << ", count=" << options.rankCount
               << ", protocol=" << options.protocol
               << ", role=" << options.role
               << ", nid=" << options.nic
               << ", iptype=" << options.type << ")";
        return output;
    }
};

struct TransportMemoryRegion {
    uint64_t addr = 0;  /* virtual address of memory could be hbm or host dram */
    uint64_t size = 0;  /* size of memory to be registered */
    int32_t access = REG_MR_ACCESS_FLAG_BOTH_READ_WRITE; /* access right by local and remote */
    uint32_t flags = 0; /* optional flags: 加一个flag标识是DRAM还是HBM */

    friend std::ostream &operator<<(std::ostream &output, const TransportMemoryRegion &mr)
    {
        output << "MemoryRegion address size=" << mr.size << ", access=" << mr.access
            << ", flags=" << mr.flags << ")";
        return output;
    }
};

struct TransportMemoryKey {
    uint32_t keys[16];

    friend std::ostream &operator<<(std::ostream &output, const TransportMemoryKey &key)
    {
        output << "MemoryKey" << std::hex;
        for (auto i = 0U; i < sizeof(key.keys) / sizeof(key.keys[0]); i++) {
            output << "-" << key.keys[i];
        }
        output << std::dec;
        return output;
    }
};

struct TransportRankPrepareInfo {
    std::string nic;
    hybm_role_type role{HYBM_ROLE_PEER};
    std::vector<TransportMemoryKey> memKeys;

    TransportRankPrepareInfo() {}

    TransportRankPrepareInfo(std::string n, TransportMemoryKey k)
        : nic{std::move(n)}, role{HYBM_ROLE_PEER}, memKeys{k} {}

    TransportRankPrepareInfo(std::string n, hybm_role_type r, TransportMemoryKey k)
        : nic{std::move(n)}, role{r}, memKeys{k} {}

    TransportRankPrepareInfo(std::string n, std::vector<TransportMemoryKey> ks)
        : nic{std::move(n)}, role{HYBM_ROLE_PEER}, memKeys{std::move(ks)} {}

    TransportRankPrepareInfo(std::string n, hybm_role_type r, std::vector<TransportMemoryKey> ks)
        : nic{std::move(n)}, role{r}, memKeys{std::move(ks)} {}

    friend std::ostream &operator<<(std::ostream &output, const TransportRankPrepareInfo &info)
    {
        output << "PrepareInfo(nic=" << info.nic << ", role=" << info.role << ", memKeys=[";
        for (auto &key : info.memKeys) {
            output << key << " ";
        }
        output << "])";
        return output;
    }
};

struct HybmTransPrepareOptions {
    std::unordered_map<uint32_t, TransportRankPrepareInfo> options;

    friend std::ostream &operator<<(std::ostream &output, const HybmTransPrepareOptions &info)
    {
        output << "PrepareOptions(";
        for (auto &op : info.options) {
            output << op.first << " => " << op.second << ", ";
        }
        output << ")";
        return output;
    }
};

}  // namespace transport
}  // namespace mf
}  // namespace ock

#endif  // MF_HYBRID_HYBM_TRANSPORT_COMMON_H
