/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef SMEM_SMEM_SHM_ENTRY_MANAGER_H
#define SMEM_SMEM_SHM_ENTRY_MANAGER_H

#include "smem_common_includes.h"
#include "smem_shm_entry.h"
#include "smem_config_store.h"

namespace ock {
namespace smem {
class SmemShmEntryManager {
public:
    static SmemShmEntryManager &Instance();

public:
    SmemShmEntryManager() = default;
    ~SmemShmEntryManager() = default;

    SmemShmEntryManager(const SmemShmEntryManager &) = delete;
    SmemShmEntryManager(SmemShmEntryManager &&) = delete;
    SmemShmEntryManager &operator=(const SmemShmEntryManager &) = delete;
    SmemShmEntryManager &operator=(SmemShmEntryManager &&) = delete;

    Result Initialize(const char *configStoreIpPort, uint32_t worldSize, uint32_t rankId, uint16_t deviceId,
                      smem_shm_config_t *config);
    Result CreateEntryById(uint32_t id, SmemShmEntryPtr &entry);
    Result GetEntryByPtr(uintptr_t ptr, SmemShmEntryPtr &entry);
    Result GetEntryById(uint32_t id, SmemShmEntryPtr &entry);
    Result RemoveEntryByPtr(uintptr_t ptr);

    uint16_t GetDeviceId() const;

    StorePtr GetStoreClient() const;

    void Destroy();

private:
    std::mutex entryMutex_;
    std::map<uintptr_t, SmemShmEntryPtr> ptr2EntryMap_; /* lookup entry by ptr */
    std::map<uint32_t, SmemShmEntryPtr> entryIdMap_;    /* deduplicate entry by id */
    smem_shm_config_t config_{};
    uint16_t deviceId_ = 0;
    bool inited_ = false;
    std::string ip_;
    uint16_t port_ = 9980L;

    StorePtr store_ = nullptr;
};

inline uint16_t SmemShmEntryManager::GetDeviceId() const
{
    return deviceId_;
}

inline StorePtr SmemShmEntryManager::GetStoreClient() const
{
    return store_;
}

}  // namespace smem
}  // namespace ock

#endif  // SMEM_SMEM_SHM_ENTRY_MANAGER_H
