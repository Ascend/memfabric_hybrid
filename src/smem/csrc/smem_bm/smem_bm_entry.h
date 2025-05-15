/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_SMEM_BM_ENTRY_H
#define MEMFABRIC_HYBRID_SMEM_BM_ENTRY_H

#include "smem_bm.h"
#include "smem_common_includes.h"

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
    explicit SmemBmEntry(const SmemBmEntryOptions& options) : options_(options)
    {
    }

    ~SmemBmEntry() override = default;

    Result Join(uint32_t flags, void **localGvaAddress);

    Result Leave(uint32_t flags);

    Result DateCopy(void *src, void *dest, uint64_t size, smem_bm_copy_type t, uint32_t flags);

    uint32_t Id() const;

private:
    /* hot used variables */
    bool inited_ = false;
    std::mutex mutex_;

    /* non-hot used variables */
    SmemBmEntryOptions options_;
};
using SmemBmEntryPtr = SmRef<SmemBmEntry>;

inline uint32_t SmemBmEntry::Id() const
{
    return options_.id;
}

}  // namespace smem
}  // namespace ock

#endif  //MEMFABRIC_HYBRID_SMEM_BM_ENTRY_H
