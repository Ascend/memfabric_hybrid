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
#include "smem_trans_entry_manager.h"

#include "smem_net_common.h"
#include "smem_net_group_engine.h"
#include "smem_store_factory.h"

namespace ock {
namespace smem {
SmemTransEntryManager &SmemTransEntryManager::Instance()
{
    static SmemTransEntryManager instance;
    return instance;
}

Result SmemTransEntryManager::Initialize(const std::string &storeUrl, int32_t maxRetry)
{
    UrlExtraction extraction;
    auto ret = extraction.ExtractIpPortFromUrl(storeUrl);
    SM_VALIDATE_RETURN(ret == SM_OK, "parse store url failed: " << ret, ret);

    confStore_ = StoreFactory::CreateStoreByUrl(storeUrl, false, UINT32_MAX, -1, maxRetry);
    SM_ASSERT_RETURN(confStore_ != nullptr, StoreFactory::GetFailedReason());
    confStore_ = StoreFactory::PrefixStore(confStore_, "TRANS_");

    std::vector<uint8_t> rankIdData;
    ret = confStore_->GetCoreStore()->Get(AutoRankingStr, rankIdData, SMEM_DEFAUT_WAIT_TIME * SECOND_TO_MILLSEC);
    if (ret == SM_OK && rankIdData.size() == sizeof(uint32_t)) {
        union Transfer {
            uint32_t rankId;
            uint8_t data[4];
        } trans{};
        std::copy_n(rankIdData.begin(), sizeof(trans.data), trans.data);
        rank_ = trans.rankId;
        auto tcpConfigStore = Convert<ConfigStore, ConfigStoreManager>(confStore_);
        tcpConfigStore->SetRankId(static_cast<int32_t>(rank_));
        SM_LOG_INFO("Success to auto ranking rankId: " << trans.rankId << " deviceId: " << deviceId_);
        storeUrl_ = storeUrl;
        return SM_OK;
    }
    SM_LOG_ERROR("Failed to auto ranking deviceId: " << deviceId_ << ", ret: " << ret
                                                     << ", dataSize: " << rankIdData.size());
    return SM_ERROR;
}

void SmemTransEntryManager::UnInitialize()
{
    confStore_ = nullptr;
    storeUrl_.clear();
    rank_ = UINT32_MAX;
    ptr2EntryMap_.clear();
    name2EntryMap_.clear();
}

Result SmemTransEntryManager::CreateEntryByName(const std::string &name, const std::string &storeUrl,
                                                const smem_trans_config_t &config, SmemTransEntryPtr &entry)
{
    std::lock_guard<std::mutex> guard(entryMutex_);
    /* look up the shm entry exists or not with lock */
    auto iter = name2EntryMap_.find(name);
    if (iter != name2EntryMap_.end()) {
        SM_LOG_WARN("create shm entry failed as already exists.");
        return SM_DUPLICATED_OBJECT;
    }

    if (confStore_ != nullptr) {
        if (storeUrl_ != storeUrl) {
            SM_LOG_ERROR("has connect to " << storeUrl_ << ", can't connet to " << storeUrl);
            return SM_INVALID_PARAM;
        }
    } else {
        auto ret = Initialize(storeUrl, static_cast<int32_t>(config.initTimeout));
        SM_ASSERT_RETURN(ret == SM_OK, ret);
    }

    auto store = StoreFactory::PrefixStore(confStore_, std::to_string(entryIdx_) + "_");
    if (store == nullptr) {
        SM_LOG_ERROR("create new prefix store for entity: " << name << " failed");
        return SM_ERROR;
    }

    /* create new trans entry */
    auto tmpEntry = SmMakeRef<SmemTransEntry>(config, name, rank_, entryIdx_, store);
    SM_ASSERT_RETURN(tmpEntry != nullptr, SM_NEW_OBJECT_FAILED);

    /* add into set and map */
    name2EntryMap_.emplace(name, tmpEntry);
    ptr2EntryMap_.emplace(reinterpret_cast<uintptr_t>(tmpEntry.Get()), tmpEntry);

    /* assign out object ptr */
    entry = tmpEntry;
    entryIdx_++;
    SM_LOG_DEBUG("create new smem trans entry success.");
    return SM_OK;
}

Result SmemTransEntryManager::GetEntryByPtr(uintptr_t ptr, SmemTransEntryPtr &entry)
{
    std::lock_guard<std::mutex> guard(entryMutex_);
    /* look up the trans entry exists or not with lock */
    auto iter = ptr2EntryMap_.find(ptr);
    if (iter != ptr2EntryMap_.end()) {
        entry = iter->second;
        return SM_OK;
    }

    SM_LOG_DEBUG("not found trans entry");
    return SM_OBJECT_NOT_EXISTS;
}

Result SmemTransEntryManager::GetEntryByName(const std::string &name, SmemTransEntryPtr &entry)
{
    std::lock_guard<std::mutex> guard(entryMutex_);
    /* look up the trans entry exists or not with lock */
    auto iter = name2EntryMap_.find(name);
    if (iter != name2EntryMap_.end()) {
        entry = iter->second;
        return SM_OK;
    }

    SM_LOG_DEBUG("not found trans entry.");
    return SM_OBJECT_NOT_EXISTS;
}

Result SmemTransEntryManager::RemoveEntryByPtr(uintptr_t ptr)
{
    std::lock_guard<std::mutex> guard(entryMutex_);
    /* look up the trans entry exists or not with lock */
    auto iter = ptr2EntryMap_.find(ptr);
    if (iter == ptr2EntryMap_.end()) {
        SM_LOG_DEBUG("not found trans entry");
        return SM_OBJECT_NOT_EXISTS;
    }

    /* assign to a tmp ptr and remove from map */
    auto entry = iter->second;
    ptr2EntryMap_.erase(iter);

    /* remove from id set */
    SM_ASSERT_RETURN(entry != nullptr, SM_ERROR);
    name2EntryMap_.erase(entry->Name());

    SM_LOG_DEBUG("remove trans entry success");

    return SM_OK;
}

Result SmemTransEntryManager::RemoveEntryByName(const std::string &name)
{
    std::lock_guard<std::mutex> guard(entryMutex_);
    /* look up the trans entry exists or not with lock */
    auto iter = name2EntryMap_.find(name);
    if (iter == name2EntryMap_.end()) {
        SM_LOG_DEBUG("not found trans entry.");
        return SM_OBJECT_NOT_EXISTS;
    }

    /* assign to a tmp ptr and remove from map */
    auto entry = iter->second;
    name2EntryMap_.erase(iter);

    /* remove from id set */
    SM_ASSERT_RETURN(entry != nullptr, SM_ERROR);
    auto ptr = reinterpret_cast<uintptr_t>(entry.Get());
    ptr2EntryMap_.erase(ptr);

    SM_LOG_DEBUG("remove trans entry success");

    return SM_OK;
}
} // namespace smem
} // namespace ock
