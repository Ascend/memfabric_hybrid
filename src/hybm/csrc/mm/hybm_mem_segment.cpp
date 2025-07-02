/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm_mem_segment.h"
#include "hybm_devide_mem_segment.h"
#include "hybm_types.h"

namespace ock {
namespace mf {
MemSegmentPtr MemSegment::Create(const MemSegmentOptions &options, int entityId)
{
    if (options.rankId >= options.rankCnt) {
        BM_LOG_ERROR("rank(" << options.rankId << ") but total " << options.rankCnt);
        return nullptr;
    }

    MemSegmentPtr tmpSeg;
    switch (options.segType) {
        case HYBM_MST_HBM:
            tmpSeg = std::make_shared<MemSegmentDevice>(options, entityId);
            break;
        case HYBM_MST_DRAM:
            BM_LOG_WARN("Un-supported memory seg type " << options.segType);
            break;
        default:
            BM_LOG_ERROR("Invalid memory seg type " << options.segType);
    }
    return tmpSeg;
}
}
}
