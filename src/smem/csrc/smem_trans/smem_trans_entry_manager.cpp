/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "smem_trans_entry_manager.h"

#include "smem_net_common.h"
#include "smem_net_group_engine.h"
#include "smem_store_factory.h"

namespace ock {
namespace smem {
SmemTransEntryManager &SmemTransEntryManager::Instance()
{
#ifdef UT_ENABLED
    static thread_local SmemTransEntryManager instance;
#else
    static SmemTransEntryManager instance;
#endif
    return instance;
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

    /* create new shm entry */
    SmemStoreHelper storeHelper{name, storeUrl, config.role};
    auto tmpEntry = SmMakeRef<SmemTransEntry>(name, storeHelper);
    SM_ASSERT_RETURN(tmpEntry != nullptr, SM_NEW_OBJECT_FAILED);

    /* add into set and map */
    name2EntryMap_.emplace(name, tmpEntry);
    ptr2EntryMap_.emplace(reinterpret_cast<uintptr_t>(tmpEntry.Get()), tmpEntry);

    /* assign out object ptr */
    entry = tmpEntry;

    SM_LOG_DEBUG("create new shm entry success.");
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
}
}
