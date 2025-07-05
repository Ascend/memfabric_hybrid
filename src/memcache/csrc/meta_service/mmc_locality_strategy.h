/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_LOCALITY_STRATEGY_H
#define MEM_FABRIC_MMC_LOCALITY_STRATEGY_H

#include "mmc_mem_blob.h"
#include "mmc_types.h"
#include "mmc_blob_allocator.h"
#include "mmc_msg_base.h"
#include "mmc_msg_packer.h"
#include <vector>

namespace ock {
namespace mmc {

struct AllocOptions {
    uint64_t blobSize_{0};
    uint32_t numBlobs_{0};
    uint16_t mediaType_{0};
    uint32_t preferredRank_{0};
    uint32_t flags_{0};
    AllocOptions(){}
    AllocOptions(uint64_t blobSize, uint32_t numBlobs, uint16_t mediaType, uint32_t preferredRank, uint32_t flags) :
                  blobSize_(blobSize), numBlobs_(numBlobs), mediaType_(mediaType), preferredRank_(preferredRank),
                  flags_(flags){}
};

struct MmcLocalMemCurInfo {
    uint64_t capacity_;
};

using MmcMemPoolCurInfo = std::map<MmcLocation, MmcLocalMemCurInfo>;

using MmcAllocators = std::map<MmcLocation, MmcBlobAllocatorPtr>;

class MmcLocalityStrategy : public MmcReferable {
public:
    static Result ArrangeLocality(const MmcAllocators &allocators, const AllocOptions &allocReq,
                                  std::vector<MmcLocation> &locations)
    {
        // todo 改为迭代器遍历
        MmcLocation location;
        location.mediaType_ = allocReq.mediaType_;
        location.rank_ = allocReq.preferredRank_;
        bool isEnd = false;
        for (uint32_t i = 0; i < allocReq.numBlobs_; i++) {
            while (true) {
                auto it = allocators.find(location);
                if (it != allocators.end()) {
                    auto allocator = it->second;
                    auto result = allocator->PreAlloc(allocReq.blobSize_);
                    if (result == MMC_OK) {
                        locations.push_back(location);
                        break;
                    } else if (result == MMC_NOT_ENOUGH_MEMORY) {
                        location.rank_++;
                    } else {
                        return MMC_ERROR;
                    }
                } else {
                    if (isEnd == false) {
                        isEnd = true;
                        location.rank_ = 0;
                    } else {
                        return MMC_ERROR;
                    }
                }
                if (location.rank_ == allocReq.preferredRank_) {
                    return MMC_ERROR;
                }
            }

        }
        return MMC_OK;
    };
};

} // namespace mmc
} // namespace ock

#endif // MEM_FABRIC_MMC_LOCALITY_STRATEGY_H