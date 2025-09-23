/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "dl_acl_api.h"
#include "hybm_device_user_mem_seg.h"
#include "hybm_device_mem_segment.h"
#include "hybm_types.h"
#include "hybm_host_mem_segment.h"
#include "hybm_mem_segment.h"

namespace ock {
namespace mf {

bool MemSegment::deviceInfoReady{false};
int MemSegment::deviceId_{-1};
uint32_t MemSegment::pid_{0};
uint32_t MemSegment::sdid_{0};

MemSegmentPtr MemSegment::Create(const MemSegmentOptions &options, int entityId)
{
    if (options.rankId >= options.rankCnt) {
        BM_LOG_ERROR("rank(" << options.rankId << ") but total " << options.rankCnt);
        return nullptr;
    }

    auto ret = MemSegmentDevice::SetDeviceInfo(options.devId);
    if (ret != BM_OK) {
        BM_LOG_ERROR("MemSegmentDevice::GetDeviceId with devId: " << options.devId << " failed: " << ret);
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

Result MemSegment::InitDeviceInfo()
{
    if (deviceInfoReady) {
        return BM_OK;
    }

    auto ret = DlAclApi::AclrtGetDevice(&deviceId_);
    if (ret != 0) {
        BM_LOG_ERROR("get device id failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = DlAclApi::RtDeviceGetBareTgid(&pid_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("get bare tgid failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    constexpr auto sdidInfo = 26;
    int64_t value = 0;
    ret = DlAclApi::RtGetDeviceInfo(deviceId_, 0, sdidInfo, &value);
    if (ret != BM_OK) {
        BM_LOG_ERROR("get sdid failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    sdid_ = static_cast<uint32_t>(value);
    deviceInfoReady = true;
    return BM_OK;
}
}
}
