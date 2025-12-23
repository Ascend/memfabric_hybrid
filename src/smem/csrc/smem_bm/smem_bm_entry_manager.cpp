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
#include <thread>
#include <algorithm>

#include "smem_net_common.h"
#include "smem_net_group_engine.h"
#include "smem_store_factory.h"
#include "smem_tcp_config_store.h"
#include "smem_bm_entry_manager.h"

namespace ock {
namespace smem {
#pragma pack(push, 1)
struct RankTable {
    uint32_t ipv4;
    uint8_t deviceId;
    RankTable(): ipv4{0}, deviceId{0} {}
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

SmemBmEntryManager::~SmemBmEntryManager()
{
    // 防止析构函数里面调用UnInitalize
    for (auto &pair : ptr2EntryMap_) {
        pair.second->UnInitalize();
    }
    ptr2EntryMap_.clear();
    for (auto &map : entryIdMap_) {
        map.second->UnInitalize();
    }
    entryIdMap_.clear();
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
    StoreFactory::SetTlsInfo(config_.storeTlsConfig);
    if (!config_.autoRanking) {
        SM_ASSERT_RETURN(config_.rankId < worldSize_, SM_INVALID_PARAM);
        confStore_ = StoreFactory::CreateStore(storeUrlExtraction_.ip, storeUrlExtraction_.port,
            (config_.rankId == 0 && config_.startConfigStoreServer), worldSize_, static_cast<int>(config_.rankId));
        SM_ASSERT_RETURN(confStore_ != nullptr, StoreFactory::GetFailedReason());
    } else {
        if (config_.startConfigStoreServer) {
            auto ret = RacingForStoreServer();
            SM_ASSERT_RETURN(ret == SM_OK, ret);
        }

        if (confStore_ == nullptr) {
            confStore_ = StoreFactory::CreateStoreClient(storeUrlExtraction_.ip, storeUrlExtraction_.port, worldSize_);
            SM_ASSERT_RETURN(confStore_ != nullptr, StoreFactory::GetFailedReason());
        }
    }
    confStore_ = StoreFactory::PrefixStore(confStore_, "SMEM_BM_");
    return SM_OK;
}

int32_t SmemBmEntryManager::RacingForStoreServer()
{
    std::string localIp;
    auto ret = GetLocalIpWithTarget(storeUrlExtraction_.ip, localIp);
    SM_ASSERT_RETURN(ret == SM_OK, SM_ERROR);
    if (localIp != storeUrlExtraction_.ip) {
        return SM_OK;
    }

    confStore_ = StoreFactory::CreateStore(storeUrlExtraction_.ip, storeUrlExtraction_.port, true, worldSize_);
    if (confStore_ != nullptr || StoreFactory::GetFailedReason() == SM_RESOURCE_IN_USE) {
        return SM_OK;
    }

    return StoreFactory::GetFailedReason();
}

int32_t SmemBmEntryManager::AutoRanking()
{
    std::string localIp;
    std::vector<uint8_t> rankIdDate;
    auto ret = confStore_->GetCoreStore()->Get(AutoRankingStr, rankIdDate,
                                               SMEM_DEFAUT_WAIT_TIME * SECOND_TO_MILLSEC);
    if (ret == SM_OK && rankIdDate.size() == sizeof(uint32_t)) {
        union Transfer {
            uint32_t rankId;
            uint8_t date[4];
        } trans{};
        std::copy_n(rankIdDate.begin(), sizeof(trans.date), trans.date);
        config_.rankId = trans.rankId;
        auto tcpConfigStore = Convert<ConfigStore, TcpConfigStore>(confStore_->GetCoreStore());
        tcpConfigStore->SetRankId(config_.rankId);
        SM_LOG_INFO("Success to auto ranking rankId: " << trans.rankId << " localIp: "
                                                       << localIp << " deviceId: " << deviceId_);
        return SM_OK;
    }
    SM_LOG_ERROR("Failed to auto ranking deviceId: " << deviceId_);
    return SM_ERROR;
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
    SmemBmEntryOptions opt{ id, config_.rankId, config_.dynamicWorldSize, config_.controlOperationTimeout };
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
    std::lock_guard<std::mutex> guard(entryMutex_);
    inited_ = false;
    confStore_ = nullptr;
    StoreFactory::DestroyStore(storeUrlExtraction_.ip, storeUrlExtraction_.port);
}

}  // namespace smem
}  // namespace ock