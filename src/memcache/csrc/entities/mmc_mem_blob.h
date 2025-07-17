/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_MEM_BLOB_H
#define MEM_FABRIC_MMC_MEM_BLOB_H

#include "mmc_blob_state.h"
#include "mmc_common_includes.h"

namespace ock {
namespace mmc {
struct MemObjQueryInfo {
    uint32_t size_;
    uint16_t prot_;
    uint8_t numBlobs_;
    bool valid_;
    MemObjQueryInfo() : size_(0), prot_(0), numBlobs_(0), valid_(false) {}
    MemObjQueryInfo(const uint32_t size, const uint16_t prot, const uint8_t numBlobs, const bool valid)
        : size_(size), prot_(prot), numBlobs_(numBlobs), valid_(valid) {}
};

class MmcMemBlob;
using MmcMemBlobPtr = MmcRef<MmcMemBlob>;

struct MmcBlobFilter : public MmcReferable {
    uint32_t rank_{UINT32_MAX};
    uint16_t mediaType_{UINT16_MAX};
    BlobState state_{NONE};
    MmcBlobFilter(const uint32_t &rank, const uint16_t &mediaType, const BlobState &state)
        : rank_(rank),
          mediaType_(mediaType),
          state_(state){};
};
using MmcBlobFilterPtr = MmcRef<MmcBlobFilter>;

struct MmcMemBlobDesc {
    uint32_t rank_ = UINT32_MAX;        /* rank id of the blob located */
    uint32_t size_ = 0;                 /* data size of the blob */
    uint64_t gva_ = UINT64_MAX;         /* global virtual address */
    uint16_t mediaType_ = UINT16_MAX;   /* media type where blob located */
    uint16_t prot_ = 0;                 /* prot, i.e. access */
    BlobState state_ = BlobState::NONE; /* state of the blob */

    MmcMemBlobDesc() = default;
    MmcMemBlobDesc(const uint32_t &rank, const uint64_t &gva, const uint32_t &size, const uint16_t &mediaType,
                   const BlobState &state, const uint16_t &prot)
        : rank_(rank),
          size_(size),
          gva_(gva),
          mediaType_(mediaType),
          prot_(prot),
          state_(state)
    {
    }
};

class MmcMemBlob final : public MmcReferable {
public:
    MmcMemBlob() = delete;
    MmcMemBlob(const uint32_t &rank, const uint64_t &gva, const uint32_t &size, const uint16_t &mediaType,
               const BlobState &state = NONE)
        : rank_(rank),
          gva_(gva),
          size_(size),
          mediaType_(mediaType),
          state_(state),
          nextBlob_(nullptr)
    {
    }
    ~MmcMemBlob() override = default;

    /**
     * @brief Update the state of the blob
     *
     * @param ret     [in] BlobActionResult
     */
    Result UpdateState(BlobActionResult ret);

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

    /**
     * @brief Get the state of the prot
     * @return state
     */
    uint16_t Prot();

    bool MatchFilter(const MmcBlobFilterPtr &filter) const;

    MmcMemBlobDesc GetDesc() const;

private:
    const uint32_t rank_;              /* rank id of the blob located */
    const uint64_t gva_;               /* global virtual address */
    const uint32_t size_;              /* data size of the blob */
    const uint16_t mediaType_;         /* media type where blob located */
    BlobState state_{BlobState::NONE}; /* state of the blob */
    uint16_t prot_{0};                 /* prot, i.e. access */

    MmcMemBlobPtr nextBlob_;

    Spinlock spinlock_;

    static const StateTransTable stateTransTable_;
};

inline Result MmcMemBlob::UpdateState(BlobActionResult ret)
{
    std::lock_guard<Spinlock> guard(spinlock_);
    auto curStateIter = stateTransTable_.find(state_);
    if (curStateIter == stateTransTable_.end()) {
        MMC_LOG_WARN("Cannot update state! The current state is not in the stateTransTable!");
        return MMC_UNMATCHED_STATE;
    }

    const auto retIter = curStateIter->second.find(ret);
    if (retIter == curStateIter->second.end()) {
        MMC_LOG_WARN("Cannot update state! Retcode dismatch!");
        return MMC_UNMATCHED_RET;
    }

    state_ = retIter->second;
    return MMC_OK;
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

inline uint16_t MmcMemBlob::Prot()
{
    std::lock_guard<Spinlock> guard(spinlock_);
    return prot_;
}

inline bool MmcMemBlob::MatchFilter(const MmcBlobFilterPtr &filter) const
{
    if (filter == nullptr) {
        return true;
    }

    return (rank_ == filter->rank_) && (mediaType_ == filter->mediaType_) &&
           (filter->state_ == NONE || state_ == filter->state_);
}

inline MmcMemBlobDesc MmcMemBlob::GetDesc() const
{
    return MmcMemBlobDesc{rank_, gva_, size_, mediaType_, state_, prot_};
}

}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_MEM_BLOB_H
