/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_HYBRID_MMC_SMEM_BM_HELPER_H
#define MF_HYBRID_MMC_SMEM_BM_HELPER_H

#include <string>
#include "smem_bm_def.h"

namespace ock {
namespace mmc {
class MmcSmemBmHelper {
public:
    static inline smem_bm_data_op_type TransSmemBmDataOpType(const mmc_bm_create_config_t& config)
    {
        if (config.dataOpType == "device_sdma") {
            return SMEMB_DATA_OP_SDMA;
        }
        if (config.dataOpType == "device_rdma") {
            return SMEMB_DATA_OP_DEVICE_RDMA;
        }
        if (config.dataOpType == "host_tcp") {
            return SMEMB_DATA_OP_HOST_TCP;
        }
        if (config.dataOpType == "host_rdma") {
            return SMEMB_DATA_OP_HOST_RDMA;
        }
        return SMEMB_DATA_OP_BUTT;
    }
};

}
}
#endif // MF_HYBRID_MMC_SMEM_BM_HELPER_H
