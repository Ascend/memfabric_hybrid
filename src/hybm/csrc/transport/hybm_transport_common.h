/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_HYBM_TRANSPORT_COMMON_H
#define MF_HYBRID_HYBM_TRANSPORT_COMMON_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <ostream>
#include <unordered_map>
#include "hybm_def.h"
#include "hybm_define.h"
#include "mf_tls_def.h"

namespace ock {
namespace mf {
namespace transport {
constexpr uint32_t REG_MR_FLAG_DRAM = 0x1U;
constexpr uint32_t REG_MR_FLAG_HBM = 0x2U;

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
    hybm_type initialType;
    hybm_role_type role;
    std::string nic;
    tls_config tlsOption;
};

static inline std::ostream &operator<<(std::ostream &output, const TransportOptions &options)
{
    output << "TransportOptions(rankId=" << options.rankId << ", count=" << options.rankCount << ", nic=" << options.nic
           << ")";
    return output;
}

struct TransportMemoryRegion {
    uint64_t addr = 0;  /* virtual address of memory could be hbm or host dram */
    uint64_t size = 0;  /* size of memory to be registered */
    int32_t access = RA_ACCESS_NORMAL; /* access right by local and remote */
    uint32_t flags = 0; /* optional flags: 加一个flag标识是DRAM还是HBM */
};

static inline std::ostream &operator<<(std::ostream &output, const TransportMemoryRegion &mr)
{
    output << "MemoryRegion(size=" << mr.size << ", access=" << mr.access << ", flags=" << mr.flags << ")";
    return output;
}

struct TransportMemoryKey {
    uint32_t keys[16];
};

static inline std::ostream &operator<<(std::ostream &output, const TransportMemoryKey &key)
{
    output << "MemoryKey";
    for (auto i = 0U; i < sizeof(key.keys) / sizeof(key.keys[0]); i++) {
        output << "-" << key.keys[i];
    }
    return output;
}

struct TransportRankPrepareInfo {
    std::string nic;
    hybm_role_type role{HYBM_ROLE_PEER};
    std::vector<TransportMemoryKey> memKeys;
    TransportRankPrepareInfo() {}
    TransportRankPrepareInfo(std::string n, TransportMemoryKey k) : nic{std::move(n)}, memKeys{k} {}
    TransportRankPrepareInfo(std::string n, std::vector<TransportMemoryKey> ks)
        : nic{std::move(n)},
          memKeys{std::move(ks)}
    {
    }
};

struct HybmTransPrepareOptions {
    std::unordered_map<uint32_t, TransportRankPrepareInfo> options;
};

}
}
}

#endif  // MF_HYBRID_HYBM_TRANSPORT_COMMON_H
