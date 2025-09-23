/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hybm_data_operator.h"
namespace ock {
namespace mf {
int32_t DataOperator::BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                    const ExtOptions &options) noexcept
{
    for (auto i = 0U; i < params.batchSize; i++) {
        hybm_copy_params p{params.sources[i], params.destinations[i], params.dataSizes[i]};
        auto ret = DataCopy(p, direction, options);
        if (ret != 0) {
            return ret;
        }
    }
    return BM_OK;
}
}
}