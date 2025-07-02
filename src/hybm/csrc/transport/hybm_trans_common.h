/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_HYBRID_HYBM_TRANS_COMMON_H
#define MF_HYBRID_HYBM_TRANS_COMMON_H

#include "hybm_common_include.h"

namespace ock {
namespace mf {
enum HybmTransType {
    TT_HCCP_RDMA = 0,

    TT_BUTT,
};

struct HybmTransOptions {
    uint32_t rankId;
    std::string nic;
    HybmTransType transType;
    HybmTransOptions() : HybmTransOptions{TT_HCCP_RDMA} {}
    HybmTransOptions(HybmTransType type) : transType{type} {}
};

struct HybmTransMemReg {
    void *addr = nullptr; /* virtual address of memory could be hbm or host dram */
    uint64_t size = 0;    /* size of memory to be registered */
    int32_t access = 0;   /* access right by local and remote */
    uint32_t flags = 0;   /* optional flags */
};

struct HybmTransKey {
    uint8_t keys[64];
};

struct HybmTransPrepareOptions {
    bool isServer = true;               /* server or client */
    std::vector<uint64_t> whitelist; /* list of whilelist */
};
}

#endif // MF_HYBRID_HYBM_TRANS_COMMON_H
