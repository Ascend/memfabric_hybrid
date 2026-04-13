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
#include <shared_mutex>
#include <atomic>
#include <list>
#include <queue>
#include "smem.h"
#include "smem_common_includes.h"
#include "smem_config_store.h"

namespace ock {
namespace smem {

class SmemNetGroupEngine;
using SmemGroupEnginePtr = SmRef<SmemNetGroupEngine>;
using SmemGroupChangeCallback = std::function<Result(uint32_t rank)>;
const uint32_t REMOVE_INTERVAL = 2;
constexpr uint32_t MAX_RANK_COUNT = SMEM_WORLD_SIZE_MAX;
constexpr uint32_t BITS_COUNT_IN_U64 = 64U;
constexpr uint32_t RANK_BITS_U64_COUNT = MAX_RANK_COUNT / BITS_COUNT_IN_U64;
constexpr uint32_t DEFAULT_STORE_KEY_DELAY_CLEAN_COUNT = 10;

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
    SmemGroupChangeCallback updateCb;
    SmemGroupChangeCallback leaveCb;
};

enum GroupEventType : int32_t {
    JOIN_EVENT = 0,
    LEAVE_EVENT = 1,
    RECOVER_EVENT = 2,
    LINK_DOWN_EVENT = 3,
    UPDATE_EVENT = 4,
    NULL_EVNET = 5,
    STOP_EVENT = 6,
};

template<typename T>
struct GroupListenContext {
    uint32_t watchId = UINT32_MAX;
    int32_t ret = SM_OK;
    std::list<T> values;
};

#pragma pack(push, 4)
struct SmemGroupInfo {
    // dynamic info
    uint32_t version;
    uint32_t groupSize;
    uint32_t curEvent;
    uint32_t targetRank;
    uint32_t submitRank;
    uint64_t joinedRanksBitmap[RANK_BITS_U64_COUNT];

    friend std::ostream &operator<<(std::ostream &os, const SmemGroupInfo &obj)
    {
        os << "SmemGroupInfo{size:" << obj.groupSize << " event:" << obj.curEvent
           << " target:" << obj.targetRank << " src:" << obj.submitRank << " ver:" << obj.version
           << " mask:" << obj.joinedRanksBitmap[0] << "}";
        return os;
    }
};
#pragma pack(pop)

class SmemNetGroupEngine : public SmReferable {
public:
    static SmemGroupEnginePtr Create(const StorePtr &store, const SmemGroupOption &option);

public:
    SmemNetGroupEngine(const StoreManagerPtr &store, const SmemGroupOption &option) : store_(store), option_(option)
    {
        groupInfo_.groupSize = option_.rankSize;
        joined_ = !option_.dynamic;
        if (option_.dynamic) {
            groupInfo_.groupSize = 0U; // not join, size is zero
        }
        store_->RegisterReconnectHandler(std::bind(&SmemNetGroupEngine::LinkReconnectHandler, this));
    }
    ~SmemNetGroupEngine() override;

    Result GroupBarrier();

    Result GroupBarrier(const char *key, uint32_t rankSize, uint32_t rankId);

    Result GroupGatherResult(int32_t localRet, int32_t &totalRet);

    Result GroupAllGather(const char *sendBuf, uint32_t sendSize, char *recvBuf, uint32_t recvSize);

    Result GroupAllGather(const char *key, uint32_t rankSize, uint32_t rankId, const char *sendBuf, uint32_t sendSize,
                          char *recvBuf, uint32_t recvSize);

    Result GroupBarrierPrefixKey(uint32_t dstRank, std::string &update);

    Result GroupGatherPrefixKey(uint32_t dstRank, std::string &update,
                                std::unordered_map<uint32_t, std::string> &retMap);

    Result TryRemovePrefixKey(uint32_t rank);

    Result GroupBroadcastExit(int status);

    Result RegisterExit(const std::function<void(int)> &exit);

    // dynamic group func
    int32_t AllocNumber();

    Result ReleaseNumber(int32_t val);

    Result StartListen();

    bool IsJoined() const { return joined_.load(); }

    Result GroupJoin();

    Result GroupUpdate();

    Result GroupLeave();

    uint32_t GetLocalRank() const;

    uint32_t GetRankSize() const;

    void GroupSnClean();

private:
    SmemGroupInfo GenerateInfo(uint32_t event, uint32_t target, std::string &old);
    bool TryUpdateInfo(SmemGroupInfo &info);
    uint32_t ReWatchEvent();
    uint32_t ReWatchLinkDown();
    void GroupListenEvent();
    void GroupListenLinkState();
    int32_t JoinLeaveEventProcess();
    void RankLinkDownEventProcess(uint32_t rankId);
    void GroupWatchCb(int result, const std::string &key, const std::string &value);
    void RemoteRankLinkDownCb(uint32_t remoteRankId);
    bool UpdateBitmapFromRank(SmemGroupInfo &info, uint32_t rankId);
    void GetAllRanksFromBitMap(std::vector<uint32_t> &rankIds);
    bool TestBitmapForRank(uint32_t rankId) const;
    void ClearBitmapForRank(SmemGroupInfo &info, uint32_t rankId);
    int32_t LinkReconnectHandler();
    uint32_t TryRemoveAllLeavedPrefixKey();
    void RankExit(int result, const std::string &key, const std::string &value);
    void GroupOldKeyDelayClean(const std::string &prefix, const std::string &suffix,
        uint32_t snStart, uint32_t snEnd, uint32_t delayCount = DEFAULT_STORE_KEY_DELAY_CLEAN_COUNT);
    Result StoreGetCanInterrupt(const std::string &key, std::string &value, uint64_t timeoutMs);
    void TryCleanOldEvent();
    Result DoLinkDownOnce(uint32_t rankId);

    StoreManagerPtr store_ = nullptr;
    SmemGroupOption option_;
    int32_t groupVersion_ = 0;
    uint32_t allGatherGroupSn_ = 0;
    uint32_t barrierGroupSn_ = 0;
    std::function<void(int)> globalExitHandler_;
    std::unordered_map<std::string, uint32_t> userGroupGatherSn_;
    std::unordered_map<std::string, uint32_t> userGroupBarrierSn_;
    std::set<int32_t> allocedSet_;

    std::thread eventListenThread_;
    SmemTimedwait eventListenSignal_;
    GroupListenContext<SmemGroupInfo> eventCtx_;
    std::atomic_uint32_t currentLeaveCount_{0};
    std::atomic_uint32_t currentStopCount_{0};

    std::thread linkListenThread_;
    SmemTimedwait linkListenSignal_;
    GroupListenContext<uint32_t> linkCtx_;
    std::atomic_uint32_t currentLinkDownCount_{0};

    SmemTimedwait linkOpSignal_;
    int32_t linkOpRet_;
    SmemTimedwait localOpSignal_;
    int32_t localOpRet_;

    std::atomic_uint32_t listenThreadStarted_{0};
    std::atomic_bool joined_ = false;
    std::atomic_bool groupStoped_ = false;
    mutable std::shared_mutex groupInfoMutex_;
    SmemGroupInfo groupInfo_{};
    std::atomic_uint32_t lastSubmitVersion_{0};
    std::atomic_uint64_t lastUpdateTime_{UINT64_MAX};

    std::queue<std::string> delayCleanKeyList_;
};

inline uint32_t SmemNetGroupEngine::GetLocalRank() const
{
    return option_.rank;
}

inline uint32_t SmemNetGroupEngine::GetRankSize() const
{
    return groupInfo_.groupSize;
}

} // namespace smem
} // namespace ock
#endif // SMEM_SMEM_NET_GROUP_ENGINE_H