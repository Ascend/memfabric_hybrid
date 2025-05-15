/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef SMEM_SMEM_NET_GROUP_ENGINE_H
#define SMEM_SMEM_NET_GROUP_ENGINE_H

#include <atomic>
#include <functional>
#include "smem_common_includes.h"
#include "smem_config_store.h"

namespace ock {
namespace smem {

class SmemNetGroupEngine;
using SmemGroupEnginePtr = SmRef<SmemNetGroupEngine>;
using SmemGroupChangeCallback = std::function<Result(uint32_t rank)>;

/**
 * @brief create group option
 * @param rankSize          [in] the number of rank
 * @param rank              [in] local rank (rank is not necessarily between 0 and rankSize)
 * @param timeoutMs         [in] operation timeout (barrier, all_gather)
 * @param dynamic           [in] rankSize is dynamic (can join or leave some rank)
 * @param joinCb            [in] the callback which is called when some rank join
 * @param leaveCb           [in] the callback which is called when some rank leave
 */
struct SmemGroupOption {
    uint32_t rankSize;
    uint32_t rank;
    uint64_t timeoutMs;

    bool dynamic;
    SmemGroupChangeCallback joinCb;
    SmemGroupChangeCallback leaveCb;
};

class SmemNetGroupEngine : public SmReferable {
public:
    static SmemGroupEnginePtr Create(const StorePtr& store, const SmemGroupOption &option);

public:
    SmemNetGroupEngine(const StorePtr& store, const SmemGroupOption &option) : option_(option)
    {
        joined_ = !option_.dynamic;
        if (option_.dynamic) {
            option_.rankSize = 1;
        }
    }
    ~SmemNetGroupEngine() override = default;

    Result GroupBarrier();

    Result GroupAllGather(const char *sendBuf, uint32_t sendSize, char *recvBuf, uint32_t recvSize);

    Result StartListenEvent();

    Result GroupJoin();

    Result GroupLeave();

    uint32_t GetLocalRank() const;

    uint32_t GetRankSize() const;

private:
    void GroupListenEvent();

    bool OperationPreCheck();

    StorePtr store_ = nullptr;
    SmemGroupOption option_;
    std::atomic<uint32_t> groupSn_ = { 0 };
    bool joined_ = false;
    bool listenThreadStarted_ = false;
    bool groupStoped_ = false;
};

inline uint32_t SmemNetGroupEngine::GetLocalRank() const
{
    return option_.rank;
}

inline uint32_t SmemNetGroupEngine::GetRankSize() const
{
    return option_.rankSize;
}

}
}
#endif // SMEM_SMEM_NET_GROUP_ENGINE_H
