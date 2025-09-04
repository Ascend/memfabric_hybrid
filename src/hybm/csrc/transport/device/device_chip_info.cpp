/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#include "dl_acl_api.h"
#include "hybm_logger.h"
#include "device_chip_info.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
int DeviceChipInfo::Init() noexcept
{
    int64_t infoValue = 0U;
    auto ret = DlAclApi::RtGetDeviceInfo(deviceId_, 0, INFO_TYPE_PHY_CHIP_ID, &infoValue);
    if (ret != 0) {
        BM_LOG_ERROR("RtGetDeviceInfo(INFO_TYPE_PHY_CHIP_ID) failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    chipId_ = static_cast<uint32_t>(infoValue);

    ret = DlAclApi::RtGetDeviceInfo(deviceId_, 0, INFO_TYPE_PHY_DIE_ID, &infoValue);
    if (ret != 0) {
        BM_LOG_ERROR("RtGetDeviceInfo(INFO_TYPE_PHY_DIE_ID) failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    dieId_ = static_cast<uint32_t>(infoValue);

    chipDieOffset_ = 0x10000000000UL; // RT_ASCEND910B1_DIE_ADDR_OFFSET;
    chipOffset_ = 0x80000000000UL; // RT_ASCEND910B1_CHIP_ADDR_OFFSET;
    chipBaseAddr_ = 0UL;
    ret = DlAclApi::RtGetDeviceInfo(deviceId_, 0, INFO_TYPE_ADDR_MODE, &infoValue);
    if (ret != 0) {
        BM_LOG_ERROR("RtGetDeviceInfo(INFO_TYPE_ADDR_MODE) failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    addrFlatDevice_ = (infoValue != 0L);
    if (addrFlatDevice_) {
        chipBaseAddr_ = 0x200000000000ULL; // RT_CHIP_BASE_ADDR;
        chipOffset_ = 0x20000000000UL; // RT_ASCEND910B1_HCCS_CHIP_ADDR_OFFSET;
    }

    return 0;
}
}
}
}

}