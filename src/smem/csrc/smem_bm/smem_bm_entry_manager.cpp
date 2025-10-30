/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <thread>
#include <algorithm>

#include "smem_net_common.h"
#include "smem_net_group_engine.h"
#include "smem_store_factory.h"
#include "smem_bm_entry_manager.h"

namespace ock {
namespace smem {
#pragma pack(push, 1)
struct RankTable {
    uint32_t ipv4;
    uint8_t deviceId;
    RankTable() : ipv4{0}, deviceId{0} {}
    RankTable(uint32_t ip, uint16_t dev) : ipv4{ip}, deviceId{static_cast<uint8_t>(dev)} {}

    static bool Less(const RankTable &r1, const RankTable &r2)
    {
        if (r1.ipv4 != r2.ipv4) {
            return r1.ipv4 < r2.ipv4;
        }

        return r1.deviceId < r2.deviceId;
    }
};
struct RankTableV6 {
    uint8_t ipv6[16];
    uint8_t deviceId;
    RankTableV6() : ipv6{}, deviceId{0} {}
    RankTableV6(uint8_t ip[16], uint16_t dev) : deviceId{static_cast<uint8_t>(dev)}
    {
        constexpr int SIZE = 16;
        std::copy(ip, ip + SIZE, ipv6);
    }

    static bool Less(const RankTableV6 &r1, const RankTableV6 &r2)
    {
        constexpr int SIZE = 16;
        for (size_t i = 0; i < SIZE; i++) {
            if (r1.ipv6[i] != r2.ipv6[i]) {
                return r1.ipv6[i] < r2.ipv6[i];
            }
        }

        return r1.deviceId < r2.deviceId;
    }
};
#pragma pack(pop)

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

    SM_VALIDATE_RETURN(worldSize != 0, "invalid param, worldSize is 0", SM_INVALID_PARAM);

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

    inited_ = true;
    SM_LOG_INFO("initialize store(" << storeURL << ") world size(" << worldSize << ") device(" << deviceId << ") OK.");
    return SM_OK;
}

int32_t SmemBmEntryManager::PrepareStore()
{
    SM_ASSERT_RETURN(storeUrlExtraction_.ExtractIpPortFromUrl(storeURL_) == SM_OK, SM_INVALID_PARAM);
    if (!config_.autoRanking) {
        SM_ASSERT_RETURN(config_.rankId < worldSize_, SM_INVALID_PARAM);
        if (config_.rankId == 0 && config_.startConfigStore) {
            confStore_ = StoreFactory::CreateStore(storeUrlExtraction_.ip, storeUrlExtraction_.port, true, 0);
            SM_LOG_INFO("smem bm start store server success, rk: " << config_.rankId);
        } else {
            confStore_ = StoreFactory::CreateStore(storeUrlExtraction_.ip, storeUrlExtraction_.port, false,
                                                   static_cast<int>(config_.rankId));
        }
        SM_ASSERT_RETURN(confStore_ != nullptr, StoreFactory::GetFailedReason());
    } else {
        if (config_.startConfigStore) {
            auto ret = RacingForStoreServer();
            SM_ASSERT_RETURN(ret == SM_OK, ret);
        }

        if (confStore_ == nullptr) {
            confStore_ = StoreFactory::CreateStore(storeUrlExtraction_.ip, storeUrlExtraction_.port, false);
            SM_ASSERT_RETURN(confStore_ != nullptr, StoreFactory::GetFailedReason());
        }
    }
    confStore_ = StoreFactory::PrefixStore(confStore_, "SMEM_BM_");

    return SM_OK;
}

int32_t SmemBmEntryManager::RacingForStoreServer()
{
    mf_ip_addr localAddress;
    std::string localIp;
    auto ret = GetLocalIpWithTarget(storeUrlExtraction_.ip, localIp, localAddress);
    SM_ASSERT_RETURN(ret == SM_OK, SM_ERROR);
    if (localIp != storeUrlExtraction_.ip) {
        return SM_OK;
    }

    confStore_ = StoreFactory::CreateStore(storeUrlExtraction_.ip, storeUrlExtraction_.port, true);
    if (confStore_ != nullptr || StoreFactory::GetFailedReason() == SM_RESOURCE_IN_USE) {
        return SM_OK;
    }

    return StoreFactory::GetFailedReason();
}

int32_t SmemBmEntryManager::ProcessRankTableByIPTypeWhenIpv6(mf_ip_addr localAddress, uint64_t size,
                                                             std::string rankTableKey, std::string sortedRankTableKey,
                                                             std::vector<uint8_t> &rtv)
{
    int32_t ret = SM_OK;
    std::vector<RankTableV6> ranks;
    if (size == sizeof(RankTableV6) * worldSize_) {
        ret = confStore_->Get(rankTableKey, rtv, SMEM_DEFAUT_WAIT_TIME * SECOND_TO_MILLSEC);
        SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "get key: " << rankTableKey << " failed: " << ret);

        ret = confStore_->Remove(rankTableKey);
        SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "remove key: " << rankTableKey << " failed: " << ret);

        ranks = std::vector<RankTableV6>{(RankTableV6 *)rtv.data(), (RankTableV6 *)rtv.data() + worldSize_};
        std::sort(ranks.begin(), ranks.end(), RankTableV6::Less);

        rtv = std::vector<uint8_t>{(uint8_t *)ranks.data(), (uint8_t *)ranks.data() +
            sizeof(RankTableV6) * worldSize_};
        ret = confStore_->Set(sortedRankTableKey, rtv);
        SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "set key: " << sortedRankTableKey << " failed: " << ret);
    } else {
        ret = confStore_->Get(sortedRankTableKey, rtv, SMEM_DEFAUT_WAIT_TIME * SECOND_TO_MILLSEC);
        SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "get key: " << sortedRankTableKey << " failed: " << ret);
        ranks = std::vector<RankTableV6>{(RankTableV6 *)rtv.data(), (RankTableV6 *)rtv.data() + worldSize_};
    }

    constexpr int SIZE = 16;
    for (auto i = 0U; i < ranks.size(); ++i) {
        if (std::equal(ranks[i].ipv6, ranks[i].ipv6 + SIZE, localAddress.addr.addrv6) &&
            ranks[i].deviceId == deviceId_) {
            config_.rankId = i;
            break;
        }
    }
    return ret;
}

int32_t SmemBmEntryManager::ProcessRankTableByIPType(mf_ip_addr localAddress, uint64_t size,
                                                     std::string rankTableKey, std::string sortedRankTableKey,
                                                     std::vector<uint8_t> &rtv)
{
    int32_t ret = SM_OK;
    if (localAddress.type == IpV4) {
        std::vector<RankTable> ranks;
        if (size == sizeof(RankTable) * worldSize_) {
            ret = confStore_->Get(rankTableKey, rtv, SMEM_DEFAUT_WAIT_TIME * SECOND_TO_MILLSEC);
            SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "get key: " << rankTableKey << " failed: " << ret);

            ret = confStore_->Remove(rankTableKey);
            SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "remove key: " << rankTableKey << " failed: " << ret);

            ranks = std::vector<RankTable>{(RankTable *)rtv.data(), (RankTable *)rtv.data() + worldSize_};
            std::sort(ranks.begin(), ranks.end(), RankTable::Less);

            rtv = std::vector<uint8_t>{(uint8_t *)ranks.data(), (uint8_t *)ranks.data() +
                sizeof(RankTable) * worldSize_};
            ret = confStore_->Set(sortedRankTableKey, rtv);
            SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "set key: " << sortedRankTableKey << " failed: " << ret);
        } else {
            ret = confStore_->Get(sortedRankTableKey, rtv, SMEM_DEFAUT_WAIT_TIME * SECOND_TO_MILLSEC);
            SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "get key: " << sortedRankTableKey << " failed: " << ret);
            ranks = std::vector<RankTable>{(RankTable *)rtv.data(), (RankTable *)rtv.data() + worldSize_};
        }

        for (auto i = 0U; i < ranks.size(); ++i) {
            if (ranks[i].ipv4 == localAddress.addr.addrv4 && ranks[i].deviceId == deviceId_) {
                config_.rankId = i;
                break;
            }
        }
    } else if (localAddress.type == IpV6) {
        ret = ProcessRankTableByIPTypeWhenIpv6(localAddress, size, rankTableKey, sortedRankTableKey, rtv);
    }
    return ret;
}

int32_t SmemBmEntryManager::AutoRanking()
{
    mf_ip_addr localAddress;
    std::string localIp;

    auto ret = GetLocalIpWithTarget(storeUrlExtraction_.ip, localIp, localAddress);
    if (ret != 0) {
        SM_LOG_ERROR("get local ip address connect to target ip failed: " << ret);
        return ret;
    }

    std::string rankTableKey = std::string("AutoRanking#RankTables");
    std::string sortedRankTableKey = std::string("AutoRanking#SortedRankTables");
    uint64_t size;
    std::vector<uint8_t> rtv {};
    if (localAddress.type == IpV4) {
        RankTable rt{localAddress.addr.addrv4, deviceId_};
        rtv = std::vector<uint8_t>{(uint8_t *)&rt, (uint8_t *)&rt + sizeof(rt)};
    } else if (localAddress.type == IpV6) {
        RankTableV6 rt{localAddress.addr.addrv6, deviceId_};
        rtv = std::vector<uint8_t>{(uint8_t *)&rt, (uint8_t *)&rt + sizeof(rt)};
    }
    ret = confStore_->Append(rankTableKey, rtv, size);
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "append key: " << rankTableKey << " failed: " << ret);

    ret = ProcessRankTableByIPType(localAddress, size, rankTableKey, sortedRankTableKey, rtv);
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "process rank table failed: " << ret);

    return SM_OK;
}

Result SmemBmEntryManager::CreateEntryById(uint32_t id, SmemBmEntryPtr &entry /* out */)
{
    std::lock_guard<std::mutex> guard(entryMutex_);
    /* look up the bm entry exists or not with lock */
    SM_ASSERT_RETURN(inited_, SM_NOT_STARTED);
    auto iter = entryIdMap_.find(id);
    if (iter != entryIdMap_.end()) {
        SM_LOG_WARN("create bm entry failed as already exists, id: " << id);
        return SM_DUPLICATED_OBJECT;
    }

    /* create new bm entry */
    SmemBmEntryOptions opt{id, config_.rankId, config_.dynamicWorldSize, config_.controlOperationTimeout};
    auto store = StoreFactory::PrefixStore(confStore_, std::string("(").append(std::to_string(id)).append(")_"));
    if (store == nullptr) {
        SM_LOG_ERROR("create new prefix store for entity: " << id << " failed");
        return SM_ERROR;
    }

    auto tmpEntry = SmMakeRef<SmemBmEntry>(opt, store);
    SM_ASSERT_RETURN(tmpEntry != nullptr, SM_NEW_OBJECT_FAILED);

    /* add into set and map */
    entryIdMap_.emplace(id, tmpEntry);
    ptr2EntryMap_.emplace(reinterpret_cast<uintptr_t>(tmpEntry.Get()), tmpEntry);

    /* assign out object ptr */
    entry = tmpEntry;
    SM_LOG_DEBUG("create new bm entry success, id: " << id);
    return SM_OK;
}

Result SmemBmEntryManager::GetEntryByPtr(uintptr_t ptr, SmemBmEntryPtr &entry)
{
    std::lock_guard<std::mutex> guard(entryMutex_);
    /* look up the bm entry exists or not with lock */
    SM_ASSERT_RETURN(inited_, SM_NOT_STARTED);
    auto iter = ptr2EntryMap_.find(ptr);
    if (iter != ptr2EntryMap_.end()) {
        entry = iter->second;
        return SM_OK;
    }

    SM_LOG_DEBUG("not found bm entry");
    return SM_OBJECT_NOT_EXISTS;
}

Result SmemBmEntryManager::GetEntryById(uint32_t id, SmemBmEntryPtr &entry)
{
    std::lock_guard<std::mutex> guard(entryMutex_);
    /* look up the bm entry exists or not with lock */
    SM_ASSERT_RETURN(inited_, SM_NOT_STARTED);
    auto iter = entryIdMap_.find(id);
    if (iter != entryIdMap_.end()) {
        entry = iter->second;
        return SM_OK;
    }

    SM_LOG_DEBUG("not found bm entry with id " << id);
    return SM_OBJECT_NOT_EXISTS;
}

Result SmemBmEntryManager::RemoveEntryByPtr(uintptr_t ptr)
{
    std::lock_guard<std::mutex> guard(entryMutex_);
    /* look up the bm entry exists or not with lock */
    SM_ASSERT_RETURN(inited_, SM_NOT_STARTED);
    auto iter = ptr2EntryMap_.find(ptr);
    if (iter == ptr2EntryMap_.end()) {
        SM_LOG_DEBUG("not found bm entry");
        return SM_OBJECT_NOT_EXISTS;
    }

    /* assign to a tmp ptr and remove from map */
    auto entry = iter->second;
    ptr2EntryMap_.erase(iter);

    /* remove from id set */
    SM_ASSERT_RETURN(entry != nullptr, SM_ERROR);
    entryIdMap_.erase(entry->Id());

    SM_LOG_DEBUG("remove bm entry success, id: " << entry->Id());

    return SM_OK;
}

void SmemBmEntryManager::Destroy()
{
    inited_ = false;
    confStore_ = nullptr;
    StoreFactory::DestroyStore(storeUrlExtraction_.ip, storeUrlExtraction_.port);
}

}  // namespace smem
}  // namespace ock
