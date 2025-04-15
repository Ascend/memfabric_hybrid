/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <algorithm>
#include "smem_net_group_engine.h"

namespace ock {
namespace smem {

const std::string SMEM_NET_SET_STR = "ok";
constexpr uint32_t SMEM_GATHER_PREFIX_SIZE = 4U;

Result SmemNetGroupEngine::GroupBarrier(std::string &key, uint32_t size)
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);

    std::string idx = std::to_string(operationSn_.fetch_add(1U));
    std::string addKey = key + "_" + idx + "_BA";
    std::string waitKey = key + "_" + idx + "_BW";
    int64_t val = 0;
    auto ret = store_->Add(addKey, 1, val);
    if (ret != SM_OK) {
        SM_LOG_ERROR("store add key :" << addKey << " failed, ret: " << ret);
        return SM_ERROR;
    }
    if (val == size) {
        ret = store_->Set(waitKey, SMEM_NET_SET_STR);
        if (ret != SM_OK) {
            SM_LOG_ERROR("store set key :" << waitKey << " failed, ret: " << ret);
            return SM_ERROR;
        }
    }
    std::string getVal;
    ret = store_->Get(waitKey, getVal, timeoutMs_);
    if (ret != SM_OK) {
        SM_LOG_ERROR("store get key :" << addKey << " failed, ret: " << ret);
        return SM_ERROR;
    }

    if (getVal != SMEM_NET_SET_STR) {
        SM_LOG_ERROR("store get key :" << addKey << " val is not equal, val: " <<
            getVal << " expect: " << SMEM_NET_SET_STR);
        return SM_ERROR;
    }
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

Result SmemNetGroupEngine::GroupAllGather(std::string &key, uint32_t rank, uint32_t size,
    const char *sendBuf, uint32_t sendSize, char *recvBuf, uint32_t recvSize)
{
    SM_ASSERT_RETURN(store_ != nullptr, SM_INVALID_PARAM);
    SM_ASSERT_RETURN(sendSize * size == recvSize, SM_INVALID_PARAM);

    std::string idx = std::to_string(operationSn_.fetch_add(1U));
    std::string addKey = key + "_" + idx + "_GA";
    std::string waitKey = key + "_" + idx + "_GW";

    std::vector<uint8_t> input(sendSize + SMEM_GATHER_PREFIX_SIZE);
    GatherFillRank(input, rank);
    (void)std::copy_n(sendBuf, sendSize, input.data() + SMEM_GATHER_PREFIX_SIZE);

    uint64_t val = 0;
    auto ret = store_->Append(addKey, input, val);
    if (ret != SM_OK) {
        SM_LOG_ERROR("store add key :" << addKey << " failed, ret: " << ret);
        return SM_ERROR;
    }
    if (val == input.size() * size) {
        ret = store_->Set(waitKey, SMEM_NET_SET_STR);
        if (ret != SM_OK) {
            SM_LOG_ERROR("store set key :" << waitKey << " failed, ret: " << ret);
            return SM_ERROR;
        }
    }
    std::string getVal;
    ret = store_->Get(waitKey, getVal, timeoutMs_);
    if (ret != SM_OK) {
        SM_LOG_ERROR("store get key :" << addKey << " failed, ret: " << ret);
        return SM_ERROR;
    }

    if (getVal != SMEM_NET_SET_STR) {
        SM_LOG_ERROR("store get key :" << addKey << " val is not equal, val: " <<
                                       getVal << " expect: " << SMEM_NET_SET_STR);
        return SM_ERROR;
    }

    std::vector<uint8_t> output;
    ret = store_->Get(addKey, output, timeoutMs_);
    if (ret != SM_OK || output.size() != input.size() * size) {
        SM_LOG_ERROR("after wait, store get key :" << addKey << " failed, ret: " <<
            ret << " recv_size: " << output.size());
        return SM_ERROR;
    }

    SortGatherRecv(output, sendSize, size, recvBuf);
    return SM_OK;
}

}
}