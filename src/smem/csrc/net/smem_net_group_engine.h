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
    SmemNetGroupEngine(const StorePtr& store, uint32_t rankSize, uint32_t rank, uint32_t timeoutMs) :
        store_(store), rankSize_(rankSize), localRank_(rank), timeoutMs_(timeoutMs) {}
    ~SmemNetGroupEngine() override = default;

    Result GroupBarrier();

    Result GroupAllGather(const char *sendBuf, uint32_t sendSize, char *recvBuf, uint32_t recvSize);

    uint32_t GetLocalRank() const;

    uint32_t GetRankSize() const;

private:
    StorePtr store_ = nullptr;
    uint32_t timeoutMs_ = 0;
    uint32_t rankSize_ = UINT32_MAX;
    uint32_t localRank_ = UINT32_MAX;
    std::atomic<uint32_t> groupSn_ = { 0 };
};

inline uint32_t SmemNetGroupEngine::GetLocalRank() const
{
    return localRank_;
}

inline uint32_t SmemNetGroupEngine::GetRankSize() const
{
    return rankSize_;
}

using SmemGroupEnginePtr = SmRef<SmemNetGroupEngine>;
}
}
#endif // SMEM_SMEM_NET_GROUP_ENGINE_H
