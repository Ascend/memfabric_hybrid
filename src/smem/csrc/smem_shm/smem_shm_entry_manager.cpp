/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "smem_shm_entry_manager.h"
#include "smem_net_common.h"
#include "smem_store_factory.h"

namespace ock {
namespace smem {
SmemShmEntryManager &SmemShmEntryManager::Instance()
{
#ifdef UT_ENABLED
    static thread_local SmemShmEntryManager instance;
#else
    static SmemShmEntryManager instance;
#endif
    return instance;
}

Result SmemShmEntryManager::Initialize(const char *configStoreIpPort, uint32_t worldSize, uint32_t rankId,
                                       uint16_t deviceId, smem_shm_config_t *config)
{
    std::lock_guard<std::mutex> guard(entryMutex_);
    if (inited_) {
        SM_LOG_WARN("smem shm has inited!");
        return SM_OK;
    }

    SM_PARAM_VALIDATE(config == nullptr, "Invalid param, config is NULL", SM_INVALID_PARAM);
    SM_PARAM_VALIDATE(configStoreIpPort == nullptr, "Invalid param, ipPort is NULL", SM_INVALID_PARAM);

    UrlExtraction option;
    std::string url(configStoreIpPort);
    SM_ASSERT_RETURN(option.ExtractIpPortFromUrl(url) == SM_OK, SM_INVALID_PARAM);

    if (rankId == 0 && config->startConfigStore) {
        storeServer_ = ock::smem::StoreFactory::CreateStore(option.ip, option.port, true, 0);
        SM_ASSERT_RETURN(storeServer_ != nullptr, SM_ERROR);
    }
    storeClient_ = ock::smem::StoreFactory::CreateStore(option.ip, option.port, false, static_cast<int32_t>(rankId));
    SM_ASSERT_RETURN(storeClient_ != nullptr, SM_ERROR);

    // TODO: acc link开放建链超时接口
    // TODO: 确保所有client都建链

    config_ = *config;
    deviceId_ = deviceId;
    inited_ = true;
    return SM_OK;
}

Result SmemShmEntryManager::CreateEntryById(uint32_t id, SmemShmEntryPtr &entry /* out */)
{
    std::lock_guard<std::mutex> guard(entryMutex_);
    /* look up the shm entry exists or not with lock */
    SM_ASSERT_RETURN(inited_, SM_NOT_STARTED);
    auto iter = entryIdSet_.find(id);
    if (iter != entryIdSet_.end()) {
        SM_LOG_WARN("Failed to create shm entry with id " << id << " as already exists");
        return SM_DUPLICATED_OBJECT;
    }

    /* create new shm entry */
    auto tmpEntry = SmMakeRef<SmemShmEntry>(id);
    SM_ASSERT_RETURN(tmpEntry != nullptr, SM_NEW_OBJECT_FAILED);

    /* add into set and map */
    entryIdSet_.emplace(id);
    ptr2EntryMap_.emplace(reinterpret_cast<uintptr_t>(tmpEntry.Get()), tmpEntry);

    /* assign out object ptr */
    entry = tmpEntry;
    entry->SetConfig(config_);

    SM_LOG_DEBUG("Create new shm entry with id " << id);
    return SM_OK;
}

Result SmemShmEntryManager::GetEntryByPtr(uintptr_t ptr, SmemShmEntryPtr &entry)
{
    std::lock_guard<std::mutex> guard(entryMutex_);
    /* look up the shm entry exists or not with lock */
    SM_ASSERT_RETURN(inited_, SM_NOT_STARTED);
    auto iter = ptr2EntryMap_.find(ptr);
    if (iter != ptr2EntryMap_.end()) {
        entry = iter->second;
        return SM_OK;
    }

    SM_LOG_DEBUG("Not found shm entry with ptr " << ptr);
    return SM_OBJECT_NOT_EXISTS;
}

Result SmemShmEntryManager::RemoveEntryByPtr(uintptr_t ptr)
{
    std::lock_guard<std::mutex> guard(entryMutex_);
    /* look up the shm entry exists or not with lock */
    SM_ASSERT_RETURN(inited_, SM_NOT_STARTED);
    auto iter = ptr2EntryMap_.find(ptr);
    if (iter == ptr2EntryMap_.end()) {
        SM_LOG_DEBUG("Not found shm entry with ptr " << ptr);
        return SM_OBJECT_NOT_EXISTS;
    }

    /* assign to a tmp ptr and remove from map */
    auto entry = iter->second;
    ptr2EntryMap_.erase(iter);

    /* remove from id set */
    SM_ASSERT_RETURN(entry != nullptr, SM_ERROR);
    entryIdSet_.erase(entry->Id());

    SM_LOG_DEBUG("Remove shm entry with ptr " << ptr << ", id " << entry->Id());

    return SM_OK;
}
}
}
