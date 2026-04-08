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
#ifndef MF_SMEM_TRANS_ENTRY_MANAGER_H
#define MF_SMEM_TRANS_ENTRY_MANAGER_H

#include "smem_common_includes.h"
#include "smem_trans_entry.h"
#include "smem_config_store.h"

namespace ock {
namespace smem {
constexpr uint32_t TRANS_ENTITY_ID_BASE = 256U;

class SmemTransEntryManager {
public:
    static SmemTransEntryManager &Instance();

public:
    SmemTransEntryManager() = default;
    ~SmemTransEntryManager() = default;

    Result Initialize(const std::string &storeUrl, int32_t maxRetry);
    void UnInitialize();
    Result CreateEntryByName(const std::string &name, const std::string &storeUrl, const smem_trans_config_t &config,
                             SmemTransEntryPtr &entry);
    Result GetEntryByPtr(uintptr_t ptr, SmemTransEntryPtr &entry);
    Result GetEntryByName(const std::string &name, SmemTransEntryPtr &entry);
    Result RemoveEntryByPtr(uintptr_t ptr);
    Result RemoveEntryByName(const std::string &name);

private:
    std::mutex entryMutex_;
    std::map<uintptr_t, SmemTransEntryPtr> ptr2EntryMap_;    /* lookup entry by ptr */
    std::map<std::string, SmemTransEntryPtr> name2EntryMap_; /* deduplicate entry by name */
    StorePtr confStore_ = nullptr;
    std::string storeUrl_;
    uint32_t rank_{UINT32_MAX};
    uint16_t deviceId_{0};
    uint32_t entryIdx_{TRANS_ENTITY_ID_BASE};
};
} // namespace smem
} // namespace ock

#endif // MF_SMEM_TRANS_ENTRY_MANAGER_H
