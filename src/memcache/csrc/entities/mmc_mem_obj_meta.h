/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_MEM_OBJ_META_H
#define MEM_FABRIC_MMC_MEM_OBJ_META_H

#include "mmc_mem_blob.h"
#include "mmc_ref.h"
#include "mmc_spinlock.h"

namespace ock {
namespace mmc {

static const uint16_t MAX_NUM_BLOB_CHAINS = 5; // to make sure MmcMemObjMeta <= 64 bytes

class MmcMemObjMeta : public MmcReferable {
public:
    MmcMemObjMeta() = default;
    void AddBlob(MmcMemBlobPtr blob);
    Result RemoveBlob(MmcMemBlobPtr blob);
    void ExtendLease(uint64_t ttl);
    bool IsLeaseExpired();
    uint16_t NumBlobs();

private:
    uint8_t numBlobs_{0};
    uint8_t priority_{0};
    uint16_t prot_{0};
    MmcMemBlobPtr blobs_[MAX_NUM_BLOB_CHAINS];
    uint64_t lease_{0};
    uint32_t size_{0}; // byteSize of each blob

    Spinlock spinlock_; // 4 bytes
};

using MmcMemBlobPtr = MmcRef<MmcMemBlob>;

inline void MmcMemObjMeta::ExtendLease(uint64_t ttl)
{
    std::lock_guard<Spinlock> guard(spinlock_);
    auto now = std::chrono::steady_clock::now();
    uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    lease_ = std::max(lease_, nowMs + ttl);
}

inline bool MmcMemObjMeta::IsLeaseExpired()
{
    std::lock_guard<Spinlock> guard(spinlock_);
    auto now = std::chrono::steady_clock::now();
    uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    bool ret = lease_ < nowMs;
    return ret;
}

inline uint16_t MmcMemObjMeta::NumBlobs()
{
    std::lock_guard<Spinlock> guard(spinlock_);
    auto ret = numBlobs_;
    return ret;
}

} // namespace mmc
} // namespace ock

#endif // MEM_FABRIC_MMC_MEM_OBJ_META_H
