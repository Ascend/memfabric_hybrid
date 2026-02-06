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
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <climits>
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
const std::string SMEM_GROUP_LISTEN_EVENT_KEY = "EVENT";
const std::string SMEM_GROUP_DYNAMIC_SIZE_KEY = "DSIZE";
const std::string SMEM_GROUP_CAS_ALLOC_NUM_KEY = "AT_NUM";
constexpr uint32_t SMEM_ALLOC_NUM_SIZE = SMEM_SHM_ATOMIC_NUM_LIMIT;
constexpr uint32_t SMEM_ALLOC_NUM_BUF_LEN = (SMEM_ALLOC_NUM_SIZE + 7) / 8; // uint8_t
constexpr uint32_t SMEM_GATHER_PREFIX_SIZE = 4U;
constexpr int32_t SMEM_GROUP_MS_TO_US = 1000;
constexpr int64_t SMEM_GROUP_LISTER_TIMEOUT = 10LL * 1000;              // 10s, unit: ms
constexpr int32_t SMEM_GROUP_SLEEP_TIMEOUT = 100 * SMEM_GROUP_MS_TO_US; // 100ms, unit: us
constexpr int32_t SMEM_GROUP_SLEEP_5S = 5000 * SMEM_GROUP_MS_TO_US;     // 5s

constexpr uint32_t UINT_BIT = 8U;
constexpr int32_t GROUP_DYNAMIC_SIZE_BIT_LEN = 30;
constexpr uint32_t GROUP_DYNAMIC_SIZE_BIT_MASK = (1 << 30) - 1;

constexpr uint32_t USER_GROUP_KEY_LEN_MAX = 64;

struct JoinLeaveEventValue {
    bool join; // true => join, false => leave
    char evt;
    uint32_t rankId;

    JoinLeaveEventValue() : JoinLeaveEventValue{true, 0} {}
    JoinLeaveEventValue(bool j, uint32_t r) : join{j}, evt{j ? 'J' : 'L'}, rankId{r} {}

    std::string ToString() const
    {
        std::stringstream ss;
        ss << (join ? 'J' : 'L') << rankId;
        return ss.str();
    }

    int32_t Parse(const std::string &str)
    {
        std::stringstream ss(str);
        ss >> evt >> rankId;
        join = (evt == 'J');
        auto check = ToString();
        if (check != str) {
            SM_LOG_ERROR("parse from str: (" << str << ") invalid.");
            return SM_ERROR;
        }
        return SM_OK;
    }
};

static inline std::pair<int32_t, int32_t> SplitSizeAndVersion(int64_t val)
{
    auto unsignedVal = static_cast<uint64_t>(val);
    return std::make_pair(unsignedVal >> GROUP_DYNAMIC_SIZE_BIT_LEN, unsignedVal & GROUP_DYNAMIC_SIZE_BIT_MASK);
}

static int64_t MergeSizeAndVersion(int32_t ver, int32_t size)
{
    auto unsignedVer = static_cast<uint32_t>(ver);
    auto unsignedSize = static_cast<uint32_t>(size);
    return ((1LL * unsignedVer) << GROUP_DYNAMIC_SIZE_BIT_LEN) | unsignedSize;
}

SmemNetGroupEngine::~SmemNetGroupEngine()
{
    groupStoped_ = true;
    if (listenCtx_.watchId != UINT32_MAX) {
        (void)store_->Unwatch(listenCtx_.watchId);
    }
    uint32_t linkStatusWatchId = listenLinkStatusWatchId_.load(std::memory_order_acquire);
    if (linkStatusWatchId != UINT32_MAX) {
        (void)store_->Unwatch(linkStatusWatchId);
        listenLinkStatusWatchId_.store(UINT32_MAX, std::memory_order_release);
    }
    if (listenThread_.joinable()) {
        listenSignal_.PthreadSignal();
        listenThread_.join();
    }
}

SmemGroupEnginePtr SmemNetGroupEngine::Create(const StorePtr &store, const SmemGroupOption &option)
{
    std::string prefix = (option.dynamic ? "D_" : "S_");
    StorePtr ss = StoreFactory::PrefixStore(store, prefix);
    SM_ASSERT_RETURN(ss != nullptr, nullptr);

    SmemGroupEnginePtr group = SmMakeRef<SmemNetGroupEngine>(ss, option);
    SM_ASSERT_RETURN(group != nullptr, nullptr);

    if (option.dynamic) {
        SM_ASSERT_RETURN(group->StartListenEvent() == SM_OK, nullptr);
    }
    return group.Get();
}

Result SmemNetGroupEngine::GroupBarrier()
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    uint32_t size = option_.rankSize;
    std::string idx = std::to_string(groupVersion_) + "_" + std::to_string(++barrierGroupSn_);
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
        uint32_t removeBarrierGroupSn = barrierGroupSn_ - REMOVE_INTERVAL;
        std::string removeAddIdx = std::to_string(groupVersion_) + "_" + std::to_string(removeBarrierGroupSn) + "_BA";
        std::string removeWaitIdx = std::to_string(groupVersion_) + "_" + std::to_string(removeBarrierGroupSn) + "_BW";
        /* There is no need to return ERROR, when the removed key is already not exist.
        The WARNING LOG is contained in the remove func itself, no need to print more log. */
        (void)store_->Remove(removeAddIdx);
        (void)store_->Remove(removeWaitIdx);
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

Result SmemNetGroupEngine::GroupBarrier(const char *key, uint32_t rankSize, uint32_t rankId)
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(key != nullptr, "invalid param, key is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(strlen(key) < USER_GROUP_KEY_LEN_MAX, "key too long:" << strlen(key), SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(rankSize <= option_.rankSize,
                       "rankSize is invalid! input:" << rankSize << " option:" << option_.rankSize, SM_INVALID_PARAM);
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
        uint32_t removeBarrierGroupSn = localSn - REMOVE_INTERVAL;
        std::string removeAddIdx = userKey + "_" + std::to_string(removeBarrierGroupSn) + "_BA";
        std::string removeWaitIdx = userKey + "_" + std::to_string(removeBarrierGroupSn) + "_BW";
        /* There is no need to return ERROR, when the removed key is already not exist.
        The WARNING LOG is contained in the remove func itself, no need to print more log. */
        (void)store_->Remove(removeAddIdx);
        (void)store_->Remove(removeWaitIdx);
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
    uint32_t size = option_.rankSize;
    SM_ASSERT_RETURN(sendSize * size == recvSize, SM_INVALID_PARAM);

    std::string idx = std::to_string(groupVersion_) + "_" + std::to_string(++allGatherGroupSn_);
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
        uint32_t rmAllGatherGroupSn = allGatherGroupSn_ - REMOVE_INTERVAL;
        std::string removeAddIdx = std::to_string(groupVersion_) + "_" + std::to_string(rmAllGatherGroupSn) + "_GA";
        std::string removeWaitIdx = std::to_string(groupVersion_) + "_" + std::to_string(rmAllGatherGroupSn) + "_GW";
        /* There is no need to return ERROR, when the removed key is already not exist.
        The WARNING LOG is contained in the remove func itself, no need to print more log. */
        (void)store_->Remove(removeAddIdx);
        (void)store_->Remove(removeWaitIdx);
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
    SM_VALIDATE_RETURN(key != nullptr, "invalid param, key is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(strlen(key) < USER_GROUP_KEY_LEN_MAX, "key too long:" << strlen(key), SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(rankSize <= option_.rankSize,
                       "rankSize is invalid! input:" << rankSize << " option:" << option_.rankSize, SM_INVALID_PARAM);
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
    if (val == input.size() && allGatherGroupSn_ > REMOVE_INTERVAL) {
        uint32_t rmAllGatherGroupSn = allGatherGroupSn_ - REMOVE_INTERVAL;
        std::string removeAddIdx = std::to_string(groupVersion_) + "_" + std::to_string(rmAllGatherGroupSn) + "_GA";
        std::string removeWaitIdx = std::to_string(groupVersion_) + "_" + std::to_string(rmAllGatherGroupSn) + "_GW";
        /* There is no need to return ERROR, when the removed key is already not exist.
        The WARNING LOG is contained in the remove func itself, no need to print more log. */
        (void)store_->Remove(removeAddIdx);
        (void)store_->Remove(removeWaitIdx);
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
    std::vector<uint8_t> old;
    int32_t num;
    int32_t ret;
    do {
        swap(expect, old);
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
        ret = store_->Cas(SMEM_GROUP_CAS_ALLOC_NUM_KEY, expect, value, old);
    } while (ret != 0 || old != expect);
    allocedSet_.insert(num);
    return num;
}

Result SmemNetGroupEngine::ReleaseNumber(int32_t val)
{
    if (allocedSet_.count(val) == 0) {
        SM_LOG_ERROR("key(" << val << ") is not exist!");
        return SM_OBJECT_NOT_EXISTS;
    }
    allocedSet_.erase(val);

    std::vector<uint8_t> expect;
    std::vector<uint8_t> value(SMEM_ALLOC_NUM_BUF_LEN, 0);
    std::vector<uint8_t> old;
    int32_t ret;

    value[val / UINT_BIT] ^= 1U << (val % UINT_BIT);
    do {
        swap(expect, old);
        if (expect.size() != 0) {
            value = expect;
        }
        if ((value[val / UINT_BIT] >> (val % UINT_BIT)) & 1U) {
            value[val / UINT_BIT] ^= 1U << (val % UINT_BIT);
        } else {
            SM_LOG_WARN("key(" << val << ") has released!");
            return SM_OK;
        }
        ret = store_->Cas(SMEM_GROUP_CAS_ALLOC_NUM_KEY, expect, value, old);
    } while (ret == 0 && old == expect);
    return SM_OK;
}

bool SmemNetGroupEngine::ReWatch()
{
    uint32_t wid;
    auto ret = store_->Watch(SMEM_GROUP_LISTEN_EVENT_KEY,
                             std::bind(&SmemNetGroupEngine::GroupWatchCb, this, std::placeholders::_1,
                                       std::placeholders::_2, std::placeholders::_3),
                             wid);
    if (ret != SM_OK) {
        SM_LOG_WARN_LIMIT("group watch failed, ret: " << ret);
        if (listenLinkStatusWatchId_.load() > 0) {
            store_->Unwatch(listenLinkStatusWatchId_.load());
            listenLinkStatusWatchId_.store(UINT32_MAX);
        }
        usleep(SMEM_GROUP_SLEEP_TIMEOUT);
        return true;
    }
    SM_LOG_INFO("Watch group listen successfully, wid: " << wid);
    listenCtx_.watchId = wid;
    listenCtx_.ret = SM_OK;
    if (listenLinkStatusWatchId_.load() == UINT32_MAX) {
        ret = store_->Watch(
            WatchRankType::WATCH_RANK_LINK_DOWN,
            [this](WatchRankType type, uint32_t downRankId) { RemoteRankLinkDownCb(downRankId); }, wid);
        if (ret != SM_OK) {
            SM_LOG_WARN_LIMIT("group watch failed, ret: " << ret);
            store_->Unwatch(listenCtx_.watchId);
            listenCtx_.watchId = UINT32_MAX;
            usleep(SMEM_GROUP_SLEEP_TIMEOUT);
            return true;
        }
        listenLinkStatusWatchId_.store(wid);
        SM_LOG_INFO("Watch link down event successfully, wid: " << wid);
    }
    return false;
}

void SmemNetGroupEngine::GroupListenEvent()
{
    std::string getVal;
    std::string prevEvent;

    listenThreadStarted_ = true;
    while (!groupStoped_.load()) {
        if (!joined_) {
            usleep(SMEM_GROUP_SLEEP_TIMEOUT);
            continue;
        }

        if (listenCtx_.watchId == UINT32_MAX) {
            if (ReWatch()) {
                continue;
            }
        }

        int contextRet = SM_OK;
        std::list<GroupEvent> currentEvents;
        auto ret = listenSignal_.TimedwaitMillsecs(SMEM_GROUP_LISTER_TIMEOUT, [this, &contextRet, &currentEvents]() {
            currentEvents = std::move(listenCtx_.events);
            contextRet = listenCtx_.ret;
        });

        if (groupStoped_.load()) {
            break;
        }

        if (ret != SM_OK) {
            continue;
        }

        if (contextRet != SM_OK) {
            store_->Unwatch(listenCtx_.watchId);
            listenCtx_.watchId = UINT32_MAX;
            store_->Unwatch(listenLinkStatusWatchId_.load());
            listenLinkStatusWatchId_.store(UINT32_MAX);
            continue;
        }

        if (!joined_) { // maybe has leaved
            continue;
        }

        for (auto &event : currentEvents) {
            std::unique_lock<std::mutex> uniqueLock{groupEventHandleMutex_};
            if (event.remoteRankId != option_.rank) {
                uniqueLock.unlock();
            }
            if (event.eventType == GroupEventType::LUNCH_JOIN_LEAVE_EVENT) {
                JoinLeaveEventProcess(event.value, prevEvent);
            } else if (event.eventType == GroupEventType::REMOTE_DOWN_EVENT) {
                RankLinkDownEventProcess(event.remoteRankId, prevEvent);
            } else {
                SM_LOG_ERROR("unknown group event type: " << static_cast<uint32_t>(event.eventType));
            }
        }
    }
    listenThreadStarted_ = false;
}

void SmemNetGroupEngine::JoinLeaveEventProcess(const std::string &value, std::string &prevEventValue)
{
    listenSignal_.OperateInLock([this]() { listenCtx_.watchId = UINT32_MAX; });

    JoinLeaveEventValue eVal;
    if (eVal.Parse(value) != SM_OK) {
        SM_LOG_WARN("value: (" << value << ") invalid.");
        return;
    }

    if (value == prevEventValue) {
        return;
    }
    prevEventValue = value;

    int64_t tmpVal = 0L;
    auto ret = store_->Add(SMEM_GROUP_DYNAMIC_SIZE_KEY, 0L, tmpVal);
    if (ret != SM_OK) {
        SM_LOG_ERROR("get group dynamic size failed, ret: " << ret);
        return;
    }

    auto sizePair = SplitSizeAndVersion(tmpVal);
    SM_LOG_INFO("handle group event, local_rk:" << option_.rank << " rank:" << eVal.rankId << " event:" << eVal.evt);
    UpdateGroupVersion(sizePair.first + 1);

    if (eVal.join) {
        option_.rankSize = static_cast<uint32_t>(SplitSizeAndVersion(tmpVal).second + 1);
        if (option_.joinCb != nullptr) {
            option_.joinCb(eVal.rankId);
        }
    } else {
        option_.rankSize = static_cast<uint32_t>(SplitSizeAndVersion(tmpVal).second - 1);
        if (option_.leaveCb != nullptr) {
            option_.leaveCb(eVal.rankId);
        }
        ClearBitmapForRank(eVal.rankId);
    }
}

void SmemNetGroupEngine::RankLinkDownEventProcess(uint32_t rankId, std::string &prevEventValue)
{
    SM_ASSERT_RET_VOID(store_ != nullptr);
    std::string currentEventValue;
    prevEventValue = std::to_string(option_.rank).append("L").append(std::to_string(rankId));
    auto ret = store_->Get(SMEM_GROUP_LISTEN_EVENT_KEY, currentEventValue, 0L);
    if (ret != StoreErrorCode::SUCCESS && ret != StoreErrorCode::NOT_EXIST) {
        SM_LOG_ERROR("cannot read event value from store : " << ret);
        return;
    }

    if (!TestBitmapForRank(rankId)) {
        SM_LOG_INFO("link down rank: " << rankId << " not joined.");
        return;
    }

    std::string existValue;
    if (!currentEventValue.empty()) {
        JoinLeaveEventValue eventValue;
        if (eventValue.Parse(currentEventValue) != SM_OK) {
            SM_LOG_WARN("event value invalid: (" << currentEventValue << ")");
            return;
        }

        // A节点正在加入/退出中，B节点发生故障
        if (rankId != eventValue.rankId) {
            SM_LOG_INFO("current rank: " << option_.rank << ", link down rank: " << rankId
                                         << ", current event: " << currentEventValue << " process later.");
            listenSignal_.OperateInLock(
                [this, &currentEventValue]() { listenCtx_.events.emplace_back(GroupEvent(currentEventValue)); }, true);
            return;
        }

        // 正在做退出或加入的节点发生故障
        ret = store_->Cas(SMEM_GROUP_LISTEN_EVENT_KEY, currentEventValue, "unlinked", existValue);
        if (ret != StoreErrorCode::SUCCESS) {
            SM_LOG_ERROR("cannot cas event value from store : " << ret);
            return;
        }
        LinkDownUpdateMeta(rankId);
        if (existValue == currentEventValue) {
            SM_LOG_INFO("rank: " << option_.rank << " remove key : " << SMEM_GROUP_LISTEN_EVENT_KEY);
            ret = store_->Remove(SMEM_GROUP_LISTEN_EVENT_KEY);
            if (ret != StoreErrorCode::SUCCESS) {
                SM_LOG_ERROR("cannot cas event value from store : " << ret);
            }
        }
    } else {
        LinkDownUpdateMeta(rankId);
    }

    ClearBitmapForRank(rankId);
}

void SmemNetGroupEngine::LinkDownUpdateMeta(uint32_t rankId)
{
    SM_ASSERT_RET_VOID(store_ != nullptr);
    int64_t tmpVal = 0L;
    auto ret = store_->Add(SMEM_GROUP_DYNAMIC_SIZE_KEY, 0L, tmpVal);
    if (ret != SM_OK) {
        SM_LOG_ERROR("get group dynamic size failed, ret: " << ret);
        return;
    }

    auto sizePair = SplitSizeAndVersion(tmpVal);
    auto version = sizePair.first;
    auto rankSize = static_cast<uint32_t>(sizePair.second);
    if (version != groupVersion_) {
        SM_LOG_DEBUG("[DEBUG]CAS for key(" << SMEM_GROUP_DYNAMIC_SIZE_KEY << ") version: " << version
                                           << " groupVersion_:" << groupVersion_ << " local:" << option_.rank
                                           << " downRank:" << rankId);
        UpdateGroupVersion(groupVersion_ + 1);
        option_.rankSize--;
    } else {
        version++;
        rankSize = --option_.rankSize;
        UpdateGroupVersion(version);
        auto newVal = MergeSizeAndVersion(version, rankSize);
        auto oldValStr = std::to_string(tmpVal);
        auto newValStr = std::to_string(newVal);
        std::string existStr;
        ret = store_->Cas(SMEM_GROUP_DYNAMIC_SIZE_KEY, oldValStr, newValStr, existStr);
        SM_LOG_DEBUG("[DEBUG]CAS for key(" << SMEM_GROUP_DYNAMIC_SIZE_KEY << ") local: " << option_.rank << " downRank:"
                                           << rankId << " oldVal:" << std::hex << tmpVal << " newVal:" << newVal);
        if (ret != SM_OK) {
            SM_LOG_ERROR("CAS for key(" << SMEM_GROUP_DYNAMIC_SIZE_KEY << ") failed: " << ret);
        }
    }
    option_.leaveCb(rankId);
}

void SmemNetGroupEngine::GroupWatchCb(int result, const std::string &key, const std::string &value)
{
    int ctxRet = SM_OK;
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("result: " << result);
        ctxRet = SM_ERROR;
        listenCtx_.ret = SM_ERROR;
    }

    if (key != SMEM_GROUP_LISTEN_EVENT_KEY) {
        ctxRet = SM_ERROR;
        listenCtx_.ret = SM_ERROR;
    }

    listenSignal_.OperateInLock(
        [this, ctxRet, &value]() {
            listenCtx_.ret = ctxRet;
            if (ctxRet == SM_OK) {
                listenCtx_.events.emplace_back(GroupEvent{value});
            }
        },
        true);
}

void SmemNetGroupEngine::RemoteRankLinkDownCb(uint32_t remoteRankId)
{
    SM_LOG_DEBUG("[DEBUG]RemoteRankLinkDownCb rank id: " << remoteRankId);
    listenSignal_.OperateInLock(
        [this, remoteRankId]() {
            listenCtx_.ret = SM_OK;
            listenCtx_.events.emplace_back(GroupEvent(remoteRankId));
        },
        true);
}

void SmemNetGroupEngine::ClearBitmapForRank(uint32_t rankId)
{
    if (rankId >= MAX_RANK_COUNT) {
        SM_LOG_ERROR("ClearBitmapForRank invalid rank id: " << rankId);
        return;
    }

    std::unique_lock<std::mutex> uniqueLock{rankBitmapMutex_};
    auto index = rankId / BITS_COUNT_IN_U64;
    auto shift = rankId % BITS_COUNT_IN_U64;
    joinedRanksBitmap_[index] &= ~(1UL << shift);
}

bool SmemNetGroupEngine::TestBitmapForRank(uint32_t rankId) const
{
    if (rankId >= MAX_RANK_COUNT) {
        SM_LOG_ERROR("TestBitmapForRank invalid rank id: " << rankId);
        return false;
    }

    std::unique_lock<std::mutex> uniqueLock{rankBitmapMutex_};
    auto index = rankId / BITS_COUNT_IN_U64;
    auto shift = rankId % BITS_COUNT_IN_U64;

    return ((joinedRanksBitmap_[index] & (1UL << shift)) != 0UL);
}

Result SmemNetGroupEngine::StartListenEvent()
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(listenSignal_.Initialize() == SM_OK, SM_ERROR);

    uint32_t wid = 0;
    auto ret = store_->Watch(
        WatchRankType::WATCH_RANK_LINK_DOWN,
        [this](WatchRankType type, uint32_t downRankId) { RemoteRankLinkDownCb(downRankId); }, wid);
    SM_ASSERT_RETURN(ret == SM_OK, ret);
    listenLinkStatusWatchId_.store(wid);

    std::thread th(&SmemNetGroupEngine::GroupListenEvent, this);
    while (!listenThreadStarted_) {
        usleep(SMEM_GROUP_SLEEP_TIMEOUT);
    }
    listenThread_ = std::move(th);
    return SM_OK;
}

Result SmemNetGroupEngine::GroupJoin()
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(option_.dynamic, SM_INVALID_PARAM);
    std::unique_lock<std::mutex> uniqueLock{groupEventHandleMutex_};
    if (joined_) {
        return SM_OK;
    }
    std::string old;
    std::string val = "J" + std::to_string(option_.rank);
    int retry_count = 0;
    static constexpr int MAX_RETRY = 100000;
    while (retry_count++ < MAX_RETRY) {
        auto ret = store_->Cas(SMEM_GROUP_LISTEN_EVENT_KEY, "", val, old);
        if (ret == SM_OK && (old.empty() || old == val)) {
            break;
        }
        usleep(SMEM_GROUP_SLEEP_TIMEOUT);
    }
    int64_t tmp;
    auto ret = store_->Add(SMEM_GROUP_DYNAMIC_SIZE_KEY, 0, tmp);
    if (ret != SM_OK) {
        SM_LOG_ERROR("get group dynamic size failed, ret: " << ret);
        goto join_exit;
    }
    GroupSnClean();
    UpdateGroupVersion(SplitSizeAndVersion(tmp).first + 1);
    option_.rankSize = static_cast<uint32_t>(SplitSizeAndVersion(tmp).second + 1);
    if (option_.joinCb != nullptr) {
        ret = option_.joinCb(option_.rank);
        if (ret != SM_OK) {
            SM_LOG_ERROR("call join func failed, ret: " << ret);
            goto join_exit;
        }
    }
    ret = store_->Add(SMEM_GROUP_DYNAMIC_SIZE_KEY, 1LL << GROUP_DYNAMIC_SIZE_BIT_LEN | 1, tmp);
    if (ret != SM_OK) {
        SM_LOG_ERROR("update group dynamic size failed, ret: " << ret);
    }

join_exit:
    auto ret2 = store_->Remove(SMEM_GROUP_LISTEN_EVENT_KEY);
    if (ret2 != SM_OK) {
        SM_LOG_ERROR("reset group event failed, ret: " << ret2);
    }

    if ((ret | ret2) == SM_OK) {
        joined_ = true;
        return SM_OK;
    }
    return SM_ERROR;
}

Result SmemNetGroupEngine::GroupLeave()
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(option_.dynamic, SM_INVALID_PARAM);
    std::unique_lock<std::mutex> uniqueLock{groupEventHandleMutex_};
    SM_ASSERT_RETURN(joined_, SM_NOT_STARTED);

    Result ret;
    uint32_t watchId = 0;
    listenSignal_.OperateInLock(
        [&watchId, this]() {
            watchId = listenCtx_.watchId;
            groupStoped_ = true;
        },
        true);
    ret = store_->Unwatch(watchId);
    if (ret != SM_OK) {
        SM_LOG_ERROR("unwatch id: " << watchId << " failed: " << ret);
    }
    std::string old;
    std::string val = "L" + std::to_string(option_.rank);
    int retry_count = 0;
    static constexpr int MAX_RETRY = 100000;
    while (retry_count++ < MAX_RETRY) {
        ret = store_->Cas(SMEM_GROUP_LISTEN_EVENT_KEY, "", val, old);
        if (ret == SM_OK && (old.empty() || old == val)) {
            break;
        }
        usleep(SMEM_GROUP_SLEEP_TIMEOUT);
    }

    if (option_.leaveCb != nullptr) {
        ret = option_.leaveCb(option_.rank);
        if (ret != SM_OK) {
            SM_LOG_ERROR("call join func failed, ret: " << ret);
            goto leave_exit;
        }
    }
    int64_t tmpVal;
    ret = store_->Add(SMEM_GROUP_DYNAMIC_SIZE_KEY, GROUP_DYNAMIC_SIZE_BIT_MASK, tmpVal);
    if (ret != SM_OK) {
        SM_LOG_ERROR("update group dynamic size failed, ret: " << ret);
    }
    GroupSnClean();
    UpdateGroupVersion(SplitSizeAndVersion(tmpVal).first + 1);

leave_exit:
    auto ret2 = store_->Remove(SMEM_GROUP_LISTEN_EVENT_KEY);
    if (ret2 != SM_OK) {
        SM_LOG_ERROR("reset group event failed, ret: " << ret2);
    }

    joined_ = false;
    return (ret | ret2) == SM_OK ? SM_OK : SM_ERROR;
}

void SmemNetGroupEngine::UpdateGroupVersion(int32_t ver)
{
    groupVersion_ = ver;
    allGatherGroupSn_ = 0;
    barrierGroupSn_ = 0;
    SM_LOG_DEBUG("[DEBUG]Update version(" << SMEM_GROUP_DYNAMIC_SIZE_KEY << ") ver:" << " local:" << option_.rank);
}

void SmemNetGroupEngine::SetBitmapFromRanks(const std::vector<uint32_t> &rankIds)
{
    uint64_t tempBitmap[RANK_BITS_U64_COUNT];
    bzero(tempBitmap, sizeof(tempBitmap));
    for (auto rankId : rankIds) {
        if (rankId >= MAX_RANK_COUNT) {
            SM_LOG_ERROR("AddRanksToBitmap invalid rank id: " << rankId);
            continue;
        }

        auto index = rankId / BITS_COUNT_IN_U64;
        auto shift = rankId % BITS_COUNT_IN_U64;
        tempBitmap[index] |= (1UL << shift);
    }

    std::unique_lock<std::mutex> uniqueLock{rankBitmapMutex_};
    for (auto i = 0U; i < RANK_BITS_U64_COUNT; i++) {
        joinedRanksBitmap_[i] = tempBitmap[i];
    }
}

int32_t SmemNetGroupEngine::LinkReconnectHandler()
{
    uint32_t wid = 0;
    auto ret = store_->Watch(
        WatchRankType::WATCH_RANK_LINK_DOWN,
        [this](WatchRankType type, uint32_t downRankId) { RemoteRankLinkDownCb(downRankId); }, wid);
    if (ret != SM_OK) {
        SM_LOG_WARN("Failed to watch rank link status, ret: " << ret);
    } else {
        SM_LOG_INFO("Watch link down event successful wid: " << wid);
        listenLinkStatusWatchId_.store(wid);
    }

    int64_t tmpVal = 0L;
    ret = store_->Add(SMEM_GROUP_DYNAMIC_SIZE_KEY, 0L, tmpVal);
    if (ret != SM_OK) {
        SM_LOG_ERROR("get group dynamic size failed, ret: " << ret);
        return ret;
    }

    auto version = groupVersion_;
    auto rankSize = option_.rankSize;
    auto newVal = MergeSizeAndVersion(version, rankSize);
    auto oldValStr = std::to_string(tmpVal);
    auto newValStr = std::to_string(newVal);
    std::string existStr;
    SM_LOG_DEBUG("[DEBUG]Try cas for key(" << SMEM_GROUP_DYNAMIC_SIZE_KEY << ") version: " << std::hex << version
                                           << ", rankSize:" << rankSize << ", oldVar:" << tmpVal);
    ret = store_->Cas(SMEM_GROUP_DYNAMIC_SIZE_KEY, oldValStr, newValStr, existStr);
    if (ret != SM_OK) {
        SM_LOG_WARN("CAS for key(" << SMEM_GROUP_DYNAMIC_SIZE_KEY << ") failed: " << ret);
    } else {
        StrUtil::String2Int(existStr, newVal);
        auto pair = SplitSizeAndVersion(newVal);
        SM_LOG_INFO("Cas for key(" << SMEM_GROUP_DYNAMIC_SIZE_KEY << ") version: " << version
                                   << ", rankSize:" << rankSize << ", oldVar: " << pair.first << "_" << pair.second);
    }
    return SM_OK;
}

void SmemNetGroupEngine::GroupSnClean()
{
    for (uint32_t i = 0; i < REMOVE_INTERVAL; i++) {
        if (allGatherGroupSn_ < i) {
            break;
        }
        uint32_t rmAllGatherGroupSn = allGatherGroupSn_ - i;
        std::string removeAddIdx = std::to_string(groupVersion_) + "_" + std::to_string(rmAllGatherGroupSn) + "_GA";
        std::string removeWaitIdx = std::to_string(groupVersion_) + "_" + std::to_string(rmAllGatherGroupSn) + "_GW";
        (void)store_->Remove(removeAddIdx);
        (void)store_->Remove(removeWaitIdx);
    }

    for (uint32_t i = 0; i < REMOVE_INTERVAL; i++) {
        if (barrierGroupSn_ < i) {
            break;
        }
        uint32_t removeBarrierGroupSn = barrierGroupSn_ - i;
        std::string removeAddIdx = std::to_string(groupVersion_) + "_" + std::to_string(removeBarrierGroupSn) + "_BA";
        std::string removeWaitIdx = std::to_string(groupVersion_) + "_" + std::to_string(removeBarrierGroupSn) + "_BW";
        (void)store_->Remove(removeAddIdx);
        (void)store_->Remove(removeWaitIdx);
    }
}
} // namespace smem
} // namespace ock