/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef SMEM_SMEM_NET_GROUP_ENGINE_H
#define SMEM_SMEM_NET_GROUP_ENGINE_H

#include <functional>
#include <thread>
#include "smem_common_includes.h"
#include "smem_config_store.h"

namespace ock {
namespace smem {

class SmemNetGroupEngine;
using SmemGroupEnginePtr = SmRef<SmemNetGroupEngine>;
using SmemGroupChangeCallback = std::function<Result(uint32_t rank)>;
const uint32_t REMOVE_INTERVAL = 2;

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

struct GroupListenContext {
    uint32_t watchId = UINT32_MAX;
    int32_t ret = SM_OK;
    std::string value;
};

class SmemNetGroupEngine : public SmReferable {
public:
    static SmemGroupEnginePtr Create(const StorePtr &store, const SmemGroupOption &option);

public:
    SmemNetGroupEngine(const StorePtr &store, const SmemGroupOption &option) : store_(store), option_(option)
    {
        joined_ = !option_.dynamic;
        if (option_.dynamic) {
            option_.rankSize = 1;
        }
    }
    ~SmemNetGroupEngine() override;

    Result GroupBarrier();

    Result GroupAllGather(const char *sendBuf, uint32_t sendSize, char *recvBuf, uint32_t recvSize);

    Result StartListenEvent();

    Result GroupJoin();

    Result GroupLeave();

    uint32_t GetLocalRank() const;

    uint32_t GetRankSize() const;

    void GroupSnClean();

private:
    bool IsDigit(const std::string& str);
    void GroupListenEvent();
    Result TryCasEventKey(std::string &val);
    void UpdateGroupVersion(int32_t ver);
    void GroupWatchCb(int result, const std::string &key, const std::string &value);
    bool DealWithListenEvent(std::string& getVal, std::string& prevEvent);

    StorePtr store_ = nullptr;
    SmemGroupOption option_;
    int32_t groupVersion_ = 0;
    uint32_t allGatherGroupSn_ = 0;
    uint32_t barrierGroupSn_ = 0;

    std::thread listenThread_;
    SmemTimedwait listenSignal_;
    GroupListenContext listenCtx_;
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
