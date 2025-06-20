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

static const uint16_t MAX_NUM_BLOB_CHAINS = 5;  // to make sure MmcMemObjMeta <= 64 bytes

class MmcMemObjMeta : public MmcReferable {
public:
    MmcMemObjMeta() = default;
    ~MmcMemObjMeta() override = default;

    /**
     * @brief Add blob info into the mem object,
     * for replicated blob on different rank ids
     *
     * @param blob         [in] blob pointer to be added
     */
    void AddBlob(const MmcMemBlobPtr &blob);

    /**
     * @brief Remove a blob from the mem object
     *
     * @param blob         [in] blob pointer to be removed
     * @return 0 if removed
     */
    Result RemoveBlob(const MmcMemBlobPtr &blob);

    /**
     * @brief Extend the lease
     * @param ttl          [in] time of live in ms
     */
    void ExtendLease(uint64_t ttl);

    /**
     * @brief Check if the lease already expired
     * @return true if expired
     */
    bool IsLeaseExpired();

    /**
     * @brief Get the number of blobs
     * @return number of blobs
     */
    uint16_t NumBlobs();

private:
    /* make sure the size of this class is 64 bytes */
    uint16_t prot_{0};                         /* prot of the mem object, i.e. accessibility */
    uint8_t priority_{0};                      /* priority of the memory object, used for eviction */
    uint8_t numBlobs_{0};                      /* number of blob that the memory object, i.e. replica count */
    MmcMemBlobPtr blobs_[MAX_NUM_BLOB_CHAINS]; /* pointers of blobs */
    uint64_t lease_{0};                        /* lease of the memory object */
    uint32_t size_{0};                         /* byteSize of each blob */
    Spinlock spinlock_;                        /* 4 bytes */
};

using MmcMemBlobPtr = MmcRef<MmcMemBlob>;

inline void MmcMemObjMeta::ExtendLease(const uint64_t ttl)
{
    using namespace std::chrono;
    const uint64_t nowMs = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();

    std::lock_guard<Spinlock> guard(spinlock_);
    lease_ = std::max(lease_, nowMs + ttl);
}

inline bool MmcMemObjMeta::IsLeaseExpired()
{
    using namespace std::chrono;
    const uint64_t nowMs = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();

    std::lock_guard<Spinlock> guard(spinlock_);
    return (lease_ < nowMs);
}

inline uint16_t MmcMemObjMeta::NumBlobs()
{
    std::lock_guard<Spinlock> guard(spinlock_);
    return numBlobs_;
}

}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_MEM_OBJ_META_H
