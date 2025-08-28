/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
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