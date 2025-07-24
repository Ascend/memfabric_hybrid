/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
*/
#ifndef MF_HYBRID_HYBM_TRANS_COMMON_H
#define MF_HYBRID_HYBM_TRANS_COMMON_H

#include "hybm_common_include.h"
#include <vector>
#include <unordered_map>

namespace ock {
namespace mf {
enum HybmTransType {
    TT_HCCP = 0,
    TT_HCOM,
    TT_COMPOSE,
    TT_BUTT,
};

struct HybmTransOptions {
    uint32_t rankId;
    uint32_t rankCount;
    std::string nic;

};

struct HybmTransMemReg {
    uint64_t addr = 0; /* virtual address of memory could be hbm or host dram */
    uint64_t size = 0;    /* size of memory to be registered */
    int32_t access = 0;   /* access right by local and remote */
    uint32_t flags = 0;   /* optional flags */
};

struct HybmTransKey {
    uint32_t keys[16];
};


struct MrInfo {
    uint64_t memAddr;
    uint64_t size;
    uint64_t lAddress;
    HybmTransKey lKey;
    void *mr;
};

struct HybmTransPrepareOptions {
    bool isServer = true;               /* server or client */
    std::vector<uint64_t> whitelist; /* list of whilelist */
    std::unordered_map<uint32_t, std::string> nics;
    std::vector<MrInfo> mrs;
};
}
}

#endif // MF_HYBRID_HYBM_TRANS_COMMON_H
