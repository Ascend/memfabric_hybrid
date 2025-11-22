/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_DATA_ACTION_H
#define MEM_FABRIC_HYBRID_HYBM_DATA_ACTION_H

#include "hybm_common_include.h"
#include "hybm_big_mem.h"

namespace ock {
namespace mf {

struct ExtOptions {
    uint32_t srcRankId;
    uint32_t destRankId;
    void *stream;
    uint32_t flags;
};

typedef struct {
    std::vector<void *> localAddrs;
    std::vector<void *> globalAddrs;
    std::vector<uint64_t> counts;
} CopyDescriptor;

class DataOperator {
public:
    virtual int32_t Initialize() noexcept = 0;
    virtual void UnInitialize() noexcept = 0;

    virtual int32_t DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                             const ExtOptions &options) noexcept = 0;
    virtual int32_t DataCopy2d(hybm_copy_2d_params &params, hybm_data_copy_direction direction,
                               const ExtOptions &options) noexcept = 0;
    virtual int32_t BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                  const ExtOptions &options) noexcept = 0;
    /*
     * 异步data copy
     * @return 0 if successful, > 0 is wait id, < 0 is error
     */
    virtual int32_t DataCopyAsync(hybm_copy_params &params, hybm_data_copy_direction direction,
                                  const ExtOptions &options) noexcept = 0;

    virtual int32_t Wait(int32_t waitId) noexcept = 0;

    virtual ~DataOperator() = default;

public:
    void UpdateGvaSpace(hybm_mem_type type, uint64_t gva, uint64_t localSpaceSize, uint64_t rankCount) noexcept
    {
        if (type == HYBM_MEM_TYPE_BUTT || localSpaceSize == 0) {
            BM_LOG_ERROR("type " << type << " error, local space " << localSpaceSize);
            return;
        }
        gva_[type] = gva;
        localSpaceSize_[type] = localSpaceSize;
        rankCount_[type] = rankCount;
        BM_LOG_INFO("update type " << type << " gva: " << gva_[type] << ", space:" << localSpaceSize_[type]
                                   << ", rankCnt:" << rankCount_[type]);
    };

    uint32_t GetRankIdByGva(uint64_t gva) noexcept
    {
        for (auto type = 0; type < HYBM_MEM_TYPE_BUTT; ++type) {
            if (gva >= gva_[type] && gva < (gva_[type] + rankCount_[type] * localSpaceSize_[type])) {
                return (gva - gva_[type]) / localSpaceSize_[type];
            }
        }
        return UINT32_MAX;
    }

protected:
    uint64_t gva_[HYBM_MEM_TYPE_BUTT] = {0};
    uint64_t localSpaceSize_[HYBM_MEM_TYPE_BUTT] = {0};
    uint64_t rankCount_[HYBM_MEM_TYPE_BUTT] = {0};
};
}
}

#endif // MEM_FABRIC_HYBRID_HYBM_DATA_ACTION_H