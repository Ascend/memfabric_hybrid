/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef SMEM_SMEM_NET_GROUP_ENGINE_H
#define SMEM_SMEM_NET_GROUP_ENGINE_H

#include <atomic>
#include "smem_common_includes.h"
#include "smem_config_store.h"

namespace ock {
namespace smem {

class SmemNetGroupEngine : public SmReferable {
public:
    SmemNetGroupEngine(StorePtr store, uint32_t timeoutMs) : store_(store), timeoutMs_(timeoutMs) {}
    ~SmemNetGroupEngine() = default;

    Result GroupBarrier(std::string &key, uint32_t size);

    Result GroupAllGather(std::string &key, uint32_t rank, uint32_t size,
        const char *sendBuf, uint32_t sendSize, char *recvBuf, uint32_t recvSize);

private:
    StorePtr store_ = nullptr;
    uint32_t timeoutMs_ = 0;
    std::atomic<uint32_t> operationSn_ = { 0 };
};

using SmemGroupEnginePtr = SmRef<SmemNetGroupEngine>;
}
}
#endif // SMEM_SMEM_NET_GROUP_ENGINE_H
