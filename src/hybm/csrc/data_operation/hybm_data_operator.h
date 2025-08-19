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

class DataOperator {
public:
    virtual int32_t Initialize() noexcept = 0;
    virtual void UnInitialize() noexcept = 0;

    virtual int32_t DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                             const ExtOptions &options) noexcept = 0;
    virtual int32_t DataCopy2d(hybm_copy_2d_params &params, hybm_data_copy_direction direction,
                               const ExtOptions &options) noexcept = 0;

    /*
     * 异步data copy
     * @return 0 if successful, > 0 is wait id, < 0 is error
     */
    virtual int32_t DataCopyAsync(hybm_copy_params &params, hybm_data_copy_direction direction,
                                  const ExtOptions &options) noexcept = 0;

    virtual int32_t Wait(int32_t waitId) noexcept = 0;

    virtual ~DataOperator() = default;
};
}
}

#endif // MEM_FABRIC_HYBRID_HYBM_DATA_ACTION_H
