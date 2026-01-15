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
#ifndef SMEM_SMEM_NET_GROUP_ENGINE_H
#define SMEM_SMEM_NET_GROUP_ENGINE_H

#include <functional>
#include <thread>
#include <atomic>
#include <list>
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

enum class GroupEventType : int8_t { LUNCH_JOIN_LEAVE_EVENT = 0, REMOTE_DOWN_EVENT = 1 };

struct GroupEvent {
    GroupEventType eventType;
    uint32_t remoteRankId;
    std::string value;
    explicit GroupEvent(uint32_t id) : eventType{GroupEventType::REMOTE_DOWN_EVENT}, remoteRankId{id} {}
    explicit GroupEvent(std::string val)
        : eventType{GroupEventType::LUNCH_JOIN_LEAVE_EVENT}, remoteRankId{std::numeric_limits<uint32_t>::max()},
          value{std::move(val)}
    {}
};

struct GroupListenContext {
    uint32_t watchId = UINT32_MAX;
    int32_t ret = SM_OK;
    std::list<GroupEvent> events;
};

constexpr uint32_t MAX_RANK_COUNT = 1024U;
constexpr uint32_t BITS_COUNT_IN_U64 = 64U;
constexpr uint32_t RANK_BITS_U64_COUNT = MAX_RANK_COUNT / BITS_COUNT_IN_U64;

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
        store_->RegisterReconnectHandler(std::bind(&SmemNetGroupEngine::LinkReconnectHandler, this));
        bzero(joinedRanksBitmap_, sizeof(joinedRanksBitmap_));
    }
    ~SmemNetGroupEngine() override;

    Result GroupBarrier();

    Result GroupAllGather(const char *sendBuf, uint32_t sendSize, char *recvBuf, uint32_t recvSize);

    Result GroupBroadcastExit(int status);

    Result RegisterExit(const std::function<void(int)> &exit);

    Result StartListenEvent();

    Result GroupJoin();

    Result GroupLeave();

    uint32_t GetLocalRank() const;

    uint32_t GetRankSize() const;

    void SetBitmapFromRanks(const std::vector<uint32_t> rankIds);

    void GroupSnClean();

private:
    void GroupListenEvent();
    void JoinLeaveEventProcess(const std::string &value, std::string &prevEventValue);
    void RankLinkDownEventProcess(uint32_t rankId, std::string &prevEventValue);
    void LinkDownUpdateMeta(uint32_t rankId);
    void UpdateGroupVersion(int32_t ver);
    void GroupWatchCb(int result, const std::string &key, const std::string &value);
    void RemoteRankLinkDownCb(uint32_t remoteRankId);
    void ClearBitmapForRank(uint32_t rankId);
    bool TestBitmapForRank(uint32_t rankId) const;
    int32_t LinkReconnectHandler();
    void RankExit(int result, const std::string &key, const std::string &value);

    StorePtr store_ = nullptr;
    SmemGroupOption option_;
    int32_t groupVersion_ = 0;
    uint32_t allGatherGroupSn_ = 0;
    uint32_t barrierGroupSn_ = 0;

    std::thread listenThread_;
    SmemTimedwait listenSignal_;
    GroupListenContext listenCtx_;
    bool joined_ = false;
    std::atomic<bool> listenThreadStarted_{false};
    bool groupStoped_ = false;
    std::function<void(int)> globalExitHandler_;
    uint64_t joinedRanksBitmap_[RANK_BITS_U64_COUNT]{};
    mutable std::mutex rankBitmapMutex_;
};

inline uint32_t SmemNetGroupEngine::GetLocalRank() const
{
    return option_.rank;
}

inline uint32_t SmemNetGroupEngine::GetRankSize() const
{
    return option_.rankSize;
}

} // namespace smem
} // namespace ock
#endif // SMEM_SMEM_NET_GROUP_ENGINE_H