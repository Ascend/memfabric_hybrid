/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
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
