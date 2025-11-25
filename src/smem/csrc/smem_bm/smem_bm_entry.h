/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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
          _configStore(store),
          coreOptions_{},
          dramSliceInfo_{},
          hbmSliceInfo_{},
          entityInfo_{}
    {
    }

    ~SmemBmEntry() override
    {
        UnInitalize();
    };

    int32_t Initialize(const hybm_options &options);

    void UnInitalize();

    Result Join(uint32_t flags);

    Result Leave(uint32_t flags);

    Result DataCopy(const void *src, void *dest, uint64_t size, smem_bm_copy_type t, uint32_t flags);

    Result DataCopyBatch(smem_batch_copy_params *params, smem_bm_copy_type t, uint32_t flags);

    Result Wait();

    Result RegisterMem(uint64_t addr, uint64_t size);

    uint32_t Id() const;

    uint32_t GetRankIdByGva(void *gva);

    const hybm_options &GetCoreOptions() const;

    void *GetGvaAddress() const;
    void *GetHostGvaAddress() const;
    void *GetDeviceGvaAddress() const;

private:
    bool AddrInHostGva(const void *address, uint64_t size);
    bool AddrInDeviceGva(const void *address, uint64_t size);

    smem_bm_mem_type GetHybmMemTypeFromGva(const void *addr, uint64_t size);
    hybm_data_copy_direction TransToHybmDirection(const smem_bm_copy_type &smemDirect, const void *src,
                                                  uint64_t srcSize, const void *dest, uint64_t destSize);
    bool AddressInRange(const void *address, uint64_t size);
    Result CreateGlobalTeam(uint32_t rankSize, uint32_t rankId);

    Result JoinHandle(uint32_t rk);
    Result LeaveHandle(uint32_t rk);
    Result ExchangeSliceForJoin(const hybm_exchange_info &sliceInfo);
    Result ExchangeEntityForJoin();

private:
    /* hot used variables */
    bool inited_ = false;
    std::mutex mutex_;
    SmemGroupEnginePtr globalGroup_ = nullptr;
    hybm_entity_t entity_ = nullptr;
    void *hostGva_ = nullptr;
    void *deviceGva_ = nullptr;

    /* non-hot used variables */
    SmemBmEntryOptions options_;
    hybm_options coreOptions_;
    StorePtr _configStore;
    hybm_exchange_info hbmSliceInfo_;
    hybm_exchange_info dramSliceInfo_;
    hybm_exchange_info entityInfo_;
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
    return deviceGva_;
}

inline void *SmemBmEntry::GetHostGvaAddress() const
{
    return hostGva_;
}

inline void *SmemBmEntry::GetDeviceGvaAddress() const
{
    return deviceGva_;
}

}  // namespace smem
}  // namespace ock

#endif  // MEMFABRIC_HYBRID_SMEM_BM_ENTRY_H
