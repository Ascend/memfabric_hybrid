/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "hybm_mem_segment.h"

#include <fstream>
#include <sstream>

#include "dl_acl_api.h"
#include "hybm_networks_common.h"
#include "hybm_device_user_mem_seg.h"
#include "hybm_device_mem_segment.h"
#include "hybm_types.h"
#include "hybm_host_mem_segment.h"
#include "hybm_sdma_mem_segment.h"
#include "hybm_types.h"

namespace ock {
namespace mf {

constexpr uint32_t ASC910B_CONN_RANKS = 8U;

bool MemSegment::deviceInfoReady_{false};
int MemSegment::deviceId_{-1};
uint32_t MemSegment::pid_{0};
uint32_t MemSegment::sdid_{0};
uint32_t MemSegment::serverId_{0};
uint32_t MemSegment::superPodId_{0};
std::string MemSegment::sysBoolId_{};
uint32_t MemSegment::bootIdHead_{0};
AscendSocType MemSegment::socType_{AscendSocType::ASCEND_UNKNOWN};

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
            if (HybmGvmHasInited()) {
                tmpSeg = std::make_shared<MemSegmentHostSDMA>(options, entityId);
            } else {
                tmpSeg = std::make_shared<MemSegmentHost>(options, entityId);
            }
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
    if (deviceInfoReady_) {
        return BM_OK;
    }

    FillSysBootIdInfo();

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

    int64_t value = 0;
    ret = DlAclApi::RtGetDeviceInfo(deviceId_, 0, INFO_TYPE_SDID, &value);
    if (ret != BM_OK) {
        BM_LOG_ERROR("get sdid failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    sdid_ = static_cast<uint32_t>(value);
    ret = DlAclApi::RtGetDeviceInfo(deviceId_, 0, INFO_TYPE_SERVER_ID, &value);
    if (ret != BM_OK) {
        BM_LOG_ERROR("get server id failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    serverId_ = static_cast<uint32_t>(value);
    BM_LOG_DEBUG("local server=0x" << std::hex << serverId_);

    ret = DlAclApi::RtGetDeviceInfo(deviceId_, 0, INFO_TYPE_SUPER_POD_ID, &value);
    if (ret != BM_OK) {
        BM_LOG_ERROR("get super pod id failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    superPodId_ = static_cast<uint32_t>(value);
    if (superPodId_ == invalidSuperPodId && serverId_ == invalidServerId) {
        if (bootIdHead_ != 0) {
            serverId_ = bootIdHead_;
        } else {
            auto networks = NetworkGetIpAddresses();
            if (networks.empty()) {
                BM_LOG_ERROR("get local host ip address empty.");
                return BM_ERROR;
            }
            serverId_ = networks[0];
        }
    }

    auto name = DlAclApi::AclrtGetSocName();
    if (name == nullptr) {
        BM_LOG_ERROR("AclrtGetSocName() failed.");
        return BM_ERROR;
    }

    std::string socName{name};
    if (socName.find("Ascend910B") != std::string::npos) {
        socType_ = AscendSocType::ASCEND_910B;
    } else if (socName.find("Ascend910_93") != std::string::npos) {
        socType_ = AscendSocType::ASCEND_910C;
    }

    BM_LOG_DEBUG("local sdid=0x" << std::hex << sdid_ << ", local server=0x" << std::hex << serverId_
                                 << ", spid=" << superPodId_ << ", socName=" << socName);
    deviceInfoReady_ = true;
    return BM_OK;
}

bool MemSegment::CanLocalHostReaches(uint32_t superPodId, uint32_t serverId, uint32_t deviceId) noexcept
{
    if (superPodId != superPodId_ || serverId != serverId_) {
        return false;
    }
    return (socType_ != ASCEND_910B) || ((deviceId / ASC910B_CONN_RANKS) == (deviceId_ / ASC910B_CONN_RANKS));
}

bool MemSegment::CanSdmaReaches(uint32_t superPodId, uint32_t serverId, uint32_t deviceId) noexcept
{
    if (serverId == serverId_) {
        return (socType_ != ASCEND_910B) || ((deviceId / ASC910B_CONN_RANKS) == (deviceId_ / ASC910B_CONN_RANKS));
    }

    if (superPodId == invalidSuperPodId || superPodId_ == invalidSuperPodId) {
        BM_LOG_DEBUG("spid: " << superPodId << ", local: " << superPodId_ << " cannot reach.");
        return false;
    }

    return superPodId == superPodId_;
}

void MemSegment::FillSysBootIdInfo() noexcept
{
    std::string bootIdPath("/proc/sys/kernel/random/boot_id");
    std::ifstream input(bootIdPath);
    input >> sysBoolId_;

    std::stringstream ss(sysBoolId_);
    ss >> std::hex >> bootIdHead_;
    BM_LOG_DEBUG("os-boot-id: " << sysBoolId_ << ", head u32: " << std::hex << bootIdHead_);
}
}
}
