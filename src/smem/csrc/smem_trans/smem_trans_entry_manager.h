/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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
