/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <climits>
#include "smem_net_group_engine.h"
#include "smem_store_factory.h"

namespace ock {
namespace smem {

const std::string SMEM_GROUP_SET_STR = "ok";
const std::string SMEM_GROUP_LISTEN_EVENT_KEY = "EVENT";
const std::string SMEM_GROUP_DYNAMIC_SIZE_KEY = "DSIZE";
constexpr uint32_t SMEM_GATHER_PREFIX_SIZE = 4U;
constexpr int64_t SMEM_LISTER_TIMEOUT = 100LL * 365 * 24 * 60 * 60 * 1000; // 100 years, unit: ms
constexpr int32_t SMEM_SLEEP_TIMEOUT = 100 * 1000; // 100ms

constexpr int32_t GROUP_DYNAMIC_SIZE_BIT_LEN = 30;
constexpr int32_t GROUP_DYNAMIC_SIZE_BIT_MASK = (1 << 30) - 1;

static inline std::pair<int32_t, int32_t> SplitSizeAndVersion(int64_t val)
{
    return std::make_pair(val >> GROUP_DYNAMIC_SIZE_BIT_LEN, val & GROUP_DYNAMIC_SIZE_BIT_MASK);
}

static int64_t MergeSizeAndVersion(int32_t ver, int32_t size)
{
    return ((1LL * ver) << GROUP_DYNAMIC_SIZE_BIT_LEN | size);
}

SmemNetGroupEngine::~SmemNetGroupEngine()
{
    groupStoped_ = true;
    if (listenCtx_.watchId != UINT32_MAX) {
        (void)store_->Unwatch(listenCtx_.watchId);
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
    std::string idx = std::to_string(groupVersion_) + "_" + std::to_string(groupSn_++);
    std::string addKey = idx + "_BA";
    std::string waitKey = idx + "_BW";
    int64_t val = 0;

    MonoPerfTrace traceBarrier;
    /* all guys add 1 to barrier key and get it */
    MonoPerfTrace traceAdd;
    auto ret = store_->Add(addKey, 1, val);
    if (ret != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("store add key: " << store_->GetCompleteKey(addKey) <<
            " failed, result:" << ConfigStore::ErrStr(ret));
        return SM_ERROR;
    }
    traceAdd.RecordEnd();
    SM_LOG_DEBUG("store add key: " << store_->GetCompleteKey(addKey) << " value: " << val);

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
        SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(waitKey) <<
            " failed, result:" << ConfigStore::ErrStr(ret));
        return SM_ERROR;
    }
    traceGetStatus.RecordEnd();

    if (getVal != SMEM_GROUP_SET_STR) {
        SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(waitKey) <<
            " val is not equal, val: " << getVal << " expect: " << SMEM_GROUP_SET_STR);
        return SM_ERROR;
    }
    traceBarrier.RecordEnd();

    SM_LOG_INFO("groupBarrier successfully, key: " << store_->GetCompleteKey(waitKey) << ", size: " <<
        size << ", timeCostUs: total(" << traceBarrier.PeriodUs() << ") add(" << traceAdd.PeriodUs() <<
        ") getStatus(" << traceGetStatus.PeriodUs() << ")");
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

Result SmemNetGroupEngine::GroupAllGather(const char *sendBuf, uint32_t sendSize, char *recvBuf, uint32_t recvSize)
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    uint32_t size = option_.rankSize;
    SM_ASSERT_RETURN(sendSize * size == recvSize, SM_INVALID_PARAM);

    std::string idx = std::to_string(groupVersion_) + "_" + std::to_string(groupSn_++);
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
        SM_LOG_AND_SET_LAST_ERROR("store add key: " << store_->GetCompleteKey(addKey) <<
            " failed, result:" << ConfigStore::ErrStr(ret));
        return SM_ERROR;
    }
    traceAppend.RecordEnd();

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
        SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(waitKey) <<
            " failed, result:" << ConfigStore::ErrStr(ret));
        return SM_ERROR;
    }
    traceGetStatus.RecordEnd();

    if (getVal != SMEM_GROUP_SET_STR) {
        SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(waitKey) << " val is not equal, val: " <<
            getVal << " expect: " << SMEM_GROUP_SET_STR);
        return SM_ERROR;
    }

    /* get the whole value */
    MonoPerfTrace traceGetData;
    std::vector<uint8_t> output;
    ret = store_->Get(addKey, output, option_.timeoutMs);
    if (ret != SM_OK || output.size() != input.size() * size) {
        SM_LOG_AND_SET_LAST_ERROR("after wait, store get key: " << store_->GetCompleteKey(addKey)
                                   << " failed, result:" << ConfigStore::ErrStr(ret)
                                   << " recv_size: " << output.size() << " input_size:" << input.size()
                                   << " group_size:" << size);
        return SM_ERROR;
    }
    traceGetData.RecordEnd();
    traceAllGather.RecordEnd();

    SortGatherRecv(output, sendSize, size, recvBuf);

    SM_LOG_INFO("allGather successfully, key: " << store_->GetCompleteKey(addKey) << ", rank: " << option_.rank <<
        ", size: " << size << ", timeCostUs: total(" << traceAllGather.PeriodUs() << ") append(" <<
        traceAppend.PeriodUs() << ") getStatus(" << traceGetStatus.PeriodUs() << ") getData(" <<
        traceGetData.PeriodUs() << ")");

    return SM_OK;
}

bool SmemNetGroupEngine::IsDigit(const std::string& str)
{
    if (str.empty()) {
        return false;
    }
    size_t start = str.find_first_not_of(" \t");
    if (start == std::string::npos) {
        return false;
    }

    for (size_t i = start; i < str.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(str[i]))) {
            return false;
        }
    }
    return true;
}

void SmemNetGroupEngine::GroupListenEvent()
{
    std::string getVal;
    std::string prevEvent;

    listenThreadStarted_ = true;
    while (!groupStoped_) {
        if (!joined_) {
            usleep(SMEM_SLEEP_TIMEOUT);
            continue;
        }

        if (listenCtx_.watchId == UINT32_MAX) {
            uint32_t wid;
            auto ret = store_->Watch(SMEM_GROUP_LISTEN_EVENT_KEY, std::bind(&SmemNetGroupEngine::GroupWatchCb, this,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), wid);
            if (ret != SM_OK) {
                SM_LOG_WARN("group watch failed, ret: " << ret);
                usleep(SMEM_SLEEP_TIMEOUT);
                continue;
            }
            listenCtx_.watchId = wid;
        }

        auto ret = listenSignal_.TimedwaitMillsecs(SMEM_LISTER_TIMEOUT);
        getVal = std::move(listenCtx_.value);
        if (groupStoped_) {
            break;
        }

        listenCtx_.watchId = UINT32_MAX;
        if (ret != SM_OK || getVal.empty()) {
            // TODO: 处理退出场景
            continue;
        }

        if (!joined_) { // maybe has leaved
            continue;
        }

        char opt = getVal[0];
        if (!IsDigit(getVal.substr(1))) {
            SM_LOG_WARN("value is not digit");
            continue;
        }
        uint32_t rk = strtol(getVal.c_str() + 1, nullptr, 10);
        if (getVal == prevEvent) {
            continue;
        }
        prevEvent = getVal;

        ret = store_->Get(SMEM_GROUP_DYNAMIC_SIZE_KEY, getVal, option_.timeoutMs);
        if (ret != SM_OK) {
            SM_LOG_ERROR("get group dynamic size failed, ret: " << ret);
            continue;
        }
        if (!IsDigit(getVal)) {
            SM_LOG_WARN("value is not digit");
            continue;
        }
        int64_t tmpVal = strtol(getVal.c_str(), nullptr, 10);
        SM_LOG_INFO("handle group event, local_rk:" << option_.rank << " event_rk:" << rk << " event:" << opt);
        UpdateGroupVersion(SplitSizeAndVersion(tmpVal).first + 1);
        if (opt == 'J') {
            option_.rankSize = SplitSizeAndVersion(tmpVal).second + 1;
            if (option_.joinCb != nullptr) {
                option_.joinCb(rk);
            }
        } else if (opt == 'L') {
            option_.rankSize = SplitSizeAndVersion(tmpVal).second - 1;
            if (option_.leaveCb != nullptr) {
                option_.leaveCb(rk);
            }
        } else {
            SM_LOG_WARN("group listen event, unknown operation:" << opt);
        }
    }
    listenThreadStarted_ = false;
}

void SmemNetGroupEngine::GroupWatchCb(int result, const std::string &key, const std::string &value)
{
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("result: " << result);
        listenCtx_.ret = SM_ERROR;
    }

    if (key != SMEM_GROUP_LISTEN_EVENT_KEY) {
        listenCtx_.ret = SM_ERROR;
    }

    listenCtx_.value = value;
    listenSignal_.PthreadSignal();
}

Result SmemNetGroupEngine::StartListenEvent()
{
    SM_ASSERT_RETURN(listenSignal_.Initialize() == SM_OK, SM_ERROR);

    std::thread th(&SmemNetGroupEngine::GroupListenEvent, this);
    while (!listenThreadStarted_) {
        usleep(SMEM_SLEEP_TIMEOUT);
    }
    listenThread_ = std::move(th);
    return SM_OK;
}

Result SmemNetGroupEngine::GroupJoin()
{
    SM_ASSERT_RETURN(option_.dynamic, SM_INVALID_PARAM);
    if (joined_) {
        return SM_OK;
    }
    std::string old;
    std::string val = "J" + std::to_string(option_.rank);
    while (true) {
        auto ret = store_->Cas(SMEM_GROUP_LISTEN_EVENT_KEY, "", val, old);
        if (ret == SM_OK && old == val) {
            break;
        }
        usleep(SMEM_SLEEP_TIMEOUT);
    }
    int64_t tmp;
    auto ret = store_->Add(SMEM_GROUP_DYNAMIC_SIZE_KEY, 0, tmp);
    if (ret != SM_OK) {
        SM_LOG_ERROR("get group dynamic size failed, ret: " << ret);
        goto join_exit;
    }

    UpdateGroupVersion(SplitSizeAndVersion(tmp).first + 1);
    option_.rankSize = SplitSizeAndVersion(tmp).second + 1;
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
    SM_ASSERT_RETURN(option_.dynamic, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(joined_, SM_NOT_STARTED);
    Result ret = 0;
    std::string old;
    std::string val = "L" + std::to_string(option_.rank);
    while (true) {
        ret = store_->Cas(SMEM_GROUP_LISTEN_EVENT_KEY, "", val, old);
        if (ret == SM_OK && old == val) {
            break;
        }
        usleep(SMEM_SLEEP_TIMEOUT);
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
    groupSn_ = 0;
}

}  // namespace smem
}  // namespace ock