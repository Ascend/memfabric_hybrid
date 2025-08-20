/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "dl_acl_api.h"
#include "hybm_mem_segment.h"
#include "hybm_device_user_mem_seg.h"
#include "hybm_device_mem_segment.h"
#include "hybm_types.h"
#include "hybm_host_mem_segment.h"

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
            tmpSeg = std::make_shared<MemSegmentHost>(options, entityId);
            break;
        case HYBM_MST_HBM_USER:
            tmpSeg = std::make_shared<MemSegmentDeviceUseMem>(options, entityId);
            break;
        default:
            BM_LOG_ERROR("Invalid memory seg type " << int(options.segType));
    }
    return tmpSeg;
}

bool MemSegment::CheckSmdaReaches(uint32_t rankId) const noexcept
{
    return false;
}
}
}
