/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MF_HYBRID_SMEM_HYBM_HELPER_H
#define MF_HYBRID_SMEM_HYBM_HELPER_H

#include <cstdint>

#include "hybm_def.h"
#include "smem_bm_def.h"

namespace ock {
namespace smem {
class SmemHybmHelper {
public:
    static inline hybm_mem_type TransHybmMemType(uint64_t localDRAMSize, uint64_t localHBMSize)
    {
        uint32_t resultMemType = 0;
        if (localDRAMSize > 0) {
            resultMemType |= HYBM_MEM_TYPE_HOST;
        }
        if (localHBMSize > 0) {
            resultMemType |= HYBM_MEM_TYPE_DEVICE;
        }

        return static_cast<hybm_mem_type>(resultMemType);
    }

    static inline hybm_data_op_type TransHybmDataOpType(smem_bm_data_op_type smemBmDataOpType)
    {
        uint32_t resultOpType = 0;
        if (smemBmDataOpType & SMEMB_DATA_OP_SDMA) {
            resultOpType |= (HYBM_DOP_TYPE_MTE | HYBM_DOP_TYPE_SDMA);
        }

        if (smemBmDataOpType & SMEMB_DATA_OP_DEVICE_RDMA) {
            resultOpType |= HYBM_DOP_TYPE_DEVICE_RDMA;
        }

        if (smemBmDataOpType & SMEMB_DATA_OP_HOST_RDMA) {
            resultOpType |= HYBM_DOP_TYPE_HOST_RDMA;
        }

        if (smemBmDataOpType & SMEMB_DATA_OP_HOST_TCP) {
            resultOpType |= HYBM_DOP_TYPE_HOST_TCP;
        }

        return static_cast<hybm_data_op_type>(resultOpType);
    }
};
}  // namespace smem
}  // namespace ock

#endif  // MF_HYBRID_SMEM_HYBM_HELPER_H
