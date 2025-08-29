/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
