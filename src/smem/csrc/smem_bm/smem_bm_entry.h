/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
    uint32_t id;
    uint32_t rank;
    uint32_t rankSize;
    uint64_t controlOperationTimeout;
};

class SmemBmEntry : public SmReferable {
public:
    explicit SmemBmEntry(const SmemBmEntryOptions &options, const StorePtr &store)
        : options_(options),
          _configStore(store)
    {
    }

    ~SmemBmEntry() override;

    int32_t Initialize(const hybm_options &options);

    Result Join(uint32_t flags, void **localGvaAddress);

    Result Leave(uint32_t flags);

    Result DataCopy(const void *src, void *dest, uint64_t size, smem_bm_copy_type t, uint32_t flags);

    Result DataCopyBatch(const void **src, void **dest, const size_t *size,
                         uint32_t count, smem_bm_copy_type t, uint32_t flags);

    Result DataCopy2d(smem_copy_2d_params &params, smem_bm_copy_type t, uint32_t flags);

    uint32_t Id() const;

    const hybm_options &GetCoreOptions() const;

    void *GetGvaAddress() const;

private:
    bool AddressInRange(const void *address, uint64_t size);
    Result CreateGlobalTeam(uint32_t rankSize, uint32_t rankId);

    Result JoinHandle(uint32_t rk);
    Result LeaveHandle(uint32_t rk);

private:
    /* hot used variables */
    bool inited_ = false;
    std::mutex mutex_;
    SmemGroupEnginePtr globalGroup_ = nullptr;
    hybm_entity_t entity_ = nullptr;
    void *gva_ = nullptr;
    hybm_mem_slice_t slice_ = nullptr;

    /* non-hot used variables */
    SmemBmEntryOptions options_;
    hybm_options coreOptions_{};
    StorePtr _configStore;
    hybm_exchange_info exInfo_{};
    hybm_exchange_info entityInfo_{};
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

#endif  // MEMFABRIC_HYBRID_SMEM_BM_ENTRY_H
