/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_LOCALITY_STRATEGY_H
#define MEM_FABRIC_MMC_LOCALITY_STRATEGY_H

#include <vector>
#include <set>
#include <random>
#include <algorithm>
#include <ostream>
#include "mmc_mem_blob.h"
#include "mmc_types.h"
#include "mmc_blob_allocator.h"
#include "mmc_msg_base.h"
#include "mmc_msg_packer.h"

namespace ock {
namespace mmc {

enum AllocFlags {
    ALLOC_ARRANGE = 0,
    ALLOC_FORCE_BY_RANK = 1 << 0, // 按照rank强制分配
    ALLOC_RANDOM = 1 << 1,
};

struct AllocOptions {
    uint64_t blobSize_{0};
    uint32_t numBlobs_{0};
    uint16_t mediaType_{0};
    std::vector<uint32_t> preferredRank_{};
    uint32_t flags_{0};
    AllocOptions() = default;
    AllocOptions(uint64_t blobSize, uint32_t numBlobs, uint16_t mediaType, const std::vector<uint32_t> &preferredRank,
                 uint32_t flags)
        : blobSize_(blobSize), numBlobs_(numBlobs), mediaType_(mediaType), preferredRank_(preferredRank), flags_(flags)
    {}

    Result Serialize(NetMsgPacker &packer) const
    {
        packer.Serialize(blobSize_);
        packer.Serialize(numBlobs_);
        packer.Serialize(mediaType_);
        packer.Serialize(preferredRank_);
        packer.Serialize(flags_);
        return MMC_OK;
    }

    Result Deserialize(NetMsgUnpacker &packer)
    {
        packer.Deserialize(blobSize_);
        packer.Deserialize(numBlobs_);
        packer.Deserialize(mediaType_);
        packer.Deserialize(preferredRank_);
        packer.Deserialize(flags_);
        return MMC_OK;
    }

    friend std::ostream &operator<<(std::ostream &os, const AllocOptions &obj)
    {
        os << "blobSize: " << obj.blobSize_ << ", numBlobs: " << obj.numBlobs_ << ", preferredRank: [";
        for (uint32_t rank : obj.preferredRank_) {
            os << rank << ", ";
        }
        return os << "], ";
    }
};

struct MmcLocalMemCurInfo {
    uint64_t capacity_;
};

using MmcMemPoolCurInfo = std::map<MmcLocation, MmcLocalMemCurInfo>;

using MmcAllocators = std::map<MmcLocation, MmcBlobAllocatorPtr>;

class MmcLocalityStrategy : public MmcReferable {
public:
    static Result ArrangeLocality(const MmcAllocators &allocators, const AllocOptions &allocReq,
                                  std::vector<MmcMemBlobPtr> &blobs, const std::unordered_set<uint32_t> &excludeRanks)
    {
        if (allocators.empty()) {
            MMC_LOG_ERROR("Cannot allocate blob, allocators empty");
            return MMC_ERROR;
        }
        if (allocators.size() < allocReq.numBlobs_) {
            MMC_LOG_ERROR("Cannot allocate blob, allocators not enough");
            return MMC_ERROR;
        }
        MmcLocation location{};
        location.mediaType_ = static_cast<MediaType>(allocReq.mediaType_);
        location.rank_ = allocReq.preferredRank_.empty() ? 0 : allocReq.preferredRank_[0];
        auto itPrefer = allocators.find(location);
        if (itPrefer == allocators.end()) {
            itPrefer = allocators.begin();
        }
        auto it = itPrefer;

        std::set<MmcLocation> visited;
        for (uint32_t i = 0; i < allocReq.numBlobs_; i++) {
            while (true) {
                if (!(visited.find(it->first) != visited.end()) && it->first.mediaType_ == allocReq.mediaType_ &&
                    excludeRanks.find(it->first.rank_) == excludeRanks.end()) {
                    auto allocator = it->second;
                    MmcMemBlobPtr blob = allocator->Alloc(allocReq.blobSize_);
                    if (blob != nullptr) {
                        blobs.push_back(blob);
                        visited.insert(it->first);  // 防止一个alloctor分配两个blob
                        break;
                    }
                }
                ++it;
                if (it == allocators.end()) {
                    it = allocators.begin();
                }
                if (it == itPrefer) {
                    MMC_LOG_ERROR("Cannot allocate blob, blobSize "<< allocReq.blobSize_
                        << " numBlobs " << allocReq.numBlobs_ << " mediaType " << allocReq.mediaType_
                        << " preferredRank " << (allocReq.preferredRank_.empty() ? 0 : allocReq.preferredRank_[0]));
                    return MMC_ERROR;
                }
            }
        }
        return MMC_OK;
    }

    static Result ForceAssign(const MmcAllocators &allocators, const AllocOptions &allocReq,
                                  std::vector<MmcMemBlobPtr> &blobs)
    {
        if (allocators.empty()) {
            MMC_LOG_ERROR("Cannot allocate blob, allocators empty");
            return MMC_ERROR;
        }
        if (allocReq.numBlobs_ != 1) {
            MMC_LOG_ERROR("force assign allocate blob numBlobs must be 1");
            return MMC_ERROR;
        }
        MmcLocation location{};
        location.mediaType_ = static_cast<MediaType>(allocReq.mediaType_);
        location.rank_ = allocReq.preferredRank_.empty() ? 0 : allocReq.preferredRank_[0];
        auto itPrefer = allocators.find(location);
        if (itPrefer == allocators.end()) {
            MMC_LOG_ERROR("Cannot force assign allocate blob, allocator rank: " << location.rank_
                << " media type: " << location.mediaType_ << " not found");
            return MMC_ERROR;
        }
        auto it = itPrefer;

        auto allocator = it->second;
        if (allocator == nullptr) {
            MMC_LOG_ERROR("Cannot force assign allocate blob, allocator rank: " << location.rank_
                << " media type: " << location.mediaType_ << " is nullptr");
            return MMC_ERROR;
        }
        MmcMemBlobPtr blob = allocator->Alloc(allocReq.blobSize_);
        if (blob != nullptr) {
            blobs.push_back(blob);
        } else {
                MMC_LOG_ERROR("Cannot force assign allocate blob, allocator rank: " << location.rank_
                << " media type: " << location.mediaType_ << " cannot alloc space");
            return MMC_ERROR;
        }

        return MMC_OK;
    }

    static Result RandomAssign(const MmcAllocators &allocators, const AllocOptions &allocReq,
                               std::vector<MmcMemBlobPtr> &blobs)
    {
        if (allocators.empty()) {
            MMC_LOG_ERROR("Cannot allocate blob, allocators empty");
            return MMC_ERROR;
        }
        MmcLocation lowerBound;
        MmcLocation upperBound;
        lowerBound.mediaType_ = static_cast<MediaType>(allocReq.mediaType_);
        lowerBound.rank_ = std::numeric_limits<uint32_t>::min();
        upperBound.mediaType_ = static_cast<MediaType>(allocReq.mediaType_);
        upperBound.rank_ = std::numeric_limits<uint32_t>::max();
        auto first = allocators.lower_bound(lowerBound);
        auto last = allocators.upper_bound(upperBound);
        const size_t numCandidates = static_cast<size_t>(std::distance(first, last));
        if (numCandidates == 0) {
            MMC_LOG_ERROR("No allocators found for media type: " << allocReq.mediaType_);
            return MMC_ERROR;
        }
        if (numCandidates < allocReq.numBlobs_) {
            MMC_LOG_ERROR("Not enough allocators for media type: " << allocReq.mediaType_
                          << ", required: " << allocReq.numBlobs_ << ", available: " << numCandidates);
            return MMC_ERROR;
        }
        std::vector<size_t> indices;
        indices.reserve(numCandidates);
        for (size_t i = 0; i < numCandidates; ++i) {
            indices.push_back(i);
        }
        std::random_device rd;
        std::mt19937 rng(rd());
        std::shuffle(indices.begin(), indices.end(), rng);
        size_t allocatedCount = 0;
        for (size_t i = 0; i < numCandidates; ++i) {
            auto it = first;
            std::advance(it, indices[i]);
            MmcMemBlobPtr blob = it->second->Alloc(allocReq.blobSize_);
            if (blob != nullptr) {
                blobs.push_back(blob);
                allocatedCount++;
                if (allocatedCount >= allocReq.numBlobs_) {
                    break;
                }
            }
        }
        if (allocatedCount < allocReq.numBlobs_) {
            MMC_LOG_ERROR("RandomAssign failed to allocate all blobs. Requested: " << allocReq.numBlobs_);
            return MMC_ERROR;
        }
        return MMC_OK;
    }
};

} // namespace mmc
} // namespace ock

#endif // MEM_FABRIC_MMC_LOCALITY_STRATEGY_H