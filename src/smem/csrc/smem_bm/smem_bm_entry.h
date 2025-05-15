/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_SMEM_BM_ENTRY_H
#define MEMFABRIC_HYBRID_SMEM_BM_ENTRY_H

#include "hybm_def.h"
#include "smem_common_includes.h"
#include "smem_config_store.h"
#include "smem_net_group_engine.h"
#include "smem_bm.h"

namespace ock {
namespace smem {
struct SmemBmEntryOptions {
    uint32_t id = 0;
};

class SmemBmEntry : public SmReferable {
public:
    explicit SmemBmEntry(const SmemBmEntryOptions &options, const StorePtr &store)
        : options_(options),
          _configStore(store)
    {
    }

    ~SmemBmEntry() override = default;

    void SetConfig(const smem_bm_config_t &config);

    int32_t Initialize(const hybm_options &options);

    Result Join(uint32_t flags, void **localGvaAddress);

    Result Leave(uint32_t flags);

    Result DateCopy(const void *src, void *dest, uint64_t size, smem_bm_copy_type t, uint32_t flags);

    uint32_t Id() const;

    const hybm_options &GetCoreOptions() const;

    void *GetGvaAddress() const;

private:
    bool AddressInRange(const void *address, uint64_t size);
    Result CreateGlobalTeam(uint32_t rankSize, uint32_t rankId);

private:
    /* hot used variables */
    bool inited_ = false;
    std::mutex mutex_;
    SmemGroupEnginePtr globalGroup_ = nullptr;
    uint32_t localRank_ = UINT32_MAX;
    hybm_entity_t entity_ = nullptr;
    void *gva_ = nullptr;

    /* non-hot used variables */
    SmemBmEntryOptions options_;
    hybm_options coreOptions_;
    smem_bm_config_t extraConfig_;
    StorePtr _configStore;
};
using SmemBmEntryPtr = SmRef<SmemBmEntry>;

inline uint32_t SmemBmEntry::Id() const
{
    return options_.id;
}

inline const hybm_options &SmemBmEntry::GetCoreOptions() const
{
    return coreOptions_;
}

inline void *SmemBmEntry::GetGvaAddress() const
{
    return gva_;
}

}  // namespace smem
}  // namespace ock

#endif  //MEMFABRIC_HYBRID_SMEM_BM_ENTRY_H
