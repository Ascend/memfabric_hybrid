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
    static inline smem_bm_data_op_type TransSmemBmDataOpType(const std::string& dataOpType)
    {
        if (dataOpType == "device_sdma") {
            return SMEMB_DATA_OP_SDMA;
        }
        if (dataOpType == "device_rdma") {
            return SMEMB_DATA_OP_DEVICE_RDMA;
        }
        if (dataOpType == "host_tcp") {
            return SMEMB_DATA_OP_HOST_TCP;
        }
        if (dataOpType == "host_rdma") {
            return SMEMB_DATA_OP_HOST_RDMA;
        }
        return SMEMB_DATA_OP_BUTT;
    }

    static inline smem_tls_config TransSmemTlsConfig(const mmc_tls_config& config)
    {
        smem_tls_config smemConfig = {};
        smemConfig.tlsEnable = config.tlsEnable;
        std::copy_n(config.caPath, SMEM_TLS_PATH_SIZE, smemConfig.caPath);
        std::copy_n(config.crlPath, SMEM_TLS_PATH_SIZE, smemConfig.crlPath);
        std::copy_n(config.certPath, SMEM_TLS_PATH_SIZE, smemConfig.certPath);
        std::copy_n(config.keyPath, SMEM_TLS_PATH_SIZE, smemConfig.keyPath);
        std::copy_n(config.keyPassPath, SMEM_TLS_PATH_SIZE, smemConfig.keyPassPath);
        std::copy_n(config.packagePath, SMEM_TLS_PATH_SIZE, smemConfig.packagePath);
        std::copy_n(config.decrypterLibPath, SMEM_TLS_PATH_SIZE, smemConfig.decrypterLibPath);

        return smemConfig;
    }
};

}
}
#endif // MF_HYBRID_MMC_SMEM_BM_HELPER_H
