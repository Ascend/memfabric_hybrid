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

void SmemTransEntryManager::UnInitialize()
{
    ptr2EntryMap_.clear();
}

Result SmemTransEntryManager::CreateEntryByName(const std::string &name, const std::string &storeUrl,
                                                const smem_trans_config_t &config, SmemTransEntryPtr &entry)
{
    UrlExtraction extraction;
    auto ret = extraction.ExtractIpPortFromUrl(storeUrl);
    SM_VALIDATE_RETURN(ret == SM_OK, "parse store url failed: " << ret, ret);
    
    StorePtr confStore = StoreFactory::CreateStoreByUrl(storeUrl, ConfigStoreModel::CSM_CLIENT, UINT32_MAX, -1,
                                                        static_cast<int32_t>(config.initTimeout));
    SM_ASSERT_RETURN(confStore != nullptr, StoreFactory::GetFailedReason());
    confStore = StoreFactory::PrefixStore(confStore, "TRANS_");

    std::vector<uint8_t> rankIdData;
    uint32_t rank;
    ret = confStore->GetCoreStore()->Get(AutoRankingStr, rankIdData, SMEM_DEFAUT_WAIT_TIME * SECOND_TO_MILLSEC);
    if (ret == SM_OK && rankIdData.size() == sizeof(uint32_t)) {
        union Transfer {
            uint32_t rankId;
            uint8_t data[4];
        } trans{};
        std::copy_n(rankIdData.begin(), sizeof(trans.data), trans.data);
        rank = trans.rankId;
        auto tcpConfigStore = Convert<ConfigStore, ConfigStoreManager>(confStore);
        tcpConfigStore->SetRankId(static_cast<int32_t>(rank));
        SM_LOG_INFO("Success to auto ranking rankId: " << trans.rankId << " deviceId: " << deviceId_);
    } else {
        SM_LOG_ERROR("Failed to auto ranking deviceId: " << deviceId_ << ", ret: " << ret
                                                         << ", dataSize: " << rankIdData.size());
        return SM_ERROR;
    }

    /* create new trans entry */
    auto tmpEntry = SmMakeRef<SmemTransEntry>(config, name, rank, entryIdx_, confStore);
    SM_ASSERT_RETURN(tmpEntry != nullptr, SM_NEW_OBJECT_FAILED);

    /* add into set and map */
    std::lock_guard<std::mutex> guard(entryMutex_);
    ptr2EntryMap_.emplace(reinterpret_cast<uintptr_t>(tmpEntry.Get()), tmpEntry);

    /* assign out object ptr */
    entry = tmpEntry;
    SM_LOG_INFO("create new smem trans entry success. url:" << name << " rank:" << rank << " server:" << storeUrl
        << " ptr:0x" << std::hex << entry.Get() << " id:" << entryIdx_);
    entryIdx_++;
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
    SM_LOG_DEBUG("remove trans entry success, entityPtr:0x" << std::hex << ptr);

    return SM_OK;
}
} // namespace smem
} // namespace ock
