/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_SMEM_BM_ENTRY_MANAGER_H
#define MEMFABRIC_HYBRID_SMEM_BM_ENTRY_MANAGER_H

#include "smem_bm.h"
#include "smem_bm_entry.h"

namespace ock {
namespace smem {

class SmemBmEntryManager {
public:
    static SmemBmEntryManager &Instance();

    SmemBmEntryManager() = default;
    ~SmemBmEntryManager() = default;

    SmemBmEntryManager(const SmemBmEntryManager &) = delete;
    SmemBmEntryManager(SmemBmEntryManager &&) = delete;

public:
    std::mutex entryMutex_;
    std::map<uintptr_t, SmemBmEntryPtr> ptr2EntryMap_; /* lookup entry by ptr */
    std::map<uint32_t, SmemBmEntryPtr> entryIdMap_;    /* deduplicate entry by id */
    smem_bm_config_t config_{};
    uint16_t deviceId_ = 0;
    bool inited_ = false;
};

}  // namespace smem
}  // namespace ock

#endif  //MEMFABRIC_HYBRID_SMEM_BM_ENTRY_MANAGER_H