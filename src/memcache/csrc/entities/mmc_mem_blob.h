/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_MEM_BLOB_H
#define MEM_FABRIC_MMC_MEM_BLOB_H

#include "mmc_common_includes.h"
#include "mmc_blob_state.h"

namespace ock {
namespace mmc {
class MmcMemBlob;
using MmcMemBlobPtr = MmcRef<MmcMemBlob>;

class MmcMemBlob final : public MmcReferable {
public:
    MmcMemBlob() = delete;
    MmcMemBlob(const uint32_t rank, const uint64_t gva, const uint32_t size, const uint16_t mediaType)
        : rank_(rank),
          gva_(gva),
          size_(size),
          mediaType_(mediaType),
          nextBlob_(nullptr)
    {
    }
    ~MmcMemBlob() override = default;

    /**
     * @brief Update the state of the blob
     *
     * @param newState     [in] new state to be set
     * @param oldState     [in] old state
     * @return 0 if new state not equal to old state, MMC_UNMATCHED_STATE if newState and old state are equal
     */
    Result UpdateState(BlobState newState, BlobState oldState);

    /**
     * @brief Link a blob to this blob
     *
     * @param nextBlob     [in] blob to be linked
     * @return 0 if next is nullptr
     */
    Result Next(const MmcMemBlobPtr &nextBlob);

    /**
     * @brief Get the linked blob
     * @return linked blob
     */
    MmcMemBlobPtr Next();

    /**
     * @brief Get the rank id of the blob, i.e. locality
     * @return rank id
     */
    uint32_t Rank() const;

    /**
     * @brief Get the global virtual address
     * @return gva
     */
    uint64_t Gva() const;

    /**
     * @brief Get the size of the blob
     * @return blob size
     */
    uint64_t Size() const;

    /**
     * @brief Get the media type of blob located, dram or xx
     * @return media type
     */
    uint16_t MediaType() const;

    /**
     * @brief Get the state of the blob
     * @return state
     */
    BlobState State();

private:
    const uint32_t rank_;              /* rank id of the blob located */
    const uint64_t gva_;               /* global virtual address */
    const uint32_t size_;              /* data size of the blob */
    const uint16_t mediaType_;         /* media type where blob located */
    BlobState state_{BlobState::INIT}; /* state of the blob */
    uint16_t prot_{0};                 /* prot, i.e. access */

    MmcMemBlobPtr nextBlob_;

    Spinlock spinlock_;
};

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

inline Result MmcMemBlob::Next(const MmcMemBlobPtr &nextBlob)
{
    std::lock_guard<Spinlock> guard(spinlock_);
    if (nextBlob_ == nullptr) {
        nextBlob_ = nextBlob;
        return MMC_OK;
    } else {
        return MMC_ERROR;
    }
}

inline uint32_t MmcMemBlob::Rank() const
{
    return rank_;
}

inline uint64_t MmcMemBlob::Gva() const
{
    return gva_;
}

inline uint64_t MmcMemBlob::Size() const
{
    return size_;
}

inline uint16_t MmcMemBlob::MediaType() const
{
    return mediaType_;
}

inline BlobState MmcMemBlob::State()
{
    std::lock_guard<Spinlock> guard(spinlock_);
    return state_;
}

inline MmcMemBlobPtr MmcMemBlob::Next()
{
    std::lock_guard<Spinlock> guard(spinlock_);
    return nextBlob_;
}

}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_MEM_BLOB_H
