/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <algorithm>
#include "smem_net_group_engine.h"

namespace ock {
namespace smem {

const std::string SMEM_NET_SET_STR = "ok";
constexpr uint32_t SMEM_GATHER_PREFIX_SIZE = 4U;

Result SmemNetGroupEngine::GroupBarrier()
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    uint32_t size = rankSize_;
    std::string idx = std::to_string(groupSn_.fetch_add(1U));
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
        ret = store_->Set(waitKey, SMEM_NET_SET_STR);
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
    ret = store_->Get(waitKey, getVal, timeoutMs_);
    if (ret != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(waitKey) <<
            " failed, result:" << ConfigStore::ErrStr(ret));
        return SM_ERROR;
    }
    traceGetStatus.RecordEnd();

    if (getVal != SMEM_NET_SET_STR) {
        SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(waitKey) <<
            " val is not equal, val: " << getVal << " expect: " << SMEM_NET_SET_STR);
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
    uint32_t size = rankSize_;
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(sendSize * size == recvSize, SM_INVALID_PARAM);

    std::string idx = std::to_string(groupSn_.fetch_add(1U));
    std::string addKey = idx + "_GA";
    std::string waitKey = idx + "_GW";

    std::vector<uint8_t> input(sendSize + SMEM_GATHER_PREFIX_SIZE);
    GatherFillRank(input, localRank_);
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
        ret = store_->Set(waitKey, SMEM_NET_SET_STR);
        if (ret != SM_OK) {
            SM_LOG_AND_SET_LAST_ERROR("store set key: " << store_->GetCompleteKey(waitKey)
                                       << " failed, result:" << ConfigStore::ErrStr(ret));
            return SM_ERROR;
        }
    }

    /* all guys wait for ok status with timeout */
    MonoPerfTrace traceGetStatus;
    std::string getVal;
    ret = store_->Get(waitKey, getVal, timeoutMs_);
    if (ret != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(waitKey) <<
            " failed, result:" << ConfigStore::ErrStr(ret));
        return SM_ERROR;
    }
    traceGetStatus.RecordEnd();

    if (getVal != SMEM_NET_SET_STR) {
        SM_LOG_AND_SET_LAST_ERROR("store get key: " << store_->GetCompleteKey(waitKey) << " val is not equal, val: " <<
            getVal << " expect: " << SMEM_NET_SET_STR);
        return SM_ERROR;
    }

    /* get the whole value */
    MonoPerfTrace traceGetData;
    std::vector<uint8_t> output;
    ret = store_->Get(addKey, output, timeoutMs_);
    if (ret != SM_OK || output.size() != input.size() * size) {
        SM_LOG_AND_SET_LAST_ERROR("after wait, store get key: " << store_->GetCompleteKey(addKey)
                                   << " failed, result:" << ConfigStore::ErrStr(ret)
                                   << " recv_size: " << output.size());
        return SM_ERROR;
    }
    traceGetData.RecordEnd();
    traceAllGather.RecordEnd();

    SortGatherRecv(output, sendSize, size, recvBuf);

    SM_LOG_INFO("allGather successfully, key: " << store_->GetCompleteKey(addKey) << ", rank: " << localRank_ <<
        ", size: " << size << ", timeCostUs: total(" << traceAllGather.PeriodUs() << ") append(" <<
        traceAppend.PeriodUs() << ") getStatus(" << traceGetStatus.PeriodUs() << ") getData(" <<
        traceGetData.PeriodUs() << ")");

    return SM_OK;
}

}  // namespace smem
}  // namespace ock