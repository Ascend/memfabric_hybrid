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
    static inline hybm_type TransHybmType(const uint64_t &localDRAMSize, const uint64_t &localHBMSize)
    {
        if (localDRAMSize == 0 && localHBMSize > 0) {
            return HYBM_TYPE_HBM_HOST_INITIATE;
        }
        if (localDRAMSize > 0 && localHBMSize == 0) {
            return HYBM_TYPE_DRAM_HOST_INITIATE;
        }
        if (localDRAMSize > 0 && localHBMSize > 0) {
            return HYBM_TYPE_HBM_DRAM_HOST_INITIATE;
        }
        return HYBM_TYPE_BUTT;
    }

    static inline hybm_data_op_type TransHybmDataOpType(smem_bm_data_op_type smemBmDataOpType)
    {
        switch (smemBmDataOpType) {
            case SMEMB_DATA_OP_SDMA:
                return HYBM_DOP_TYPE_SDMA;
            case SMEMB_DATA_OP_ROCE:
                return HYBM_DOP_TYPE_ROCE;
            case SMEMB_DATA_OP_TCP:
                return HYBM_DOP_TYPE_TCP;
            default:
                return HYBM_DOP_TYPE_BUTT;
        }
    }
};
}
}

#endif // MF_HYBRID_SMEM_HYBM_HELPER_H
