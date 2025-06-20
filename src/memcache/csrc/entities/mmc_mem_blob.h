/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_MEM_BLOB_H
#define MEM_FABRIC_MMC_MEM_BLOB_H

#include "mmc_blob_state.h"
#include "mmc_ref.h"
#include "mmc_spinlock.h"

namespace ock {
namespace mmc {

class MmcMemBlob : public MmcReferable {
public:
    MmcMemBlob() = delete;
    inline MmcMemBlob(uint32_t rank, uint64_t gva, uint32_t size, uint16_t mediaType)
        : rank_(rank), gva_(gva), size_(size), mediaType_(mediaType), nextBlob_(nullptr) {};

    Result UpdateState(BlobState newState, BlobState oldState);
    Result SetNext(MmcRef<MmcMemBlob> nextBlob);

    int32_t Rank() { return rank_; }
    uint64_t Gva() { return gva_; }
    uint64_t Size() { return size_; }
    int16_t MediaType() { return mediaType_; }
    BlobState State()
    {
        std::lock_guard<Spinlock> guard(spinlock_);
        BlobState curState = state_;
        return curState;
    }
    MmcRef<MmcMemBlob> Next()
    {
        std::lock_guard<Spinlock> guard(spinlock_);
        return nextBlob_;
    }

private:
    const uint32_t rank_;
    const uint64_t gva_;
    const uint32_t size_;
    const uint16_t mediaType_;
    BlobState state_{BlobState::INIT};
    uint16_t prot_{0};

    MmcRef<MmcMemBlob> nextBlob_;

    Spinlock spinlock_;
};

using MmcMemBlobPtr = MmcRef<MmcMemBlob>;

inline Result MmcMemBlob::UpdateState(BlobState newState, BlobState oldState)
{
    std::lock_guard<Spinlock> guard(spinlock_);
    if (state_ == oldState) {
        state_ = newState;
        return MMC_OK;
    } else {
        return MMC_UNMATCHED_STATE;
    }
}

inline Result MmcMemBlob::SetNext(MmcRef<MmcMemBlob> nextBlob)
{
    std::lock_guard<Spinlock> guard(spinlock_);
    if (nextBlob_ == nullptr) {
        nextBlob_ = nextBlob;
        return MMC_OK;
    } else {
        return MMC_ERROR;
    }
}

} // namespace mmc
} // namespace ock

#endif // MEM_FABRIC_MMC_MEM_BLOB_H
