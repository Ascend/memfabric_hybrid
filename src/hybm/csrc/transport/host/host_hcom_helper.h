/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_HYBRID_HOST_HCOM_HELPER_H
#define MF_HYBRID_HOST_HCOM_HELPER_H

#include <string>
#include <cstdint>

#include "hybm_types.h"
#include "hybm_def.h"
#include "hybm_logger.h"
#include "hcom_service_c_define.h"

namespace ock {
namespace mf {
namespace transport {
namespace host {

class HostHcomHelper {
public:
    static Result AnalysisNic(const std::string &nic, std::string &protocol, std::string &ipStr, int32_t &port);

    static inline Service_Type HybmDopTransHcomProtocol(uint32_t hybmDop)
    {
        switch (hybmDop) {
            case HYBM_DOP_TYPE_TCP:
                return C_SERVICE_TCP;
            case HYBM_DOP_TYPE_MTE:
            case HYBM_DOP_TYPE_ROCE:
                return C_SERVICE_RDMA;
            default:
                BM_LOG_ERROR("Failed to trans hcom protocol, invalid hybmDop: " << hybmDop
                    << " use default protocol rdma: " << C_SERVICE_RDMA);
                return C_SERVICE_RDMA;
        }
    }

private:
    static Result AnalysisNicWithMask(const std::string &nic, std::string &protocol, std::string &ip, int32_t &port);

    static Result SelectLocalIpByIpMask(const std::string &ipStr, const int32_t &mask, std::string &localIp);
};
}
}
}
}
#endif // MF_HYBRID_HOST_HCOM_HELPER_H
