/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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
    virtual int32_t BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                  const ExtOptions &options) noexcept;
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
