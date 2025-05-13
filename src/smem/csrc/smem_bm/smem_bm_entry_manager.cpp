/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include <thread>
#include <algorithm>

#include "smem_net_common.h"
#include "smem_net_group_engine.h"
#include "smem_bm_entry_manager.h"
#include "smem_store_factory.h"

namespace ock {
namespace smem {
SmemBmEntryManager &SmemBmEntryManager::Instance()
{
    static SmemBmEntryManager instance;
    return instance;
}

Result SmemBmEntryManager::Initialize(const std::string &storeURL, uint32_t worldSize, uint16_t deviceId,
                                      const smem_bm_config_t &config)
{
    std::lock_guard<std::mutex> guard(entryMutex_);
    if (inited_) {
        SM_LOG_WARN("smem bm manager has already initialized");
        return SM_OK;
    }

    SM_PARAM_VALIDATE(worldSize == 0, "invalid param, worldSize is 0", SM_INVALID_PARAM);
    storeURL_ = storeURL;
    worldSize_ = worldSize;
    deviceId_ = deviceId;
    config_ = config;

    auto ret = PrepareStore();
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "prepare store failed: " << ret);

    if (config_.autoRanking) {
        ret = AutoRanking();
        SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "auto ranking failed: " << ret);
    }

    SM_LOG_INFO("initialize store(" << storeURL << ") world size(" << worldSize << ") device(" << deviceId << ") OK.");
    return SM_OK;
}

int32_t SmemBmEntryManager::PrepareStore()
{
    SM_ASSERT_RETURN(storeUrlExtraction_.ExtractIpPortFromUrl(storeURL_) == SM_OK, SM_INVALID_PARAM);
    if (!config_.autoRanking) {
        SM_ASSERT_RETURN(config_.rankId >= worldSize_, SM_INVALID_PARAM);
        if (config_.rankId == 0 && config_.startConfigStore) {
            confStore_ = StoreFactory::CreateStore(storeUrlExtraction_.ip, storeUrlExtraction_.port, true, 0);
        } else {
            confStore_ = StoreFactory::CreateStore(storeUrlExtraction_.ip, storeUrlExtraction_.port, false,
                                                   static_cast<int>(config_.rankId));
        }
        SM_ASSERT_RETURN(confStore_ != nullptr, SM_ERROR);

        confStore_ = StoreFactory::PrefixStore(confStore_, "SMEM_BM#");
    } else {
        if (config_.startConfigStore) {
            SM_LOG_ERROR("AutoRanking mode not support configure store open.");
            return SM_INVALID_PARAM;
        }

        confStore_ = ock::smem::StoreFactory::CreateStore(storeUrlExtraction_.ip, storeUrlExtraction_.port, false);
    }

    return SM_OK;
}

int32_t SmemBmEntryManager::AutoRanking()
{
    std::string localIp;

    auto ret = GetLocalIpWithTarget(storeUrlExtraction_.ip, localIp);
    if (ret != 0) {
        SM_LOG_ERROR("get local ip address connect to target ip: " << storeUrlExtraction_.ip << " failed: " << ret);
        return ret;
    }

    return BarrierForAutoRanking(localIp);
}

int32_t SmemBmEntryManager::BarrierForAutoRanking(const std::string &localIp)
{
    uint16_t hostRankIdOffset = 0;
    uint16_t rankIdInHost;
    int64_t tempValue;
    int64_t hostRankCount;
    int64_t totalRankCount;
    int64_t totalHostCount;
    std::string hostRankCountKey = std::string("AutoRanking#HostRankCount#").append(localIp);
    std::string hostRankOffsetKey = std::string("AutoRanking#HostRankOffset#").append(localIp);
    std::string totalRankCountKey = std::string("AutoRanking#TotalRankCount");
    std::string totalHostCountKey = std::string("AutoRanking#TotalHostCount");
    std::string ranksInHostListKey = std::string("AutoRanking#RanksInHostList");

    auto ret = confStore_->Add(hostRankCountKey, 1L, hostRankCount);
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "store add for key: " << hostRankCountKey << " failed: " << ret);
    rankIdInHost = static_cast<uint16_t>(hostRankCount - 1L);
    if (rankIdInHost == 0U) {
        ret = confStore_->Add(totalHostCountKey, 1L, totalHostCount);
        SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "store add for key: " << totalHostCountKey << " failed: " << ret);
    }

    ret = confStore_->Add(totalRankCountKey, 1L, totalRankCount);
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "store add for key: " << totalRankCountKey << " failed: " << ret);

    while (totalRankCount < worldSize_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ret = confStore_->Add(totalRankCountKey, 0L, totalRankCount);
        SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "store read key: " << totalRankCountKey << " failed: " << ret);
    }

    ret = confStore_->Add(hostRankCountKey, 0L, hostRankCount);
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "store read key: " << hostRankCountKey << " failed: " << ret);

    ret = confStore_->Add(totalHostCountKey, 0L, totalHostCount);
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "store read key: " << totalHostCountKey << " failed: " << ret);

    SmemNetGroupEngine hostGroup{confStore_, static_cast<uint32_t>(hostRankCount), rankIdInHost, SMEM_DEFAUT_WAIT_TIME};
    std::vector<uint16_t> receiveDevices(hostRankCount);
    ret = hostGroup.GroupAllGather((const char *)&deviceId_, sizeof(deviceId_), (char *)receiveDevices.data(),
                                   receiveDevices.size() * sizeof(deviceId_));
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "store based all gather failed: " << ret);

    std::sort(receiveDevices.begin(), receiveDevices.end(), std::less<uint16_t>());
    for (auto i = 0U; i < receiveDevices.size(); ++i) {
        if (receiveDevices[i] == deviceId_) {
            rankIdInHost = i;
        }
    }

    if (rankIdInHost == 0) {
        uint64_t hostReportCount;
        std::vector<uint8_t> ranksInHost;
        ranksInHost.emplace_back(static_cast<uint8_t>(hostRankCount));
        ret = confStore_->Append(ranksInHostListKey, ranksInHost, hostReportCount);
        SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "store append for key: " << ranksInHostListKey << " failed: " << ret);

        ret = confStore_->Get(ranksInHostListKey, ranksInHost, SMEM_DEFAUT_WAIT_TIME);
        SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "store get for key: " << ranksInHostListKey << " failed: " << ret);
        for (auto i = 0U; i < hostReportCount; i++) {
            hostRankIdOffset += ranksInHost[i];
        }

        ret = confStore_->Add(hostRankOffsetKey, hostRankIdOffset, tempValue);
        SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "store add for key: " << hostRankOffsetKey << " failed: " << ret);
    }

    ret = confStore_->Add(hostRankOffsetKey, 0L, tempValue);
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "store read key: " << hostRankOffsetKey << " failed: " << ret);
    hostRankIdOffset = static_cast<uint16_t>(tempValue);
    config_.rankId = hostRankIdOffset + rankIdInHost;

    return 0;
}

}  // namespace smem
}  // namespace ock
