/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_SMEM_BM_ENTRY_MANAGER_H
#define MEMFABRIC_HYBRID_SMEM_BM_ENTRY_MANAGER_H

#include <string>
#include "smem_net_common.h"
#include "smem_bm.h"
#include "smem_bm_entry.h"
#include "smem_config_store.h"

namespace ock {
namespace smem {

class SmemBmEntryManager {
public:
    static SmemBmEntryManager &Instance();

    SmemBmEntryManager() = default;
    ~SmemBmEntryManager() = default;

    SmemBmEntryManager(const SmemBmEntryManager &) = delete;
    SmemBmEntryManager(SmemBmEntryManager &&) = delete;

    Result Initialize(const std::string &storeURL, uint32_t worldSize, uint16_t deviceId,
                      const smem_bm_config_t &config);

    Result CreateEntryById(uint32_t id, SmemBmEntryPtr &entry);
    Result GetEntryByPtr(uintptr_t ptr, SmemBmEntryPtr &entry);
    Result GetEntryById(uint32_t id, SmemBmEntryPtr &entry);
    Result RemoveEntryByPtr(uintptr_t ptr);

    inline uint32_t GetRankId() const
    {
        return config_.rankId;
    }

    inline uint32_t GetWorldSize() const
    {
        return worldSize_;
    }

    inline uint16_t GetDeviceId() const
    {
        return deviceId_;
    }

private:
    int32_t PrepareStore();
    int32_t AutoRanking();

private:
    std::mutex entryMutex_;
    std::map<uintptr_t, SmemBmEntryPtr> ptr2EntryMap_; /* lookup entry by ptr */
    std::map<uint32_t, SmemBmEntryPtr> entryIdMap_;    /* deduplicate entry by id */
    smem_bm_config_t config_{};
    std::string storeURL_;
    uint32_t worldSize_{0};
    uint16_t deviceId_{0};
    bool inited_ = false;
    UrlExtraction storeUrlExtraction_;

    StorePtr confStore_ = nullptr;
};

}  // namespace smem
}  // namespace ock

#endif  //MEMFABRIC_HYBRID_SMEM_BM_ENTRY_MANAGER_H