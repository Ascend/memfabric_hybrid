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
#include "hybm_mem_segment.h"

#include <fstream>
#include <sstream>

#include "hybm_common_include.h"
#include "dl_acl_api.h"
#include "hybm_dev_user_legacy_segment.h"
#include "hybm_dev_legacy_segment.h"
#include "hybm_gva.h"
#include "hybm_types.h"
#include "hybm_host_shm_segment.h"
#include "hybm_conn_based_segment.h"
#include "hybm_vmm_based_segment.h"
#include "hybm_va_manager.h"

namespace ock {
namespace mf {

constexpr uint32_t ASC910B_CONN_RANKS = 8U;

bool MemSegment::deviceInfoReady_{false};
int MemSegment::deviceId_{-1};
int MemSegment::logicDeviceId_{-1};
uint32_t MemSegment::pid_{0};
uint32_t MemSegment::sdid_{0};
uint32_t MemSegment::serverId_{0};
uint32_t MemSegment::superPodId_{0};
std::string MemSegment::sysBoolId_{};
uint32_t MemSegment::bootIdHead_{0};
AscendSocType MemSegment::socType_{AscendSocType::ASCEND_UNKNOWN};
bool MemSegment::atforkRegistered_{false};

void MemSegment::ResetDeviceInfoInChild() noexcept
{
    MemSegment::deviceInfoReady_ = false;
    MemSegment::deviceId_ = -1;
    MemSegment::logicDeviceId_ = -1;
    MemSegment::pid_ = 0;
    MemSegment::sdid_ = 0;
    MemSegment::serverId_ = 0;
    MemSegment::superPodId_ = 0;
    MemSegment::sysBoolId_.clear();
    MemSegment::bootIdHead_ = 0;
    MemSegment::socType_ = ock::mf::AscendSocType::ASCEND_UNKNOWN;
}

MemSegmentPtr MemSegment::Create(const MemSegmentOptions &options, int entityId)
{
    if (options.rankId >= options.rankCnt) {
        BM_LOG_ERROR("rank(" << options.rankId << ") but total " << options.rankCnt);
        return nullptr;
    }

    auto ret = MemSegment::InitDeviceInfo(options.devId);
    if (ret != BM_OK) {
        BM_LOG_ERROR("MemSegment::InitDeviceInfo failed: " << ret);
        return nullptr;
    }
    ret = HybmVaManager::GetInstance().Initialize(socType_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("HybmVaManager Initialize failed: " << ret);
        return nullptr;
    }
    MemSegmentPtr tmpSeg;
    switch (options.segType) {
        case HYBM_MST_HBM:
            if (HybmGetGvaVersion() == HYBM_GVA_V4 || socType_ == AscendSocType::ASCEND_950) {
                tmpSeg = std::make_shared<HybmVmmBasedSegment>(options, entityId);
            } else {
                tmpSeg = std::make_shared<HybmDevLegacySegment>(options, entityId);
            }
            break;
        case HYBM_MST_DRAM:
            // When host shared memory op type is set, use dedicated host shm segment.
            if ((options.dataOpType & HYBM_DOP_TYPE_HOST_SHM) != 0) {
                tmpSeg = std::make_shared<HybmHostShmSegment>(options, entityId);
            } else if ((HybmGetGvaVersion() == HYBM_GVA_V4 && socType_ == AscendSocType::ASCEND_910C &&
                        options.shmFd < 0 && options.dataOpType != HYBM_DOP_TYPE_DEVICE_RDMA) ||
                       (socType_ == AscendSocType::ASCEND_950 && options.dataOpType == HYBM_DOP_TYPE_SDMA)) {
                tmpSeg = std::make_shared<HybmVmmBasedSegment>(options, entityId);
            } else {
                tmpSeg = std::make_shared<HybmConnBasedSegment>(options, entityId);
            }
            break;
        case HYBM_MST_HBM_USER:
            tmpSeg = std::make_shared<HybmDevUserLegacySegment>(options, entityId);
            break;
        default:
            BM_LOG_ERROR("Invalid memory seg type " << int(options.segType));
    }
    return tmpSeg;
}

bool MemSegment::CheckSdmaReaches(uint32_t rankId) const noexcept
{
    return false;
}

Result MemSegment::RegisterMemCommon(const void *addr, uint64_t size, MemSlicePtr &slice)
{
    uint64_t va = reinterpret_cast<uint64_t>(addr);
    bool isHbm = (va >= HYBM_HBM_START_ADDR && va < HYBM_HBM_END_ADDR);
    int ret;
    if (isHbm) {
        slice = std::make_shared<MemSlice>(sliceCount_++, HYBM_MEM_TYPE_DEVICE, MEM_PT_TYPE_SVM, 0, va, size);
        ret = HybmVaManager::GetInstance().AddVaInfo({0, va, va, size, HYBM_MEM_TYPE_DEVICE}, options_.rankId);
        if (ret != 0) {
            BM_LOG_ERROR("add va info failed, va:" << VaToStr(va) << " ret:" << ret);
            return ret;
        }
    } else {
        void *output = nullptr;
#if defined(ASCEND_NPU)
        const bool needDeviceVa = (options_.dataOpType & HYBM_DOP_TYPE_DEVICE_RDMA) != 0U;
        if (needDeviceVa) {
            ret = DlHalApi::HalHostRegister(const_cast<void *>(addr), size, HOST_MEM_MAP_DEV, logicDeviceId_, &output);
            if (ret != 0) {
                BM_LOG_ERROR("RegisterMemory failed, size: " << size << " addr: " << std::hex << addr
                                                             << " ret: " << ret);
                return ret;
            }
        }
#endif
        auto dva = reinterpret_cast<uint64_t>(output);
        slice = std::make_shared<MemSlice>(sliceCount_++, HYBM_MEM_TYPE_HOST, MEM_PT_TYPE_SVM, 0, va, size);
        ret = HybmVaManager::GetInstance().AddVaInfo({0, dva, va, size, HYBM_MEM_TYPE_HOST}, options_.rankId);
        if (ret != 0) {
            BM_LOG_ERROR("add va info failed, va:" << VaToStr(va) << " ret:" << ret);
#if defined(ASCEND_NPU)
            if (needDeviceVa) {
                DlHalApi::HalHostUnregisterEx(const_cast<void *>(addr), logicDeviceId_, HOST_MEM_MAP_DEV);
            }
#endif
            return ret;
        }
    }

    return BM_OK;
}

Result MemSegment::InitDeviceInfo(int devId)
{
#if !defined(ASCEND_NPU)
    return BM_OK;
#endif

    if (!atforkRegistered_) {
        static std::once_flag flag;
        std::call_once(flag, []() {
            if (pthread_atfork(nullptr, nullptr, []() { MemSegment::ResetDeviceInfoInChild(); }) != 0) {
                BM_LOG_ERROR("Failed to register pthread_atfork handler!");
            }
        });
        atforkRegistered_ = true;
    }

    if (deviceInfoReady_) {
        return (deviceId_ == devId ? BM_OK : BM_INVALID_PARAM);
    }

    FillSysBootIdInfo();

    deviceId_ = devId;
    auto ret = DlAclApi::AclrtSetDevice(devId, true);
    if (ret != 0) {
        BM_LOG_ERROR("set device to:" << devId << "failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = DlAclApi::RtGetLogicDevIdByUserDevId(devId, &logicDeviceId_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("get logic deviceId failed: " << ret << " userId:" << devId);
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

    socType_ = DlAclApi::GetAscendSocType();
    BM_LOG_DEBUG("local sdid=0x" << std::hex << sdid_ << ", local server=0x" << std::hex << serverId_
                                 << ", spid=" << superPodId_);
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
} // namespace mf
} // namespace ock
