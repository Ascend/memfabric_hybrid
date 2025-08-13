/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_MEM_BLOB_H
#define MEM_FABRIC_MMC_MEM_BLOB_H

#include "mmc_blob_state.h"
#include "mmc_common_includes.h"
#include "mmc_meta_lease_manager.h"
#include "mmc_montotonic.h"
#include "mmc_def.h"

namespace ock {
namespace mmc {
struct MemObjQueryInfo {
    uint64_t size_;
    uint16_t prot_;
    uint8_t numBlobs_;
    bool valid_;
    uint32_t blobRanks_[MAX_BLOB_COPIES];
    uint16_t blobTypes_[MAX_BLOB_COPIES];
    MemObjQueryInfo() : size_(0), prot_(0), numBlobs_(0), valid_(false) {}
    MemObjQueryInfo(const uint64_t size, const uint16_t prot, const uint8_t numBlobs, const bool valid)
        : size_(size), prot_(prot), numBlobs_(numBlobs), valid_(valid)
    {
    }
};

class MmcMemBlob;
using MmcMemBlobPtr = MmcRef<MmcMemBlob>;

struct MmcBlobFilter : public MmcReferable {
    uint32_t rank_{UINT32_MAX};
    MediaType mediaType_{MEDIA_NONE};
    BlobState state_{NONE};
    MmcBlobFilter(const uint32_t &rank, const MediaType &mediaType, const BlobState &state)
        : rank_(rank), mediaType_(mediaType), state_(state) {};
};
using MmcBlobFilterPtr = MmcRef<MmcBlobFilter>;

struct MmcMemBlobDesc {
    uint32_t rank_ = UINT32_MAX;        /* rank id of the blob located */
    uint64_t size_ = 0;                 /* data size of the blob */
    uint64_t gva_ = UINT64_MAX;         /* global virtual address */
    uint16_t mediaType_ = UINT16_MAX;   /* media type where blob located */
    uint16_t prot_ = 0;                 /* prot, i.e. access */
    BlobState state_ = BlobState::NONE; /* state of the blob */

    MmcMemBlobDesc() = default;
    MmcMemBlobDesc(const uint32_t &rank, const uint64_t &gva, const uint64_t &size, const uint16_t &mediaType,
                   const BlobState &state, const uint16_t &prot)
        : rank_(rank), size_(size), gva_(gva), mediaType_(mediaType), prot_(prot), state_(state)
    {
    }
};

class MmcMemBlob final : public MmcReferable {
public:
    MmcMemBlob() = delete;
    MmcMemBlob(const uint32_t &rank, const uint64_t &gva, const uint32_t &size, const MediaType &mediaType,
               const BlobState &state)
        : rank_(rank), gva_(gva), size_(size), mediaType_(mediaType), state_(state), nextBlob_(nullptr)
    {
    }
    ~MmcMemBlob() override = default;

    /**
     * @brief Update the state of the blob
     *
     * @param ret     [in] BlobActionResult
     */
    Result UpdateState(uint32_t rankId, uint32_t operateId, BlobActionResult ret);

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
    uint16_t Type() const;

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

    inline Result ExtendLease(const uint32_t id, const uint32_t requestId, uint64_t ttl);

    inline bool IsLeaseExpired();

    friend std::ostream &operator<<(std::ostream &os, MmcMemBlob &blob)
    {
        os << "MmcMemBlob{rank=" << blob.rank_ << ",gva=" << blob.gva_ << ",size=" << blob.size_
           << ",mediaType=" << static_cast<int>(blob.mediaType_) << ",state=" << static_cast<int>(blob.state_)
           << ",prot=" << blob.prot_ << ",metaLeaseManager=" << blob.metaLeaseManager_ << "}";
        return os;
    }
    /**
     * 原则：blob由 MmcMemObjMeta 维护生命周期及锁的保护
     */
private:
    const uint32_t rank_;              /* rank id of the blob located */
    const uint64_t gva_;               /* global virtual address */
    const uint32_t size_;              /* data size of the blob */
    const enum MediaType mediaType_;   /* media type where blob located */
    BlobState state_{BlobState::NONE}; /* state of the blob */
    uint16_t prot_{0};                 /* prot, i.e. access */
    MmcMetaLeaseManager metaLeaseManager_;
    MmcMemBlobPtr nextBlob_;
    static const StateTransTable stateTransTable_;
};

inline Result MmcMemBlob::UpdateState(uint32_t rankId, uint32_t operateId, BlobActionResult ret)
{
    auto curStateIter = stateTransTable_.find(state_);
    if (curStateIter == stateTransTable_.end()) {
        MMC_LOG_ERROR("Cannot update state! The current state is not in the stateTransTable!");
        return MMC_UNMATCHED_STATE;
    }

    const auto retIter = curStateIter->second.find(ret);
    if (retIter == curStateIter->second.end()) {
        MMC_LOG_ERROR("Cannot update state! Current state is " << std::to_string(state_) << ", ret("
                                                               << std::to_string(ret)
                                                               << ") dismatch! RetCode: " << MMC_UNMATCHED_RET);
        return MMC_UNMATCHED_RET;
    }

    MMC_LOG_INFO("update blob state from " << std::to_string(state_) << " to ("
        << std::to_string(retIter->second.state_) << ")");

    state_ = retIter->second.state_;
    if (retIter->second.action_) {
        auto res = retIter->second.action_(metaLeaseManager_, rankId, operateId);
        if (res != MMC_OK) {
            MMC_LOG_ERROR("Blob update current state is " << std::to_string(state_) << " by ret(" << std::to_string(ret)
                                                          << ") failed! res=" << res);
            return res;
        }
    }
    return MMC_OK;
}

inline Result MmcMemBlob::Next(const MmcMemBlobPtr &nextBlob)
{
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

inline uint16_t MmcMemBlob::Type() const
{
    return mediaType_;
}

inline BlobState MmcMemBlob::State()
{
    return state_;
}

inline MmcMemBlobPtr MmcMemBlob::Next()
{
    return nextBlob_;
}

inline uint16_t MmcMemBlob::Prot()
{
    return prot_;
}

inline bool MmcMemBlob::MatchFilter(const MmcBlobFilterPtr &filter) const
{
    if (filter == nullptr) {
        return true;
    }

    return (filter->rank_ == UINT32_MAX || rank_ == filter->rank_) &&
           (filter->mediaType_ == MEDIA_NONE || mediaType_ == filter->mediaType_) &&
           (filter->state_ == NONE || state_ == filter->state_);
}

inline MmcMemBlobDesc MmcMemBlob::GetDesc() const
{
    return MmcMemBlobDesc{rank_, gva_, size_, mediaType_, state_, prot_};
}

Result MmcMemBlob::ExtendLease(const uint32_t id, const uint32_t requestId, uint64_t ttl)
{
    metaLeaseManager_.Add(id, requestId, ttl);
    return MMC_OK;
}
bool MmcMemBlob::IsLeaseExpired()
{
    if (metaLeaseManager_.UseCount() == 0) {
        return true;
    }
    metaLeaseManager_.Wait();
    return true;
}
}  // namespace mmc
}  // namespace ock

#endif  // MEM_FABRIC_MMC_MEM_BLOB_H
