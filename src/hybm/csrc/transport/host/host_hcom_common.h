/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/

#ifndef MF_HYBRID_HOST_HCOM_COMMON_H
#define MF_HYBRID_HOST_HCOM_COMMON_H

#include "hcom_service_c_define.h"
#include "hybm_transport_common.h"

namespace ock {
namespace mf {
namespace transport {
namespace host {
struct RegMemoryKey {
    uint32_t type{TT_HCCP};
    uint32_t reserved{0};
    Service_MemoryRegionInfo hcomInfo;
};

union RegMemoryKeyUnion {
    TransportMemoryKey commonKey;
    RegMemoryKey hostKey;
};
} // namespace host
} // namespace transport
} // namespace mf
} // namespace ock

#endif // MF_HYBRID_HOST_HCOM_COMMON_H
