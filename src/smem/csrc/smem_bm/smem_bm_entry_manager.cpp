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
    uint32_t localIpv4;
    std::string localIp;
    auto ret = GetLocalIpWithTarget(storeUrlExtraction_.ip, localIp, localIpv4);
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

int32_t SmemBmEntryManager::AutoRanking()
{
    uint32_t localIpv4;
    std::string localIp;

    auto ret = GetLocalIpWithTarget(storeUrlExtraction_.ip, localIp, localIpv4);
    if (ret != 0) {
        SM_LOG_ERROR("get local ip address connect to target ip: " << storeUrlExtraction_.ip << " failed: " << ret);
        return ret;
    }

    std::string rankTableKey = std::string("AutoRanking#RankTables");
    std::string sortedRankTableKey = std::string("AutoRanking#SortedRankTables");
    RankTable rt{localIpv4, deviceId_};
    uint64_t size;
    std::vector<uint8_t> rtv{(uint8_t *)&rt, (uint8_t *)&rt + sizeof(rt)};
    ret = confStore_->Append(rankTableKey, rtv, size);
    SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "append key: " << rankTableKey << " failed: " << ret);

    std::vector<RankTable> ranks;
    if (size == sizeof(rt) * worldSize_) {
        ret = confStore_->Get(rankTableKey, rtv, SMEM_DEFAUT_WAIT_TIME * SECOND_TO_MILLSEC);
        SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "get key: " << rankTableKey << " failed: " << ret);

        ret = confStore_->Remove(rankTableKey);
        SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "remove key: " << rankTableKey << " failed: " << ret);

        ranks = std::vector<RankTable>{(RankTable *)rtv.data(), (RankTable *)rtv.data() + worldSize_};
        std::sort(ranks.begin(), ranks.end(), RankTable::Less);

        rtv = std::vector<uint8_t>{(uint8_t *)ranks.data(), (uint8_t *)ranks.data() + sizeof(RankTable) * worldSize_};
        ret = confStore_->Set(sortedRankTableKey, rtv);
        SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "set key: " << sortedRankTableKey << " failed: " << ret);
    } else {
        ret = confStore_->Get(sortedRankTableKey, rtv, SMEM_DEFAUT_WAIT_TIME * SECOND_TO_MILLSEC);
        SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "get key: " << sortedRankTableKey << " failed: " << ret);
        ranks = std::vector<RankTable>{(RankTable *)rtv.data(), (RankTable *)rtv.data() + worldSize_};
    }

    for (auto i = 0U; i < ranks.size(); ++i) {
        if (ranks[i].ipv4 == localIpv4 && ranks[i].deviceId == deviceId_) {
            config_.rankId = i;
            break;
        }
    }

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

    SM_LOG_DEBUG("not found bm entry with ptr " << ptr);
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
        SM_LOG_DEBUG("not found bm entry with ptr " << ptr);
        return SM_OBJECT_NOT_EXISTS;
    }

    /* assign to a tmp ptr and remove from map */
    auto entry = iter->second;
    ptr2EntryMap_.erase(iter);

    /* remove from id set */
    SM_ASSERT_RETURN(entry != nullptr, SM_ERROR);
    entryIdMap_.erase(entry->Id());

    SM_LOG_DEBUG("remove bm entry success, ptr: " << ptr << ", id: " << entry->Id());

    return SM_OK;
}
}  // namespace smem
}  // namespace ock
