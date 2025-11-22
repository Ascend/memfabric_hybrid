/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MF_HYBRID_HOST_HCOM_HELPER_H
#define MF_HYBRID_HOST_HCOM_HELPER_H

#include <string>
#include <cstdint>

#include "hybm_types.h"
#include "hybm_def.h"
#include "hybm_logger.h"
#include "hcom_service_c_define.h"
#include "hybm_define.h"
#include "mf_str_util.h"

namespace ock {
namespace mf {
constexpr auto UBC_PROTOCOL_PREFIX = "ubc://";
namespace transport {
namespace host {

class HostHcomHelper {
public:
    static Result AnalysisNic(const std::string &nic, std::string &protocol, std::string &ipStr, uint32_t &port);

    static inline Service_Type HybmDopTransHcomProtocol(uint32_t hybmDop, const std::string &nic)
    {
        if (hybmDop & HYBM_DOP_TYPE_HOST_TCP) {
            return C_SERVICE_TCP;
        }
        if ((hybmDop & HYBM_DOP_TYPE_HOST_RDMA) && StrUtil::StartWith(nic, UBC_PROTOCOL_PREFIX)) {
            return C_SERVICE_UBC;
        }
        if (hybmDop & HYBM_DOP_TYPE_HOST_RDMA) {
            return C_SERVICE_RDMA;
        }
        BM_LOG_ERROR("Failed to trans hcom protocol, invalid hybmDop: " << hybmDop << " use default protocol rdma: "
                                                                        << C_SERVICE_RDMA);
        return C_SERVICE_RDMA;
    }

private:
    static Result AnalysisNicWithMask(const std::string &nic, std::string &protocol, std::string &ip, uint32_t &port);

    static Result SelectLocalIpByIpMask(const std::string &ipStr, const int32_t &mask, std::string &localIp);
};
}  // namespace host
}  // namespace transport
}  // namespace mf
}  // namespace ock
#endif  // MF_HYBRID_HOST_HCOM_HELPER_H