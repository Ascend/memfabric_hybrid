/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_SMEM_TRANS_ENTRY_MANAGER_H
#define MF_SMEM_TRANS_ENTRY_MANAGER_H

#include "smem_common_includes.h"
#include "smem_trans_entry.h"

namespace ock {
namespace smem {
class SmemTransEntryManager {
public:
    static SmemTransEntryManager &Instance();

public:
    SmemTransEntryManager() = default;
    ~SmemTransEntryManager() = default;

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
};
}
}

#endif  // MF_SMEM_TRANS_ENTRY_MANAGER_H
