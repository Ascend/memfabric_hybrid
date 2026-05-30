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
#include <pthread.h>
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <climits>
#include <shared_mutex>
#include "mf_num_util.h"
#include "mf_monotonic_time.h"
#include "acc_def.h"
#include "smem_store_factory.h"
#include "mf_str_util.h"
#include "smem_net_group_engine.h"
#include "smem_shm_def.h"

namespace ock {
namespace smem {
using namespace mf;

const std::string SMEM_GROUP_SET_STR = "ok";
const std::string SMEM_GROUP_EXIT_KEY = "EXIT";
const std::string SMEM_EXCHANGE_INFO_KEY = "BMEX_";
const std::string SMEM_GROUP_LISTEN_EVENT_KEY = "EVENT";
const std::string SMEM_GROUP_CAS_ALLOC_NUM_KEY = "AT_NUM";
constexpr uint32_t SMEM_ALLOC_NUM_SIZE = SMEM_SHM_ATOMIC_NUM_LIMIT;
constexpr uint32_t SMEM_ALLOC_NUM_BUF_LEN = (SMEM_ALLOC_NUM_SIZE + 7) / 8; // uint8_t
constexpr uint32_t SMEM_GATHER_PREFIX_SIZE = 4U;
constexpr int32_t SMEM_GROUP_MS_TO_US = 1000;
constexpr int64_t SMEM_GROUP_LISTER_TIMEOUT = 10LL * 1000;              // 10s, unit: ms
constexpr int32_t SMEM_GROUP_SLEEP_TIMEOUT = 100 * SMEM_GROUP_MS_TO_US; // 100ms, unit: us
constexpr uint64_t SMEM_EVNET_KEEP_TIME = 3 * SMEM_GROUP_MS_TO_US * SMEM_GROUP_MS_TO_US; // 3s, unit: us
constexpr uint64_t CLIENT_RECOVER_SLEEP_TIME = 6 * 1000 * 1000; // 6s, >= SERVER_RECOVER_TIME

constexpr uint32_t UINT_BIT = 8U;
constexpr uint32_t USER_GROUP_KEY_LEN_MAX = 64;
constexpr uint32_t SMEM_GROUP_INFO_SIZE = sizeof(SmemGroupInfo);
const std::string SMEM_GROUP_NOTIFY_EVENT = std::string(SMEM_GROUP_INFO_SIZE, 0);

SmemNetGroupEngine::~SmemNetGroupEngine()
{
    groupStoped_ = true;
    if (eventListenThread_.joinable()) {
        eventListenSignal_.PthreadSignal();
        eventListenThread_.join();
    }
    if (linkListenThread_.joinable()) {
        linkListenSignal_.PthreadSignal();
        linkListenThread_.join();
    }
    if (eventCtx_.watchId != UINT32_MAX) {
        (void)store_->Unwatch(eventCtx_.watchId);
        eventCtx_.watchId = UINT32_MAX;
    };
    if (linkCtx_.watchId != UINT32_MAX) {
        (void)store_->Unwatch(linkCtx_.watchId);
        linkCtx_.watchId = UINT32_MAX;
    };
}

SmemGroupEnginePtr SmemNetGroupEngine::Create(const StorePtr &store, const SmemGroupOption &option)
{
    std::string prefix = (option.dynamic ? "D_" : "S_");
    StorePtr ss = StoreFactory::PrefixStore(store, prefix);
    SM_ASSERT_RETURN(ss != nullptr, nullptr);
    StoreManagerPtr managerPtr = Convert<ConfigStore, ConfigStoreManager>(ss);
    SM_ASSERT_RETURN(managerPtr != nullptr, nullptr);
    SmemGroupEnginePtr group = SmMakeRef<SmemNetGroupEngine>(managerPtr, option);
    SM_ASSERT_RETURN(group != nullptr, nullptr);

    if (option.dynamic) {
        SM_ASSERT_RETURN(group->StartListen() == SM_OK, nullptr);
    }
    return group.Get();
}

Result SmemNetGroupEngine::StoreGetCanInterrupt(const std::string &key, std::string &value, uint64_t timeoutMs)
{
    int ret = SM_OK;
    uint64_t waitT = timeoutMs * SMEM_GROUP_MS_TO_US;
    uint64_t startT = mf::MonotonicTime::TimeUs();
    uint64_t nowT;
    uint64_t queryT = startT + SMEM_EVNET_KEEP_TIME;
    waitT = (startT > UINT64_MAX - waitT) ? UINT64_MAX : (waitT + startT);
    while ((nowT = mf::MonotonicTime::TimeUs()) < waitT) {
        ret = store_->Get(key, value, SMEM_GROUP_MS_TO_US); // wait 1s
        if (currentLeaveCount_.load() > 0 || currentLinkDownCount_.load() > 0) {
            SM_LOG_WARN("has rank leave or link down, stop get wait! key: " << store_->GetCompleteKey(key));
            return SM_INNER_BUSY;
        }
        if (currentStopCount_.load() > 0) {
            SM_LOG_WARN("now event has stoped, stop get wait! key: " << store_->GetCompleteKey(key));
            return SM_INNER_BUSY;
        }
        if (ret != SM_OK && ret != StoreErrorCode::TIMEOUT) {
            SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(key)
                                                        << " failed, result:" << ConfigStore::ErrStr(ret));
            return SM_ERROR;
        }
        if (ret == SM_OK) {
            return SM_OK;
        }
        // maybe has some rank leaved
        if (nowT > queryT) {
            uint32_t num = TryRemoveAllLeavedPrefixKey();
            if (num > 0) {
                return SM_INNER_BUSY;
            }
            queryT += SMEM_EVNET_KEEP_TIME;
        }
    }
    SM_LOG_WARN("get key timeout! key: " << store_->GetCompleteKey(key));
    return SM_TIMEOUT;
}

Result SmemNetGroupEngine::GroupGatherResult(int32_t localRet, int32_t &totalRet)
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    uint32_t size = groupInfo_.groupSize;
    std::string prefix = std::to_string(groupVersion_) + "_";
    std::string idx = prefix + std::to_string(++allGatherGroupSn_);
    std::string addKey = idx + "_GA";
    std::string waitKey = idx + "_GW";

    std::vector<uint8_t> input(sizeof(int32_t));
    uint64_t val = 0;
    *reinterpret_cast<int32_t *>(input.data()) = localRet;
    auto ret = store_->Append(addKey, input, val);
    if (ret != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("store add key: " << store_->GetCompleteKey(addKey)
                                                    << " failed, result:" << ConfigStore::ErrStr(ret));
        return SM_ERROR;
    }
    SM_LOG_DEBUG("store add key: " << store_->GetCompleteKey(addKey) << " len: " << val << " size:" << size);

    /* only the first rank needs to clear the last key, and it's unnecessary to clear map for first time */
    if (val == sizeof(int32_t) && allGatherGroupSn_ > REMOVE_INTERVAL) {
        uint32_t delSn = allGatherGroupSn_ - REMOVE_INTERVAL;
        GroupOldKeyDelayClean(prefix, "_GA", delSn, delSn);
        GroupOldKeyDelayClean(prefix, "_GW", delSn, delSn);
    }

    /* the last guy set the status to ok, and other guys just wait for the last guy set the value */
    if (val == sizeof(int32_t) * size) {
        ret = store_->Set(waitKey, SMEM_GROUP_SET_STR);
        if (ret != SM_OK) {
            SM_LOG_AND_SET_LAST_ERROR("store set key: " << store_->GetCompleteKey(waitKey)
                                                        << " failed, result:" << ConfigStore::ErrStr(ret));
            return SM_ERROR;
        }
    }

    std::string getVal;
    ret = StoreGetCanInterrupt(waitKey, getVal, option_.timeoutMs);
    if (ret != SM_OK) {
        return ret;
    }
    if (getVal != SMEM_GROUP_SET_STR) {
        SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(waitKey) << " val is not equal, val: "
                                                    << getVal << " expect: " << SMEM_GROUP_SET_STR);
        return SM_ERROR;
    }

    std::vector<uint8_t> output;
    ret = store_->Get(addKey, output, 0);
    if (ret != SM_OK || output.size() != sizeof(int32_t) * size) {
        SM_LOG_AND_SET_LAST_ERROR("after wait, store get key: "
                                  << store_->GetCompleteKey(addKey) << " failed, result:" << ConfigStore::ErrStr(ret)
                                  << " recv_size: " << output.size() << " input_size:" << input.size()
                                  << " group_size:" << size);
        return SM_ERROR;
    }

    auto *total = reinterpret_cast<int32_t *>(output.data());
    totalRet = 0;
    for (uint32_t i = 0; i < size; i++) {
        totalRet |= total[i];
    }
    return SM_OK;
}

Result SmemNetGroupEngine::GroupBarrier()
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(!option_.dynamic, SM_ERROR);
    uint32_t size = groupInfo_.groupSize;
    std::string prefix = std::to_string(groupVersion_) + "_";
    std::string idx = prefix + std::to_string(++barrierGroupSn_);
    std::string addKey = idx + "_BA";
    std::string waitKey = idx + "_BW";
    int64_t val = 0;

    MonoPerfTrace traceBarrier;
    /* all guys add 1 to barrier key and get it */
    MonoPerfTrace traceAdd;
    auto ret = store_->Add(addKey, 1, val);
    if (ret != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("store add key: " << store_->GetCompleteKey(addKey)
                                                    << " failed, result:" << ConfigStore::ErrStr(ret));
        return SM_ERROR;
    }
    traceAdd.RecordEnd();
    SM_LOG_DEBUG("store add key: " << store_->GetCompleteKey(addKey) << " value: " << val << " size:" << size);

    /* only the first rank needs to clear the last key, and it's unnecessary to clear map for first time */
    if (val == 1 && barrierGroupSn_ > REMOVE_INTERVAL) {
        uint32_t delSn = barrierGroupSn_ - REMOVE_INTERVAL;
        GroupOldKeyDelayClean(prefix, "_BA", delSn, delSn);
        GroupOldKeyDelayClean(prefix, "_BW", delSn, delSn);
    }

    /* the last guy set the status to ok, and other guys just wait for the last guy set the value */
    if (val == size) {
        ret = store_->Set(waitKey, SMEM_GROUP_SET_STR);
        if (ret != SM_OK) {
            SM_LOG_AND_SET_LAST_ERROR("store set key: " << store_->GetCompleteKey(waitKey)
                                                        << " failed, result:" << ConfigStore::ErrStr(ret));
            return SM_ERROR;
        }
        SM_LOG_DEBUG("store set key: " << store_->GetCompleteKey(waitKey));
    }

    /* all guys wait for waitKey status with timeout, timeout happens if the ok status not set by the last guy */
    MonoPerfTrace traceGetStatus;
    std::string getVal;
    ret = store_->Get(waitKey, getVal, option_.timeoutMs);
    if (ret != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(waitKey)
                                                    << " failed, ret:" << ConfigStore::ErrStr(ret));
        return ret;
    }
    traceGetStatus.RecordEnd();

    if (getVal != SMEM_GROUP_SET_STR) {
        SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(waitKey) << " val is not equal, val: "
                                                    << getVal << " expect: " << SMEM_GROUP_SET_STR);
        return SM_ERROR;
    }
    traceBarrier.RecordEnd();

    SM_LOG_INFO("groupBarrier successfully, key: " << store_->GetCompleteKey(waitKey) << ", size: " << size
                                                   << ", timeCostUs: total(" << traceBarrier.PeriodUs() << ") add("
                                                   << traceAdd.PeriodUs() << ") getStatus(" << traceGetStatus.PeriodUs()
                                                   << ")");
    return SM_OK;
}

Result SmemNetGroupEngine::GroupBarrier(const char *key, uint32_t rankSize, uint32_t rankId)
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(!option_.dynamic, SM_ERROR);
    SM_VALIDATE_RETURN(key != nullptr, "invalid param, key is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(strlen(key) < USER_GROUP_KEY_LEN_MAX, "key too long:" << strlen(key), SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(rankSize <= groupInfo_.groupSize,
        "rankSize is invalid! input:" << rankSize << " option:" << groupInfo_.groupSize, SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(rankId < rankSize, "rankId is invalid! rank:" << rankId << " size:" << rankSize,
                       SM_INVALID_PARAM);

    uint32_t size = rankSize;
    std::string userKey = std::string(key);
    uint32_t &localSn = userGroupBarrierSn_[userKey];
    std::string idx = userKey + "_" + std::to_string(++localSn);
    std::string addKey = idx + "_BA";
    std::string waitKey = idx + "_BW";
    int64_t val = 0;

    MonoPerfTrace traceBarrier;
    /* all guys add 1 to barrier key and get it */
    MonoPerfTrace traceAdd;
    auto ret = store_->Add(addKey, 1, val);
    if (ret != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("store add key: " << store_->GetCompleteKey(addKey)
                                                    << " failed, result:" << ConfigStore::ErrStr(ret));
        return SM_ERROR;
    }
    traceAdd.RecordEnd();
    SM_LOG_DEBUG("store add key: " << store_->GetCompleteKey(addKey) << " value: " << val << " size:" << size);

    /* only the first rank needs to clear the last key, and it's unnecessary to clear map for first time */
    if (val == 1 && localSn > REMOVE_INTERVAL) {
        uint32_t delSn = localSn - REMOVE_INTERVAL;
        GroupOldKeyDelayClean(userKey + "_", "_BA", delSn, delSn);
        GroupOldKeyDelayClean(userKey + "_", "_BW", delSn, delSn);
    }

    /* the last guy set the status to ok, and other guys just wait for the last guy set the value */
    if (val == size) {
        ret = store_->Set(waitKey, SMEM_GROUP_SET_STR);
        if (ret != SM_OK) {
            SM_LOG_AND_SET_LAST_ERROR("store set key: " << store_->GetCompleteKey(waitKey)
                                                        << " failed, result:" << ConfigStore::ErrStr(ret));
            return SM_ERROR;
        }
        SM_LOG_DEBUG("store set key: " << store_->GetCompleteKey(waitKey));
    }

    /* all guys wait for waitKey status with timeout, timeout happens if the ok status not set by the last guy */
    MonoPerfTrace traceGetStatus;
    std::string getVal;
    ret = store_->Get(waitKey, getVal, option_.timeoutMs);
    if (ret != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(waitKey)
                                                    << " failed, result:" << ConfigStore::ErrStr(ret));
        return SM_ERROR;
    }
    traceGetStatus.RecordEnd();

    if (getVal != SMEM_GROUP_SET_STR) {
        SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(waitKey) << " val is not equal, val: "
                                                    << getVal << " expect: " << SMEM_GROUP_SET_STR);
        return SM_ERROR;
    }
    traceBarrier.RecordEnd();

    SM_LOG_INFO("groupBarrier successfully, key: " << store_->GetCompleteKey(waitKey) << ", size: " << size
                                                   << ", timeCostUs: total(" << traceBarrier.PeriodUs() << ") add("
                                                   << traceAdd.PeriodUs() << ") getStatus(" << traceGetStatus.PeriodUs()
                                                   << ")");
    return SM_OK;
}

static inline void GatherFillRank(std::vector<uint8_t> &vec, uint32_t rank)
{
    uint32_t *st = reinterpret_cast<uint32_t *>(vec.data());
    *st = rank;
}

static void SortGatherRecv(std::vector<uint8_t> &vec, uint32_t preSize, uint32_t rankSize, char *recvBuf)
{
    std::vector<std::pair<uint32_t, uint32_t>> offset(rankSize);
    uint32_t unitSize = preSize + SMEM_GATHER_PREFIX_SIZE;
    uint8_t *ptr = vec.data();
    for (uint32_t i = 0; i < rankSize; i++) {
        uint32_t idx = i * unitSize;
        offset[i].first = *reinterpret_cast<uint32_t *>(ptr + idx);
        offset[i].second = idx + SMEM_GATHER_PREFIX_SIZE;
    }

    std::sort(offset.begin(), offset.end());
    for (uint32_t i = 0; i < rankSize; i++) {
        (void)std::copy_n(ptr + offset[i].second, preSize, recvBuf + preSize * i);
    }
}

Result SmemNetGroupEngine::GroupBroadcastExit(int status)
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(!option_.dynamic, SM_ERROR);

    auto ret = store_->Set(SMEM_GROUP_EXIT_KEY, std::to_string(status));
    SM_VALIDATE_RETURN(ret == SM_OK,
                       "store set key: " << store_->GetCompleteKey(SMEM_GROUP_EXIT_KEY)
                                         << " failed, result:" << ConfigStore::ErrStr(ret),
                       SM_ERROR);
    SM_LOG_DEBUG("store set key: " << store_->GetCompleteKey(SMEM_GROUP_EXIT_KEY));
    return ret;
}

Result SmemNetGroupEngine::RegisterExit(const std::function<void(int)> &exit)
{
    SM_ASSERT_RETURN(!option_.dynamic, SM_ERROR);
    if (globalExitHandler_ != nullptr) {
        SM_LOG_WARN("the exit function is not null");
        return SM_INVALID_PARAM;
    }
    SM_ASSERT_RETURN(exit != nullptr, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    globalExitHandler_ = exit;
    uint32_t wid;
    auto ret = store_->Watch(SMEM_GROUP_EXIT_KEY,
                             std::bind(&SmemNetGroupEngine::RankExit, this, std::placeholders::_1,
                                       std::placeholders::_2, std::placeholders::_3),
                             wid);
    if (ret != SM_OK) {
        SM_LOG_WARN("group watch failed, maybe link down, ret: " << ret);
        globalExitHandler_ = nullptr;
        return ret;
    }
    return SM_OK;
}

void SmemNetGroupEngine::RankExit(int result, const std::string &key, const std::string &value)
{
    if (result == SUCCESS && globalExitHandler_ != nullptr) {
        if (value.empty()) {
            SM_LOG_WARN("the value is empty");
            return;
        }
        int val = 0;
        try {
            long tempVal = std::stol(value);
            if (tempVal < std::numeric_limits<int>::min() || tempVal > std::numeric_limits<int>::max()) {
                SM_LOG_WARN("value out of int range: " << tempVal << " (value='" << value << "')");
                return;
            }
            val = static_cast<int>(tempVal);
        } catch (...) {
            SM_LOG_WARN("convert string to int failed");
            return;
        }
        globalExitHandler_(val);
    } else {
        SM_LOG_WARN("global exit failed");
    }
}

Result SmemNetGroupEngine::GroupAllGather(const char *sendBuf, uint32_t sendSize, char *recvBuf, uint32_t recvSize)
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(!option_.dynamic, SM_ERROR);
    uint32_t size = groupInfo_.groupSize;
    SM_ASSERT_RETURN(sendSize * size == recvSize, SM_INVALID_PARAM);

    std::string prefix = std::to_string(groupVersion_) + "_";
    std::string idx = prefix + std::to_string(++allGatherGroupSn_);
    std::string addKey = idx + "_GA";
    std::string waitKey = idx + "_GW";

    std::vector<uint8_t> input(sendSize + SMEM_GATHER_PREFIX_SIZE);
    GatherFillRank(input, option_.rank);
    (void)std::copy_n(sendBuf, sendSize, input.data() + SMEM_GATHER_PREFIX_SIZE);

    MonoPerfTrace traceAllGather;
    /* append things and get the length of value */
    MonoPerfTrace traceAppend;
    uint64_t val = 0;
    auto ret = store_->Append(addKey, input, val);
    if (ret != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("store add key: " << store_->GetCompleteKey(addKey)
                                                    << " failed, result:" << ConfigStore::ErrStr(ret));
        return SM_ERROR;
    }
    traceAppend.RecordEnd();

    /* only the first rank needs to clear the last key, and it's unnecessary to clear map for first time */
    if (val == input.size() && allGatherGroupSn_ > REMOVE_INTERVAL) {
        uint32_t delSn = allGatherGroupSn_ - REMOVE_INTERVAL;
        GroupOldKeyDelayClean(prefix, "_GA", delSn, delSn);
        GroupOldKeyDelayClean(prefix, "_GW", delSn, delSn);
    }

    /* the last guy set ok status */
    if (val == input.size() * size) {
        ret = store_->Set(waitKey, SMEM_GROUP_SET_STR);
        if (ret != SM_OK) {
            SM_LOG_AND_SET_LAST_ERROR("store set key: " << store_->GetCompleteKey(waitKey)
                                                        << " failed, result:" << ConfigStore::ErrStr(ret));
            return SM_ERROR;
        }
    }

    /* all guys wait for ok status with timeout */
    MonoPerfTrace traceGetStatus;
    std::string getVal;
    ret = store_->Get(waitKey, getVal, option_.timeoutMs);
    if (ret != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(waitKey)
                                                    << " failed, result:" << ConfigStore::ErrStr(ret));
        return SM_ERROR;
    }
    traceGetStatus.RecordEnd();

    if (getVal != SMEM_GROUP_SET_STR) {
        SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(waitKey) << " val is not equal, val: "
                                                    << getVal << " expect: " << SMEM_GROUP_SET_STR);
        return SM_ERROR;
    }

    /* get the whole value */
    MonoPerfTrace traceGetData;
    std::vector<uint8_t> output;
    ret = store_->Get(addKey, output, option_.timeoutMs);
    if (ret != SM_OK || output.size() != input.size() * size) {
        SM_LOG_AND_SET_LAST_ERROR("after wait, store get key: "
                                  << store_->GetCompleteKey(addKey) << " failed, result:" << ConfigStore::ErrStr(ret)
                                  << " recv_size: " << output.size() << " input_size:" << input.size()
                                  << " group_size:" << size);
        return SM_ERROR;
    }
    traceGetData.RecordEnd();
    traceAllGather.RecordEnd();

    SortGatherRecv(output, sendSize, size, recvBuf);

    SM_LOG_INFO("allGather successfully, key: "
                << store_->GetCompleteKey(addKey) << ", rank: " << option_.rank << ", size: " << size
                << ", timeCostUs: total(" << traceAllGather.PeriodUs() << ") append(" << traceAppend.PeriodUs()
                << ") getStatus(" << traceGetStatus.PeriodUs() << ") getData(" << traceGetData.PeriodUs() << ")");

    return SM_OK;
}

Result SmemNetGroupEngine::GroupAllGather(const char *key, uint32_t rankSize, uint32_t rankId, const char *sendBuf,
                                          uint32_t sendSize, char *recvBuf, uint32_t recvSize)
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(!option_.dynamic, SM_ERROR);
    SM_VALIDATE_RETURN(key != nullptr, "invalid param, key is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(strlen(key) < USER_GROUP_KEY_LEN_MAX, "key too long:" << strlen(key), SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(rankSize <= groupInfo_.groupSize,
        "rankSize is invalid! input:" << rankSize << " option:" << groupInfo_.groupSize, SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(rankId < rankSize, "rankId is invalid! rank:" << rankId << " size:" << rankSize,
                       SM_INVALID_PARAM);

    uint32_t size = rankSize;
    SM_ASSERT_RETURN(sendSize * size == recvSize, SM_INVALID_PARAM);

    std::string userKey = std::string(key);
    uint32_t &localSn = userGroupGatherSn_[userKey];
    std::string idx = userKey + "_" + std::to_string(++localSn);
    std::string addKey = idx + "_GA";
    std::string waitKey = idx + "_GW";

    std::vector<uint8_t> input(sendSize + SMEM_GATHER_PREFIX_SIZE);
    GatherFillRank(input, rankId);
    (void)std::copy_n(sendBuf, sendSize, input.data() + SMEM_GATHER_PREFIX_SIZE);

    MonoPerfTrace traceAllGather;
    /* append things and get the length of value */
    MonoPerfTrace traceAppend;
    uint64_t val = 0;
    auto ret = store_->Append(addKey, input, val);
    if (ret != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("store add key: " << store_->GetCompleteKey(addKey)
                                                    << " failed, result:" << ConfigStore::ErrStr(ret));
        return SM_ERROR;
    }
    traceAppend.RecordEnd();

    /* only the first rank needs to clear the last key, and it's unnecessary to clear map for first time */
    if (val == input.size() && localSn > REMOVE_INTERVAL) {
        uint32_t delSn = localSn - REMOVE_INTERVAL;
        GroupOldKeyDelayClean(userKey + "_", "_GA", delSn, delSn);
        GroupOldKeyDelayClean(userKey + "_", "_GW", delSn, delSn);
    }

    /* the last guy set ok status */
    if (val == input.size() * size) {
        ret = store_->Set(waitKey, SMEM_GROUP_SET_STR);
        if (ret != SM_OK) {
            SM_LOG_AND_SET_LAST_ERROR("store set key: " << store_->GetCompleteKey(waitKey)
                                                        << " failed, result:" << ConfigStore::ErrStr(ret));
            return SM_ERROR;
        }
    }

    /* all guys wait for ok status with timeout */
    MonoPerfTrace traceGetStatus;
    std::string getVal;
    ret = store_->Get(waitKey, getVal, option_.timeoutMs);
    if (ret != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(waitKey)
                                                    << " failed, result:" << ConfigStore::ErrStr(ret));
        return SM_ERROR;
    }
    traceGetStatus.RecordEnd();

    if (getVal != SMEM_GROUP_SET_STR) {
        SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(waitKey) << " val is not equal, val: "
                                                    << getVal << " expect: " << SMEM_GROUP_SET_STR);
        return SM_ERROR;
    }

    /* get the whole value */
    MonoPerfTrace traceGetData;
    std::vector<uint8_t> output;
    ret = store_->Get(addKey, output, option_.timeoutMs);
    if (ret != SM_OK || output.size() != input.size() * size) {
        SM_LOG_AND_SET_LAST_ERROR("after wait, store get key: "
                                  << store_->GetCompleteKey(addKey) << " failed, result:" << ConfigStore::ErrStr(ret)
                                  << " recv_size: " << output.size() << " input_size:" << input.size()
                                  << " group_size:" << size);
        return SM_ERROR;
    }
    traceGetData.RecordEnd();
    traceAllGather.RecordEnd();

    SortGatherRecv(output, sendSize, size, recvBuf);

    SM_LOG_INFO("allGather successfully, key: "
                << store_->GetCompleteKey(addKey) << ", rank: " << rankId << ", size: " << size
                << ", timeCostUs: total(" << traceAllGather.PeriodUs() << ") append(" << traceAppend.PeriodUs()
                << ") getStatus(" << traceGetStatus.PeriodUs() << ") getData(" << traceGetData.PeriodUs() << ")");

    return SM_OK;
}

int32_t SmemNetGroupEngine::AllocNumber()
{
    std::vector<uint8_t> expect;
    std::vector<uint8_t> value(SMEM_ALLOC_NUM_BUF_LEN, 0);
    std::vector<uint8_t> now;
    int32_t num;
    int32_t ret;
    do {
        swap(expect, now);
        if (expect.size() != 0) {
            value = expect;
        }
        num = -1;
        for (uint32_t i = 0; i < SMEM_ALLOC_NUM_SIZE; i++) {
            if ((value[i / UINT_BIT] & (1U << (i % UINT_BIT))) == 0) {
                num = static_cast<int32_t>(i);
                value[i / UINT_BIT] ^= 1U << (i % UINT_BIT);
                break;
            }
        }
        if (num == -1) {
            SM_LOG_ERROR("there is no free number available for allocation!");
            return SM_ERROR;
        }
        ret = store_->Cas(SMEM_GROUP_CAS_ALLOC_NUM_KEY, expect, value, now);
    } while (ret != 0);
    allocedSet_.insert(num);
    return num;
}

Result SmemNetGroupEngine::ReleaseNumber(int32_t val)
{
    if (allocedSet_.count(val) == 0) {
        SM_LOG_WARN("key(" << val << ") is not exist!");
        return SM_OBJECT_NOT_EXISTS;
    }
    allocedSet_.erase(val);

    std::vector<uint8_t> expect;
    std::vector<uint8_t> value(SMEM_ALLOC_NUM_BUF_LEN, 0);
    std::vector<uint8_t> now;
    int32_t ret;

    value[val / UINT_BIT] ^= 1U << (val % UINT_BIT);
    do {
        swap(expect, now);
        if (expect.size() != 0) {
            value = expect;
        }
        if ((value[val / UINT_BIT] >> (val % UINT_BIT)) & 1U) {
            value[val / UINT_BIT] ^= 1U << (val % UINT_BIT);
        } else {
            SM_LOG_WARN("key(" << val << ") has released!");
            return SM_OK;
        }
        ret = store_->Cas(SMEM_GROUP_CAS_ALLOC_NUM_KEY, expect, value, now);
    } while (ret != 0);
    return SM_OK;
}

Result SmemNetGroupEngine::TryRemovePrefixKey(uint32_t rank)
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(option_.dynamic, SM_ERROR);
    std::string key = SMEM_EXCHANGE_INFO_KEY + std::to_string(rank);
    auto ret = store_->Remove(key);
    if (ret == SM_OK) {
        SM_LOG_DEBUG("remove key success, src_rank:" << option_.rank << " key:" << store_->GetCompleteKey(key));
        if (rank == option_.rank) {
            prefixKey_.clear();
        }
    } else {
        SM_LOG_INFO("remove key failed, src_rank:" << option_.rank << " key:" << store_->GetCompleteKey(key)
                    << " ret:" << ret);
    }
    return ret;
}

uint32_t SmemNetGroupEngine::TryRemoveAllLeavedPrefixKey()
{
    std::vector<uint32_t> ranks;
    uint32_t count = 0;
    GetAllRanksFromBitMap(ranks);
    for (auto &rk : ranks) {
        if (rk == option_.rank) {
            continue;
        }
        uint32_t alive = 0;
        auto ret = store_->QueryAlive(rk, alive);
        if (ret == SM_OK && alive == 0) {
            SM_LOG_INFO("rank:" << rk << " has link down, try remove prefix key");
            TryRemovePrefixKey(rk);
            RemoteRankLinkDownCb(rk);
            count++;
        } else {
            std::string key = SMEM_EXCHANGE_INFO_KEY + std::to_string(rk);
            std::string val;
            ret = store_->Get(key, val, 0); // query whether the key is deleted
            if (ret == NOT_EXIST) {
                SM_LOG_INFO("rank:" << rk << " has removed");
                count++;
            }
        }
    }
    return count;
}

Result SmemNetGroupEngine::GroupBarrierPrefixKey(uint32_t dstRank, std::string &update)
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(option_.dynamic, SM_ERROR);
    std::string prefix = std::to_string(groupVersion_) + "_";
    std::string idx = prefix + std::to_string(++barrierGroupSn_);
    std::string waitKey = idx + "_BW";
    std::string key = SMEM_EXCHANGE_INFO_KEY + std::to_string(dstRank);

    if (dstRank == option_.rank) {
        // delete old key
        if (barrierGroupSn_ > REMOVE_INTERVAL) {
            uint32_t delSn = barrierGroupSn_ - REMOVE_INTERVAL;
            GroupOldKeyDelayClean(prefix, "_BA", delSn, delSn);
            GroupOldKeyDelayClean(prefix, "_BW", delSn, delSn);
        }

        uint64_t retLen = 0;
        auto ret = store_->Append(key, update, retLen);
        SM_VALIDATE_RETURN(ret == SM_OK,
                           "append prefix_key: " << store_->GetCompleteKey(key) << " failed, ret:" << ret, SM_ERROR);
        prefixKey_.append(update);
        ret = store_->Set(waitKey, SMEM_GROUP_SET_STR);
        if (ret != SM_OK) {
            SM_LOG_AND_SET_LAST_ERROR("store set key: " << store_->GetCompleteKey(waitKey)
                                                        << " failed, result:" << ConfigStore::ErrStr(ret));
            return SM_ERROR;
        }
    } else {
        std::string getVal;
        auto ret = StoreGetCanInterrupt(waitKey, getVal, option_.timeoutMs);
        if (ret != SM_OK) {
            SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(waitKey)
                                                        << " failed, ret:" << ret);
            return ret;
        }

        ret = store_->Get(key, update, 0);
        if (ret != SM_OK) {
            SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(key)
                                                        << " failed, result:" << ConfigStore::ErrStr(ret));
            return SM_ERROR;
        }
    }
    return SM_OK;
}

Result SmemNetGroupEngine::GatherAllPrefixKeys(const std::string &update,
                                               std::unordered_map<uint32_t, std::string> &retMap)
{
    std::string key = SMEM_EXCHANGE_INFO_KEY + std::to_string(option_.rank);
    auto ret = store_->Set(key, update);
    SM_VALIDATE_RETURN(ret == SM_OK,
                       "set prefix_key: " << store_->GetCompleteKey(key) << " failed, ret:" << ret, SM_ERROR);

    prefixKey_ = update;
    std::string completePrefix = store_->GetCompleteKey(SMEM_EXCHANGE_INFO_KEY);
    std::unordered_map<std::string, std::string> val;
    ret = store_->PrefixGet(SMEM_EXCHANGE_INFO_KEY, val);
    SM_VALIDATE_RETURN(ret == SM_OK,
                       "get prefix_key: " << completePrefix << " failed, ret:" << ret, SM_ERROR);
    for (auto &it : val) {
        std::vector<std::string> vec = StrUtil::Split(it.first, '_');
        if (vec.empty() || it.first.compare(0, completePrefix.length(), completePrefix) != 0) {
            SM_LOG_ERROR("prefix:" << completePrefix << " receive_key:" << it.first << " not match!");
            return SM_ERROR;
        }
        uint32_t rk;
        if (!StrUtil::String2Int(vec.back(), rk)) {
            SM_LOG_ERROR("receive_key:" << it.first << " can't get rank!");
            return SM_ERROR;
        }
        if (!TestBitmapForRank(rk)) {
            SM_LOG_WARN("has expired prefix key need to remove, rank:" << rk);
            TryRemovePrefixKey(rk);
        } else {
            retMap.emplace(rk, it.second);
        }
    }

    if (retMap.size() != groupInfo_.groupSize) {
        std::vector<uint32_t> bitmapRanks;
        GetAllRanksFromBitMap(bitmapRanks);
        for (auto rk : bitmapRanks) {
            if (rk != option_.rank && retMap.find(rk) == retMap.end()) {
                SM_LOG_WARN("rank " << rk << " is in bitmap but missing BMEX key, trigger link down cleanup");
                RemoteRankLinkDownCb(rk);
            }
        }
        return SM_INNER_BUSY;
    }
    return SM_OK;
}

Result SmemNetGroupEngine::GroupGatherPrefixKey(uint32_t dstRank, std::string &update,
                                                std::unordered_map<uint32_t, std::string> &retMap)
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(option_.dynamic, SM_ERROR);
    if (dstRank == option_.rank) { // get all
        auto ret = GatherAllPrefixKeys(update, retMap);
        if (ret != SM_OK) {
            return ret;
        }
    } else { // get one
        uint32_t ownRank = option_.rank;
        if (ownRank != dstRank && bmexNeedRefresh_ && !update.empty()) {
            std::string ownKey = SMEM_EXCHANGE_INFO_KEY + std::to_string(ownRank);
            auto writeRet = store_->Set(ownKey, update);
            if (writeRet != SM_OK) {
                SM_LOG_WARN("set own prefix_key: " << store_->GetCompleteKey(ownKey) << " failed, ret:" << writeRet);
            }
        }
        std::string key = SMEM_EXCHANGE_INFO_KEY + std::to_string(dstRank);
        std::string val;
        auto ret = StoreGetCanInterrupt(key, val, option_.timeoutMs);
        SM_VALIDATE_RETURN(ret == SM_OK, "get key: " << store_->GetCompleteKey(key) << " failed, ret:" << ret, ret);
        retMap.emplace(dstRank, val);
    }
    return SM_OK;
}

SmemGroupInfo SmemNetGroupEngine::GenerateInfo(uint32_t event, uint32_t target, std::string &old)
{
    std::shared_lock<std::shared_mutex> lock{groupInfoMutex_};
    SmemGroupInfo info = groupInfo_;
    info.version += 1;
    info.curEvent = event;
    info.submitRank = option_.rank;
    info.targetRank = target;
    old = std::string((char *)&groupInfo_, SMEM_GROUP_INFO_SIZE);
    return info;
}

bool SmemNetGroupEngine::TryUpdateInfo(SmemGroupInfo &info)
{
    std::unique_lock<std::shared_mutex> lock(groupInfoMutex_);
    SM_LOG_DEBUG("input:" << info << " before:" << groupInfo_);
    if (info.version > groupInfo_.version) {
        GroupSnClean();
        groupInfo_ = info;
        groupVersion_ = info.version;
        barrierGroupSn_ = 0;
        allGatherGroupSn_ = 0;
        lastUpdateTime_.store(mf::MonotonicTime::TimeUs());
        return true;
    }
    return false;
}

uint32_t SmemNetGroupEngine::ReWatchEvent()
{
    uint32_t wid;
    auto ret = store_->Watch(SMEM_GROUP_LISTEN_EVENT_KEY,
                             std::bind(&SmemNetGroupEngine::GroupWatchCb, this, std::placeholders::_1,
                                       std::placeholders::_2, std::placeholders::_3),
                             wid);
    if (ret != SM_OK || wid == UINT32_MAX) {
        SM_LOG_WARN_LIMIT("group watch failed, ret: " << ret << ", wid: " << wid);
        usleep(SMEM_GROUP_SLEEP_TIMEOUT);
        return UINT32_MAX;
    }
    SM_LOG_DEBUG("Watch group listen successfully, wid: " << wid);
    return wid;
}

uint32_t SmemNetGroupEngine::ReWatchLinkDown()
{
    uint32_t wid;
    auto ret = store_->Watch(
        WatchRankType::WATCH_RANK_LINK_DOWN,
        [this](WatchRankType type, uint32_t downRankId) { RemoteRankLinkDownCb(downRankId); }, wid);
    if (ret != SM_OK || wid == UINT32_MAX) {
        SM_LOG_WARN_LIMIT("group watch failed, ret: " << ret);
        usleep(SMEM_GROUP_SLEEP_TIMEOUT);
        return UINT32_MAX;
    }
    SM_LOG_INFO("Watch link down event successfully, wid: " << wid);
    return wid;
}

void SmemNetGroupEngine::GroupListenEvent()
{
    std::list<SmemGroupInfo> currentEvents;
    bool redoLast = false;
    listenThreadStarted_.fetch_add(1U);
    pthread_setname_np(pthread_self(), "grp_listen_evt");
    SM_LOG_DEBUG("GroupListenEvent start, rank:" << option_.rank);
    while (!groupStoped_.load()) {
        if (eventCtx_.watchId == UINT32_MAX) {
            eventCtx_.ret = SM_OK;
            eventCtx_.watchId = ReWatchEvent();
            if (eventCtx_.watchId == UINT32_MAX) {
                continue;
            }
            // === FIX: reset stale counters after watch re-established ===
            currentLeaveCount_.store(0);
            currentStopCount_.store(0);
            eventCtx_.values.clear();
        }

        int cRet = SM_OK;
        auto ret = eventListenSignal_.TimedwaitMillsecs(SMEM_GROUP_LISTER_TIMEOUT, [this, &cRet, &currentEvents]() {
            currentEvents.splice(currentEvents.end(), eventCtx_.values);
            cRet = eventCtx_.ret;
        });
        if (groupStoped_.load()) {
            break;
        }
        if (ret != SM_OK) {
            continue;
        }

        if (cRet != SM_OK) {
            store_->Unwatch(eventCtx_.watchId);
            eventCtx_.watchId = UINT32_MAX;
            for (auto &info : currentEvents) {
                if (info.curEvent == LEAVE_EVENT || info.curEvent == LINK_DOWN_EVENT) {
                    currentLeaveCount_.fetch_sub(1U);
                } else if (info.curEvent == STOP_EVENT) {
                    currentStopCount_.fetch_sub(1U);
                }
            }
            currentEvents.clear();
            redoLast = false;
            bmexNeedRefresh_ = true; // watch broke, may need to re-publish BMEX on next join
            continue;
        }

        ProcessEventItems(currentEvents, redoLast);
    }
    SM_LOG_DEBUG("GroupListenEvent end, rank:" << option_.rank);
    listenThreadStarted_.fetch_sub(1U);
}

void SmemNetGroupEngine::GroupListenLinkState()
{
    std::list<uint32_t> currentEvents;
    SM_LOG_DEBUG("GroupListenLinkState start, rank:" << option_.rank);
    listenThreadStarted_.fetch_add(2U);
    while (!groupStoped_.load()) {
        if (linkCtx_.watchId == UINT32_MAX) {
            linkCtx_.ret = SM_OK;
            linkCtx_.watchId = ReWatchLinkDown();
            if (linkCtx_.watchId == UINT32_MAX) {
                continue;
            }

            // === FIX: reset stale counters after watch re-established ===
            currentLinkDownCount_.store(0);
            linkCtx_.values.clear();
        }

        int cRet = SM_OK;
        auto ret = linkListenSignal_.TimedwaitMillsecs(SMEM_GROUP_LISTER_TIMEOUT, [this, &cRet, &currentEvents]() {
            currentEvents.splice(currentEvents.end(), linkCtx_.values);
            cRet = linkCtx_.ret;
        });
        if (groupStoped_.load()) {
            break;
        }
        if (ret != SM_OK) {
            continue;
        }

        if (cRet != SM_OK) {
            SM_LOG_ERROR("link watch failed, unwatch wid:" << linkCtx_.watchId << " ret:" << cRet);
            store_->Unwatch(linkCtx_.watchId);
            linkCtx_.watchId = UINT32_MAX;
        } else {
            for (auto &rank: currentEvents) {
                RankLinkDownEventProcess(rank);
            }
            // maybe join event has retried, signal up listen event thread
            if (currentEvents.size() > 0) {
                GroupWatchCb(SM_OK, SMEM_GROUP_LISTEN_EVENT_KEY, SMEM_GROUP_NOTIFY_EVENT);
            }
        }
        currentLinkDownCount_.fetch_sub(currentEvents.size());
        currentEvents.clear();
    }
    SM_LOG_DEBUG("GroupListenLinkState end, rank:" << option_.rank);
    listenThreadStarted_.fetch_sub(2U);
}

int32_t SmemNetGroupEngine::JoinLeaveEventProcess()
{
    int32_t ret = SM_OK;
    switch (groupInfo_.curEvent) {
        case JOIN_EVENT: {
            if ((joined_ || groupInfo_.targetRank == option_.rank) && option_.joinCb != nullptr) {
                if (currentStopCount_.load() > 0) {
                    SM_LOG_DEBUG("now join event has stop. version:" << groupInfo_.version);
                    return SM_OK;
                }
                // skip joinCb if has leaved or link_down
                if (currentLeaveCount_.load() == 0 && currentLinkDownCount_.load() == 0) {
                    ret = option_.joinCb(groupInfo_.targetRank);
                    if (currentStopCount_.load() > 0) {
                        ret = SM_OK;
                    }
                } else {
                    // return BUSY if joinCb return error and has leaved or link_down
                    // must read the latest value
                    SM_LOG_DEBUG("has leave or link_down, retry. leave:" << currentLeaveCount_.load()
                        << " link_down:" << currentLinkDownCount_.load());
                    ret = SM_INNER_BUSY;
                }
            }
            break;
        }
        case UPDATE_EVENT: {
            if (joined_ && option_.updateCb != nullptr) {
                if (currentStopCount_.load() > 0) {
                    SM_LOG_DEBUG("now update event has stop. version:" << groupInfo_.version);
                    return SM_OK;
                }
                // skip update if has leaved or link_down
                if (currentLeaveCount_.load() == 0 && currentLinkDownCount_.load() == 0) {
                    ret = option_.updateCb(groupInfo_.targetRank);
                    if (currentStopCount_.load() > 0) {
                        ret = SM_OK;
                    }
                } else {
                    // return BUSY if joinCb return error and has leaved or link_down
                    // must read the latest value
                    SM_LOG_DEBUG("has leave or link_down, retry. leave:" << currentLeaveCount_.load()
                        << " link_down:" << currentLinkDownCount_.load());
                    ret = SM_INNER_BUSY;
                }
            }
            break;
        }
        case RECOVER_EVENT: {
            // todo: 处理server故障场景
        }
        case LINK_DOWN_EVENT:
        case LEAVE_EVENT: {
            if (groupInfo_.targetRank != option_.rank && option_.leaveCb != nullptr) {
                ret = option_.leaveCb(groupInfo_.targetRank);
            }
            if (joined_ && groupInfo_.targetRank == option_.rank) { // local leave, wakeup leave func and exit thread
                SM_LOG_INFO("leave self, rank:" << option_.rank << " event:" << groupInfo_.curEvent);
                groupStoped_.store(false);
            }
            break;
        }
        case NULL_EVNET:
        case STOP_EVENT:
            return SM_OK;
        default: {
            SM_LOG_ERROR("unknow event:" << groupInfo_.curEvent);
            ret = SM_ERROR;
        }
    }

    if (groupInfo_.submitRank == option_.rank) {
        if (groupInfo_.curEvent != LINK_DOWN_EVENT) {
            localOpRet_ = ret;
            localOpSignal_.PthreadSignal();
        } else {
            linkOpRet_ = ret;
            linkOpSignal_.PthreadSignal();
        }
    }
    return ret;
}

Result SmemNetGroupEngine::DoLinkDownOnce(uint32_t rankId)
{
    std::string old;
    if (!TestBitmapForRank(rankId)) {
        SM_LOG_DEBUG("link down rank: " << rankId << " not joined, maybe has leaved by other");
        return SM_OK;
    }

    SmemGroupInfo info = GenerateInfo(LINK_DOWN_EVENT, rankId, old);
    ClearBitmapForRank(info, rankId);
    SM_LOG_DEBUG("remove generate_info:" << info << " base:" << groupInfo_);
    std::string val((char *)&info, SMEM_GROUP_INFO_SIZE);
    if (info.version & 1) {
        int ret = store_->Cas(SMEM_GROUP_LISTEN_EVENT_KEY, old, val, old);
        if (ret != StoreErrorCode::RESTORE) {
            if (ret != SM_OK) {
                SM_LOG_ERROR("cas event failed! ret:" << ret);
                return SM_ERROR;
            } else {
                linkOpRet_ = SM_OK;
                lastSubmitVersion_.store(info.version);
                goto wait_done;
            }
        }
        return SM_INNER_BUSY;
    } else {
        TryCleanOldEvent();
        return SM_INNER_BUSY;
    }

wait_done:
    SM_LOG_DEBUG("submit link down, target rank: " << rankId << " submit_rank:" << option_.rank);
    TryRemovePrefixKey(rankId);
    auto ret = linkOpSignal_.TimedwaitMillsecs(SMEM_GROUP_LISTER_TIMEOUT);
    if (ret != 0 || linkOpRet_ != SM_OK) {
        SM_LOG_ERROR("do link down failed! signal_ret:" << ret << " op_ret:" << localOpRet_);
        ret |= linkOpRet_;
    }
    info = GenerateInfo(NULL_EVNET, rankId, old);
    SM_LOG_DEBUG("generate info:" << info);
    std::string str((char *)&info, SMEM_GROUP_INFO_SIZE);
    auto ret2 = store_->Cas(SMEM_GROUP_LISTEN_EVENT_KEY, old, str, old);
    if (ret2 != SM_OK) {
        SM_LOG_ERROR("reset group event failed, ret: " << ret2 << " expect:" << info);
    } else {
        lastSubmitVersion_.store(info.version);
    }
    return SM_OK;
}

void SmemNetGroupEngine::ProcessEventItems(std::list<SmemGroupInfo> &currentEvents, bool &redoLast)
{
    uint32_t redoCount = 0;
    while (!currentEvents.empty()) {
        auto &info = currentEvents.front();
        bool canRemove = true;
        int32_t ret2 = SM_OK;
        if (TryUpdateInfo(info) || redoLast || redoCount > 0) {
            ret2 = JoinLeaveEventProcess();
            canRemove = (ret2 == SM_OK);
        }
        if (canRemove || currentLeaveCount_.load() > 0) {
            uint32_t curEvent = info.curEvent;
            if (curEvent == LEAVE_EVENT || curEvent == LINK_DOWN_EVENT) {
                currentLeaveCount_.fetch_sub(1U);
            } else if (curEvent == STOP_EVENT) {
                currentStopCount_.fetch_sub(1U);
            }
            currentEvents.pop_front();
            redoLast = false;
            if (ret2 == SM_OK && curEvent == JOIN_EVENT) {
                redoCount = 0;
                bmexNeedRefresh_ = false;
            }
        } else if (ret2 == SM_INNER_BUSY && redoCount < SMEM_GROUP_RETRY_TIME) {
            currentEvents.splice(currentEvents.end(), currentEvents, currentEvents.begin());
            redoLast = true;
            redoCount++;
        } else {
            break;
        }
    }
}

void SmemNetGroupEngine::RankLinkDownEventProcess(uint32_t rankId)
{
    SM_ASSERT_RET_VOID(store_ != nullptr);
    while (DoLinkDownOnce(rankId) == SM_INNER_BUSY) {
        // not cas success, retry
        usleep(SMEM_GROUP_SLEEP_TIMEOUT);
    };
}

void SmemNetGroupEngine::GroupWatchCb(int result, const std::string &key, const std::string &value)
{
    int ctxRet = SM_OK;
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("result: " << result);
        ctxRet = SM_ERROR;
    }

    if (key != SMEM_GROUP_LISTEN_EVENT_KEY) {
        ctxRet = SM_ERROR;
    }

    if (value.length() != SMEM_GROUP_INFO_SIZE) {
        SM_LOG_WARN("receive group info size is error!");
        ctxRet = SM_ERROR;
    }

    auto info = reinterpret_cast<SmemGroupInfo *>(const_cast<char *>(value.c_str()));
    SM_LOG_DEBUG("receive group info:" << *info << " current_leave:" << currentLeaveCount_);
    eventListenSignal_.OperateInLock(
        [this, ctxRet, &info]() {
            eventCtx_.ret |= ctxRet;
            if (ctxRet == SM_OK && info->version > 0) {
                if (info->curEvent == LEAVE_EVENT || info->curEvent == LINK_DOWN_EVENT) {
                    currentLeaveCount_.fetch_add(1U);
                } else if (info->curEvent == STOP_EVENT) {
                    currentStopCount_.fetch_add(1U);
                }
                eventCtx_.values.push_back(*info);
            }
        },
        true);
}

void SmemNetGroupEngine::RemoteRankLinkDownCb(uint32_t remoteRankId)
{
    SM_LOG_DEBUG("RemoteRankLinkDownCb rank id: " << remoteRankId << " current_down:" << currentLinkDownCount_);
    linkListenSignal_.OperateInLock(
        [this, remoteRankId]() {
            linkCtx_.ret = SM_OK;
            linkCtx_.values.emplace_back(remoteRankId);
            currentLinkDownCount_.fetch_add(1U);
        },
        true);
}

void SmemNetGroupEngine::ClearBitmapForRank(SmemGroupInfo &info, uint32_t rankId)
{
    if (rankId >= MAX_RANK_COUNT) {
        SM_LOG_ERROR("ClearBitmapForRank invalid rank id: " << rankId);
        return;
    }
    std::shared_lock<std::shared_mutex> lock{groupInfoMutex_};
    auto index = rankId / BITS_COUNT_IN_U64;
    auto shift = rankId % BITS_COUNT_IN_U64;
    info.joinedRanksBitmap[index] &= ~(1UL << shift);
    info.groupSize -= 1U;
}

bool SmemNetGroupEngine::TestBitmapForRank(uint32_t rankId) const
{
    if (rankId >= MAX_RANK_COUNT) {
        SM_LOG_ERROR("TestBitmapForRank invalid rank id: " << rankId);
        return false;
    }

    std::shared_lock<std::shared_mutex> lock{groupInfoMutex_};
    auto index = rankId / BITS_COUNT_IN_U64;
    auto shift = rankId % BITS_COUNT_IN_U64;

    return ((groupInfo_.joinedRanksBitmap[index] & (1UL << shift)) != 0UL);
}

bool SmemNetGroupEngine::UpdateBitmapFromRank(SmemGroupInfo &info, uint32_t rankId)
{
    if (rankId >= MAX_RANK_COUNT) {
        SM_LOG_ERROR("UpdateBitmapFromRank invalid rank id: " << rankId);
        return false;
    }

    auto index = rankId / BITS_COUNT_IN_U64;
    auto shift = rankId % BITS_COUNT_IN_U64;
    if ((info.joinedRanksBitmap[index] >> shift) & 1U) {
        return false;
    } else {
        info.joinedRanksBitmap[index] |= (1UL << shift);
        info.groupSize += 1U;
        return true;
    }
}

Result SmemNetGroupEngine::StartListen()
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(eventListenSignal_.Initialize() == SM_OK, SM_ERROR);
    SM_ASSERT_RETURN(linkListenSignal_.Initialize() == SM_OK, SM_ERROR);
    SM_ASSERT_RETURN(localOpSignal_.Initialize() == SM_OK, SM_ERROR);
    SM_ASSERT_RETURN(linkOpSignal_.Initialize() == SM_OK, SM_ERROR);

    std::thread th1(&SmemNetGroupEngine::GroupListenEvent, this);
    while (!(listenThreadStarted_.load() & 1U)) {
        usleep(SMEM_GROUP_SLEEP_TIMEOUT);
    }
    std::thread th2(&SmemNetGroupEngine::GroupListenLinkState, this);
    while (!(listenThreadStarted_.load() & 2U)) {
        usleep(SMEM_GROUP_SLEEP_TIMEOUT);
    }
    eventListenThread_ = std::move(th1);
    linkListenThread_ = std::move(th2);
    return SM_OK;
}

void SmemNetGroupEngine::TryCleanOldEvent()
{
    uint64_t now = mf::MonotonicTime::TimeUs();
    SmemGroupInfo oldInfo{};
    uint64_t last;
    {
        std::shared_lock<std::shared_mutex> lock{groupInfoMutex_};
        last = lastUpdateTime_.load();
        last = (last > UINT64_MAX - SMEM_EVNET_KEEP_TIME) ? UINT64_MAX : (last + SMEM_EVNET_KEEP_TIME);
        oldInfo = groupInfo_;
    }
    if (oldInfo.version & 1) {
        int ret = SM_OK;
        // delete old events of the same rank
        if (oldInfo.submitRank == option_.rank && lastSubmitVersion_ != oldInfo.version) {
            SM_LOG_INFO("current submit rank:" << oldInfo.submitRank << " has old version, try remove event!");
        } else if (now > last) {
            uint32_t alive = 0;
            ret = store_->QueryAlive(oldInfo.submitRank, alive);
            if (ret == SM_OK && alive == 0) {
                SM_LOG_INFO("current submit rank:" << oldInfo.submitRank << " has link down, try remove event!");
            } else {
                return;
            }
        } else {
            return;
        }

        std::string old;
        // query submit_rank, but clear target_rank's event
        SmemGroupInfo info = GenerateInfo(STOP_EVENT, oldInfo.targetRank, old);
        SM_LOG_DEBUG("generate info:" << info);
        if (info.version != oldInfo.version + 1) {
            SM_LOG_INFO("event has updated, skip remove event.");
            return;
        }
        if (oldInfo.curEvent == JOIN_EVENT) {
            ClearBitmapForRank(info, oldInfo.targetRank);
        }
        std::string val((char *)&info, SMEM_GROUP_INFO_SIZE);
        ret = store_->Cas(SMEM_GROUP_LISTEN_EVENT_KEY, old, val, old);
        if (ret == SM_OK || ret == RESTORE) {
            // nothing
        }
    }
}

Result SmemNetGroupEngine::GroupJoin()
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(option_.dynamic, SM_INVALID_PARAM);
    if (joined_) {
        return SM_OK;
    }

    TryRemovePrefixKey(option_.rank); // try to remove prefix key if it has same rank
    std::string old;
    int retry_count = 0;
    static constexpr int MAX_RETRY = 30;
    localOpRet_ = SM_OK; // init ret
    localOpSignal_.SignalClean(); // discard stale JOIN retry signal from prior attempt
    while (retry_count++ < MAX_RETRY) {
        if (!store_->GetConnectStatus()) {
            return SM_INNER_BUSY;
        }

        if (currentLeaveCount_.load() > 0 || currentLinkDownCount_.load() > 0) { // has leave or link_down, retry
            usleep(SMEM_GROUP_SLEEP_TIMEOUT);
            continue;
        }

        SmemGroupInfo info = GenerateInfo(JOIN_EVENT, option_.rank, old);
        if (!UpdateBitmapFromRank(info, option_.rank)) { // has same rank joined
            SM_LOG_INFO("found old rank, try remove it, rank:" << option_.rank);
            DoLinkDownOnce(option_.rank);
            usleep(SMEM_GROUP_SLEEP_TIMEOUT);
            continue;
        }
        std::string val((char *)&info, SMEM_GROUP_INFO_SIZE);
        SM_LOG_DEBUG("join generate_info:" << info << " base:" << groupInfo_);
        if (info.version & 1) {
            auto ret = store_->Cas(SMEM_GROUP_LISTEN_EVENT_KEY, old, val, old);
            if (ret == SM_OK) { // will cas ok if key not exist
                lastSubmitVersion_.store(info.version);
                break;
            }
        } else {
            TryCleanOldEvent();
        }
        usleep(SMEM_GROUP_SLEEP_TIMEOUT);
    }
    if (retry_count > MAX_RETRY) {
        SM_LOG_DEBUG("join failed, retry. rank:" << option_.rank);
        return SM_INNER_BUSY;
    }

    int ret = localOpSignal_.TimedwaitMillsecs(option_.timeoutMs);

    SmemGroupInfo info = GenerateInfo(NULL_EVNET, option_.rank, old);
    if (ret != SM_OK || localOpRet_ != SM_OK) {
        SM_LOG_ERROR("do join failed! signal_ret:" << ret << " op_ret:" << localOpRet_);
        ret = (ret == SM_OK ? localOpRet_ : ret);
        info.curEvent = STOP_EVENT;
        ClearBitmapForRank(info, option_.rank);
    }
    SM_LOG_DEBUG("generate info:" << info);
    std::string str((char *)&info, SMEM_GROUP_INFO_SIZE);
    auto ret2 = store_->Cas(SMEM_GROUP_LISTEN_EVENT_KEY, old, str, old);
    if (ret2 != SM_OK) {
        SM_LOG_ERROR("reset group event failed, ret: " << ret2 << " expect:" << info);
    } else {
        lastSubmitVersion_.store(info.version);
    }

    if ((ret | ret2) == SM_OK) {
        joined_ = true;
        return SM_OK;
    }
    return (ret2 == SM_OK && ret == SM_INNER_BUSY) ? SM_INNER_BUSY : SM_ERROR;
}

Result SmemNetGroupEngine::GroupUpdate()
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(option_.dynamic, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(joined_, SM_NOT_STARTED);

    std::string old;
    int retry_count = 0;
    static constexpr int MAX_RETRY = 30;
    localOpRet_ = SM_OK; // init ret
    localOpSignal_.SignalClean(); // discard stale signal from prior update retry
    while (retry_count++ < MAX_RETRY) {
        if (currentLeaveCount_.load() > 0 || currentLinkDownCount_.load() > 0) { // has leave or link_down, retry
            usleep(SMEM_GROUP_SLEEP_TIMEOUT);
            continue;
        }

        SmemGroupInfo info = GenerateInfo(UPDATE_EVENT, option_.rank, old);
        std::string val((char *)&info, SMEM_GROUP_INFO_SIZE);
        SM_LOG_DEBUG("update generate_info:" << info << " base:" << groupInfo_);
        if (info.version & 1) {
            auto ret = store_->Cas(SMEM_GROUP_LISTEN_EVENT_KEY, old, val, old);
            if (ret == SM_OK) { // will cas ok if key not exist
                lastSubmitVersion_.store(info.version);
                break;
            }
        } else {
            TryCleanOldEvent();
        }
        usleep(SMEM_GROUP_SLEEP_TIMEOUT);
    }
    if (retry_count > MAX_RETRY) {
        SM_LOG_DEBUG("update failed, retry. rank:" << option_.rank);
        return SM_INNER_BUSY;
    }

    int ret = localOpSignal_.TimedwaitMillsecs(option_.timeoutMs);
    SmemGroupInfo info = GenerateInfo(NULL_EVNET, option_.rank, old);
    if (ret != SM_OK || localOpRet_ != SM_OK) {
        SM_LOG_ERROR("do update failed! signal_ret:" << ret << " op_ret:" << localOpRet_);
        info.curEvent = STOP_EVENT;
        ret = (ret == SM_OK ? localOpRet_ : ret);
    }
    SM_LOG_DEBUG("update generate info:" << info);
    std::string str((char *)&info, SMEM_GROUP_INFO_SIZE);
    auto ret2 = store_->Cas(SMEM_GROUP_LISTEN_EVENT_KEY, old, str, old);
    if (ret2 != SM_OK) {
        SM_LOG_ERROR("reset group event failed, ret: " << ret2 << " expect:" << info);
    } else {
        lastSubmitVersion_.store(info.version);
    }

    if ((ret | ret2) == SM_OK) {
        return SM_OK;
    }
    return (ret2 == SM_OK && ret == SM_INNER_BUSY) ? SM_INNER_BUSY : SM_ERROR;
}

Result SmemNetGroupEngine::GroupLeave()
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(option_.dynamic, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(joined_, SM_NOT_STARTED);
    SM_LOG_INFO("do leave by user, rank:" << option_.rank);

    std::string old;
    int retry_count = 0;
    static constexpr int MAX_RETRY = 100000;
    localOpRet_ = SM_OK; // init ret
    while (retry_count++ < MAX_RETRY) {
        SmemGroupInfo info = GenerateInfo(LEAVE_EVENT, option_.rank, old);
        ClearBitmapForRank(info, option_.rank);
        std::string val((char *)&info, SMEM_GROUP_INFO_SIZE);
        SM_LOG_DEBUG("leave generate_info:" << info << " base:" << groupInfo_);
        if (info.version & 1) {
            auto ret = store_->Cas(SMEM_GROUP_LISTEN_EVENT_KEY, old, val, old);
            if (ret == SM_OK) {
                lastSubmitVersion_.store(info.version);
                break;
            }
        } else {
            TryCleanOldEvent();
        }
        usleep(SMEM_GROUP_SLEEP_TIMEOUT);
    }

    SM_VALIDATE_RETURN(retry_count <= MAX_RETRY, "do leave set key timeout!", SM_ERROR);

    TryRemovePrefixKey(option_.rank);
    // wait listen thread do leave
    int ret = localOpSignal_.TimedwaitMillsecs(option_.timeoutMs);
    if (ret != 0 || localOpRet_ != SM_OK) {
        SM_LOG_ERROR("wait leave timeout! signal_ret:" << ret << " op_ret:" << localOpRet_);
        ret |= localOpRet_;
    }

    SmemGroupInfo info = GenerateInfo(NULL_EVNET, option_.rank, old);
    SM_LOG_DEBUG("generate info:" << info);
    std::string str((char *)&info, SMEM_GROUP_INFO_SIZE);
    auto ret2 = store_->Cas(SMEM_GROUP_LISTEN_EVENT_KEY, old, str, old);
    if (ret2 != SM_OK) {
        SM_LOG_ERROR("reset group event failed, ret: " << ret2 << " expect:" << info);
    } else {
        lastSubmitVersion_.store(info.version);
    }

    joined_ = false;
    return (ret | ret2) == SM_OK ? SM_OK : SM_ERROR;
}

void SmemNetGroupEngine::GetAllRanksFromBitMap(std::vector<uint32_t> &rankIds)
{
    rankIds.clear();
    std::shared_lock<std::shared_mutex> lock{groupInfoMutex_};
    for (uint32_t i = 0; i < RANK_BITS_U64_COUNT; i++) {
        for (uint32_t j = 0; j < BITS_COUNT_IN_U64; j++) {
            if ((groupInfo_.joinedRanksBitmap[i] >> j) & 1U) {
                rankIds.push_back(i * BITS_COUNT_IN_U64 + j);
            }
        }
    }
}

int32_t SmemNetGroupEngine::LinkReconnectHandler()
{
    if (!option_.dynamic) {
        return SM_OK;
    }
    if (!joined_) {
        SM_LOG_WARN("not joined, skip reconnect handle, retry after sleep " << CLIENT_RECOVER_SLEEP_TIME << " us");
        usleep(CLIENT_RECOVER_SLEEP_TIME);
        return SM_OK;
    }

    std::shared_lock<std::shared_mutex> lock{groupInfoMutex_};
    SmemGroupInfo info = groupInfo_;
    lastUpdateTime_.store(0U);
    lastSubmitVersion_.store(0);
    lock.unlock();

    std::string old;
    std::string val((char *)&info, SMEM_GROUP_INFO_SIZE);
    auto ret = store_->Cas(SMEM_GROUP_LISTEN_EVENT_KEY, old, val, old);
    if (ret == SM_OK) { // will cas ok if key not exist
        SM_LOG_INFO("set group info success, rank:" << option_.rank);
    } else {
        info = *reinterpret_cast<SmemGroupInfo *>(const_cast<char *>(old.c_str())); // update last info
    }

    if (info.version & 1) {
        TryCleanOldEvent();
    }

    if (!prefixKey_.empty() && TestBitmapForRank(option_.rank)) {
        std::string key = SMEM_EXCHANGE_INFO_KEY + std::to_string(option_.rank);
        ret = store_->Set(key, prefixKey_);
        SM_VALIDATE_RETURN(ret == SM_OK,
                           "set prefix_key: " << store_->GetCompleteKey(key) << " failed, ret:" << ret, SM_ERROR);
    }

    SM_LOG_INFO("reconnect success, rank:" << option_.rank);
    return SM_OK;
}

void SmemNetGroupEngine::GroupOldKeyDelayClean(const std::string &prefix, const std::string &suffix,
                                               uint32_t snStart, uint32_t snEnd, const uint32_t delayCount)
{
    for (uint32_t i = snStart; i <= snEnd; i++) {
        std::string key = prefix + std::to_string(i) + suffix;
        delayCleanKeyList_.push(key);
    }
    while (delayCleanKeyList_.size() > delayCount) {
        (void)store_->Remove(delayCleanKeyList_.front());
        delayCleanKeyList_.pop();
    }
}

void SmemNetGroupEngine::GroupSnClean()
{
    std::string prefix = std::to_string(groupVersion_) + "_";
    uint32_t st = (allGatherGroupSn_ < REMOVE_INTERVAL) ? 1U : (allGatherGroupSn_ - REMOVE_INTERVAL + 1U);
    GroupOldKeyDelayClean(prefix, "_GA", st, allGatherGroupSn_, UINT32_MAX);
    GroupOldKeyDelayClean(prefix, "_GW", st, allGatherGroupSn_, UINT32_MAX);

    st = (barrierGroupSn_ < REMOVE_INTERVAL) ? 1U : (barrierGroupSn_ - REMOVE_INTERVAL + 1U);
    GroupOldKeyDelayClean(prefix, "_BA", st, barrierGroupSn_, UINT32_MAX);
    GroupOldKeyDelayClean(prefix, "_BW", st, barrierGroupSn_, UINT32_MAX);

    for (auto &it : userGroupBarrierSn_) {
        prefix = it.first + "_";
        st = (it.second < REMOVE_INTERVAL) ? 1U : (it.second - REMOVE_INTERVAL + 1U);
        GroupOldKeyDelayClean(prefix, "_BA", st, it.second, UINT32_MAX);
        GroupOldKeyDelayClean(prefix, "_BW", st, it.second, UINT32_MAX);
    }
    userGroupBarrierSn_.clear();
    for (auto &it : userGroupGatherSn_) {
        prefix = it.first + "_";
        st = (it.second < REMOVE_INTERVAL) ? 1U : (it.second - REMOVE_INTERVAL + 1U);
        GroupOldKeyDelayClean(prefix, "_GA", st, it.second, UINT32_MAX);
        GroupOldKeyDelayClean(prefix, "_GW", st, it.second, UINT32_MAX);
    }
    userGroupGatherSn_.clear();
}
} // namespace smem
} // namespace ock
