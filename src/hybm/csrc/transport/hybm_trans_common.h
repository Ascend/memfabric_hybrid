/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef HYBM_TRANS_COMMON_H
#define HYBM_TRANS_COMMON_H

#include "hybm_common_include.h"

namespace ock {
namespace mf {
enum TransType {
    TT_HCCP_RDMA = 0,

    TT_BUTT,
};

struct TransDeviceOptions {
    TransType transType;
    uint32_t rankId{0};
    uint32_t rankCount{1};
    TransDeviceOptions() : TransDeviceOptions{TT_HCCP_RDMA} {}
    TransDeviceOptions(TransType type) : transType{type} {}
};

struct TransMemRegInput {
    void *addr = nullptr; /* virtual address of memory could be hbm or host dram */
    uint64_t size = 0;    /* size of memory to be registered */
    int32_t access = 0;   /* access right by local and remote */
    uint32_t flags = 0;   /* optional flags */
};

struct TransMemRegOutput {
    void *handle = nullptr; /* handle */
    uint32_t lkey = 0;      /* local key for one-sided operation, i.e. write and read  */
    uint32_t rkey = 0;      /* remote key for one-sided operation, i.e. write and read */
};

struct TransPrepareOptions {
    bool isServer = true;            /* server or client */
};

struct TransDataConnOptions {
    uint32_t sqSize = 0;
    uint32_t cqSize = 0;
    uint32_t connNumber = 1;
    uint32_t maxRetries = 0;
    uint32_t timeout = 0;
};

struct TransDataConnAddressInfo {
    void *address;
    TransDataConnAddressInfo(void *addr = nullptr) : address{addr} {}
};

struct TransHandle {
    void *handle;
    TransHandle() : TransHandle{nullptr} {}
    TransHandle(void *hdl) : handle{hdl} {}
};

struct TransDataConn {};

class TransportManager;
using TransHandlePtr = std::shared_ptr<TransHandle>;
using TransDataConnPtr = std::shared_ptr<TransDataConn>;
using TransportManagerPtr = std::shared_ptr<TransportManager>;
}
}

#endif  //HYBM_TRANS_COMMON_H
