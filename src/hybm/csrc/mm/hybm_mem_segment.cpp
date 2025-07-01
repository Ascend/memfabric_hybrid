/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm_mem_segment.h"
#include "hybm_devide_mem_segment.h"

namespace ock {
namespace mf {
MemSegmentPtr MemSegment::Create(MemSegType segType, const MemSegmentOptions &options, int entityId)
{
    MemSegmentPtr tmpSeg;
    switch (segType) {
        case HyBM_MST_HBM:
            tmpSeg = std::make_shared<MemSegmentDevice>(options, entityId);
            break;
        case HyBM_MST_DRAM:
            BM_LOG_WARN("Un-supported memory seg type " << segType);
            break;
        default:
            BM_LOG_ERROR("Invalid memory seg type " << segType);
    }
    return tmpSeg;
}
}
}
