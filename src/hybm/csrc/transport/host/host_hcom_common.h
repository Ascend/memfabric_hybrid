/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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
}
}
}
}

#endif  // MF_HYBRID_HOST_HCOM_COMMON_H
