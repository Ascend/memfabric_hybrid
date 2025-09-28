/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "hybm_device_mem_segment.h"

#include <cstring>

#include "devmm_svm_gva.h"
#include "dl_acl_api.h"
#include "hybm_common_include.h"
#include "hybm_ex_info_transfer.h"
#include "hybm_gvm_user.h"
#include "hybm_logger.h"
#include "hybm_networks_common.h"

namespace ock {
namespace mf {
static constexpr uint32_t invalidSuperPodId = 0xFFFFFFFFU;
static constexpr uint32_t invalidServerId = 0x3FFU;

int MemSegmentDevice::deviceId_{-1};
int MemSegmentDevice::logicDeviceId_{-1};
int MemSegmentDevice::pid_{-1};
uint32_t MemSegmentDevice::sdid_{0};
uint32_t MemSegmentDevice::serverId_{0};
uint32_t MemSegmentDevice::superPodId_{0};

Result MemSegmentDevice::ValidateOptions() noexcept
{
    if (options_.segType != HYBM_MST_HBM || options_.size == 0 || (options_.size % DEVICE_LARGE_PAGE_SIZE) != 0) {
        BM_LOG_ERROR("Invalid options segType:" << options_.segType << " size:" << options_.size);
        return BM_INVALID_PARAM;
    }

    return BM_OK;
}

Result MemSegmentDevice::ReserveMemorySpace(void **address) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(ValidateOptions() == BM_OK, "Failed to validate options.", BM_INVALID_PARAM);
    if (globalVirtualAddress_ != nullptr) {
        BM_LOG_ERROR("already prepare virtual memory.");
        return BM_ERROR;
    }

    uint64_t base = 0;
    totalVirtualSize_ = options_.rankCnt * options_.size;
    auto ret = drv::HalGvaReserveMemory(&base, totalVirtualSize_, logicDeviceId_, 0ULL);
    if (ret != 0 || base == 0) {
        BM_LOG_ERROR("prepare virtual memory size(" << totalVirtualSize_ << ") failed. ret: " << ret);
        return BM_MALLOC_FAILED;
    }

    globalVirtualAddress_ = reinterpret_cast<uint8_t *>(base);
    allocatedSize_ = 0UL;
    sliceCount_ = 0;
    *address = globalVirtualAddress_;
    return BM_OK;
}

Result MemSegmentDevice::UnReserveMemorySpace() noexcept
{
    BM_LOG_INFO("un-reserve memory space.");
    FreeMemory();
    return BM_OK;
}

Result MemSegmentDevice::AllocLocalMemory(uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept
{
    if ((size % DEVICE_LARGE_PAGE_SIZE) != 0UL || size + allocatedSize_ > options_.size) {
        BM_LOG_ERROR("invalid allocate memory size : " << size << ", now used " << allocatedSize_ << " of "
                                                       << options_.size);
        return BM_INVALID_PARAM;
    }

    auto localVirtualBase = globalVirtualAddress_ + options_.size * options_.rankId;
    auto ret = drv::HalGvaAlloc((uint64_t)(localVirtualBase + allocatedSize_), size, 0);
    if (ret != BM_OK) {
        BM_LOG_ERROR("HalGvaAlloc memory failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    if (HybmGvmHasInited()) {
        ret = hybm_gvm_mem_fetch((uint64_t)(localVirtualBase + allocatedSize_), size, 0);
        if (ret != BM_OK) {
            drv::HalGvaFree((uint64_t)(localVirtualBase + allocatedSize_), size);
            BM_LOG_ERROR("Failed to fetch gvm memory failed: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }
    }

    auto sliceAddr = localVirtualBase + allocatedSize_;
    allocatedSize_ += size;
    slice = std::make_shared<MemSlice>(sliceCount_++, MEM_TYPE_DEVICE_HBM, MEM_PT_TYPE_SVM,
                                       reinterpret_cast<uint64_t>(sliceAddr), size);
    slices_.emplace(slice->index_, slice);
    BM_LOG_DEBUG("allocate slice(idx:" << slice->index_ << ", size:" << slice->size_ << ").");

    return BM_OK;
}

Result MemSegmentDevice::Export(std::string &exInfo) noexcept
{
    return BM_OK;
}

// export不可重入
Result MemSegmentDevice::Export(const std::shared_ptr<MemSlice> &slice, std::string &exInfo) noexcept
{
    BM_ASSERT_RETURN(slice != nullptr, BM_INVALID_PARAM);

    auto pos = slices_.find(slice->index_);
    if (pos == slices_.end()) {
        BM_LOG_ERROR("input slice(idx:" << slice->index_ << ") not exist.");
        return BM_INVALID_PARAM;
    }

    if (pos->second.slice != slice) {
        BM_LOG_ERROR("input slice(magic:" << std::hex << slice->magic_ << ") not match.");
        return BM_INVALID_PARAM;
    }

    auto exp = exportMap_.find(slice->index_);
    if (exp != exportMap_.end()) { // RtIpcSetMemoryName不支持重复调用
        exInfo = exp->second;
        return BM_OK;
    }

    HbmExportInfo info;
    auto ret = DlAclApi::RtIpcSetMemoryName((void *)(ptrdiff_t)slice->vAddress_, slice->size_, info.shmName,
                                            sizeof(info.shmName));
    if (ret != 0) {
        BM_LOG_ERROR("set memory name failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    info.mappingOffset =
        slice->vAddress_ - (uint64_t)(ptrdiff_t)(globalVirtualAddress_ + options_.size * options_.rankId);
    info.sliceIndex = static_cast<uint32_t>(slice->index_);
    info.deviceId = deviceId_;
    info.pid = pid_;
    info.rankId = options_.rankId;
    info.size = slice->size_;
    info.entityId = entityId_;
    info.sdid = sdid_;
    info.serverId = serverId_;
    info.superPodId = superPodId_;
    info.logicDeviceId = logicDeviceId_;
    info.pageTblType = MEM_PT_TYPE_SVM;
    info.memSegType = HYBM_MST_HBM;
    info.exchangeType = HYBM_INFO_EXG_IN_NODE;
    ret = LiteralExInfoTranslater<HbmExportInfo>{}.Serialize(info, exInfo);
    if (ret != BM_OK) {
        BM_LOG_ERROR("export info failed: " << ret);
        DlAclApi::RtIpcDestroyMemoryName(info.shmName);
        return BM_ERROR;
    }

    exportMap_[slice->index_] = exInfo;
    return BM_OK;
}

// import可重入
Result MemSegmentDevice::Import(const std::vector<std::string> &allExInfo) noexcept
{
    std::map<uint16_t, HbmExportInfo> importMap;
    LiteralExInfoTranslater<HbmExportInfo> translator;
    std::vector<HbmExportInfo> deserializedInfos(allExInfo.size());
    for (auto i = 0U; i < allExInfo.size(); i++) {
        auto ret = translator.Deserialize(allExInfo[i], deserializedInfos[i]);
        if (ret != 0) {
            BM_LOG_ERROR("deserialize imported info(" << i << ") failed.");
            return BM_INVALID_PARAM;
        }
        importMap.emplace(deserializedInfos[i].rankId, deserializedInfos[i]);
    }
    importMap_ = std::move(importMap);

    uint32_t localIdx = UINT32_MAX;
    for (auto i = 0U; i < deserializedInfos.size(); i++) {
        if (deserializedInfos[i].magic != HBM_SLICE_EXPORT_INFO_MAGIC) {
            BM_LOG_ERROR("import info(" << i << ") magic(" << deserializedInfos[i].magic << ") invalid.");
            return BM_INVALID_PARAM;
        }

        if (deserializedInfos[i].rankId == options_.rankId) {
            localIdx = i;
        }
    }
    BM_ASSERT_RETURN(localIdx < deserializedInfos.size(), BM_INVALID_PARAM);

    for (auto i = 0U; i < deserializedInfos.size(); i++) {
        if (deserializedInfos[i].rankId == options_.rankId) {
            continue;
        }

        if (deserializedInfos[i].logicDeviceId != logicDeviceId_) {
            auto ret = DlAclApi::RtEnableP2P(deviceId_, deserializedInfos[i].logicDeviceId, 0);
            if (ret != 0) {
                BM_LOG_ERROR("enable device access failed:"
                             << ret << " local_device:" << deviceId_ << " remote_device:"
                             << (int)deserializedInfos[i].deviceId << " logic_device:" << logicDeviceId_
                             << " remote_logic_device:" << deserializedInfos[i].logicDeviceId);
                return BM_DL_FUNCTION_FAILED;
            }
        }

        auto ret = DlAclApi::RtSetIpcMemorySuperPodPid(deserializedInfos[localIdx].shmName, deserializedInfos[i].sdid,
                                                       &deserializedInfos[i].pid, 1);
        if (ret != 0) {
            BM_LOG_ERROR("enable white list for rank(" << deserializedInfos[i].rankId << ") failed: " << ret
                << ", local rank = " << options_.rankId << ", shmName=" << deserializedInfos[localIdx].shmName);
            return BM_DL_FUNCTION_FAILED;
        }
    }

    std::copy(deserializedInfos.begin(), deserializedInfos.end(), std::back_inserter(imports_));
    return BM_OK;
}

Result MemSegmentDevice::Mmap() noexcept
{
    if (imports_.empty()) {
        return BM_OK;
    }

    for (auto &im : imports_) {
        if (im.rankId == options_.rankId) {
            continue;
        }

        auto remoteAddress = globalVirtualAddress_ + options_.size * im.rankId + im.mappingOffset;
        if (mappedMem_.find((uint64_t)remoteAddress) != mappedMem_.end()) {
            BM_LOG_INFO("remote slice on rank(" << im.rankId << ") has maped");
            continue;
        }

        if (!CanMapRemote(im)) {
            BM_LOG_INFO("remote slice on rank(" << im.rankId << ") SDMA cannot reaches.");
            continue;
        }

        BM_LOG_DEBUG("remote slice on rank(" << im.rankId << ") should map to" << ", size = " << im.size);
        auto ret = drv::HalGvaOpen((uint64_t)remoteAddress, im.shmName, im.size, 0);
        if (ret != BM_OK) {
            BM_LOG_ERROR("HalGvaOpen memory failed:" << ret);
            return -1;
        }
        mappedMem_.insert((uint64_t)remoteAddress);

        if (HybmGvmHasInited()) {
            ret = hybm_gvm_mem_fetch((uint64_t)remoteAddress, im.size, im.sdid);
            if (ret != BM_OK) {
                drv::HalGvaClose((uint64_t)remoteAddress, 0);
                BM_LOG_WARN("hybm_gvm_mem_fetch memory failed: " << ret);
            }
        }
    }
    imports_.clear();
    return BM_OK;
}

Result MemSegmentDevice::Unmap() noexcept
{
    for (auto va : mappedMem_) {
        (void)drv::HalGvaClose(va, 0);
    }
    mappedMem_.clear();

    return 0;
}

Result MemSegmentDevice::RemoveImported(const std::vector<uint32_t> &ranks) noexcept
{
    for (auto &rank : ranks) {
        if (rank >= options_.rankCnt) {
            BM_LOG_ERROR("input rank is invalid! rank:" << rank << " rankSize:" << options_.rankCnt);
            return BM_INVALID_PARAM;
        }
    }

    for (auto &rank : ranks) {
        uint64_t addr = reinterpret_cast<uint64_t>(globalVirtualAddress_) + options_.size * rank;
        auto it = mappedMem_.lower_bound(addr);
        auto st = it;
        while (it != mappedMem_.end() && (*it) < addr + options_.size) {
            (void)drv::HalGvaClose((*it), 0);
            it++;
        }

        if (st != it) {
            mappedMem_.erase(st, it);
        }
    }
    return 0;
}

std::shared_ptr<MemSlice> MemSegmentDevice::GetMemSlice(hybm_mem_slice_t slice) const noexcept
{
    auto index = MemSlice::GetIndexFrom(slice);
    auto pos = slices_.find(index);
    if (pos == slices_.end()) {
        return nullptr;
    }

    auto target = pos->second.slice;
    if (!target->ValidateId(slice)) {
        return nullptr;
    }

    return target;
}

bool MemSegmentDevice::MemoryInRange(const void *begin, uint64_t size) const noexcept
{
    if (begin < globalVirtualAddress_) {
        return false;
    }

    if (reinterpret_cast<const uint8_t *>(begin) + size > globalVirtualAddress_ + totalVirtualSize_) {
        return false;
    }

    return true;
}

Result MemSegmentDevice::ReleaseSliceMemory(const std::shared_ptr<MemSlice> &slice) noexcept
{
    if (slice == nullptr) {
        BM_LOG_ERROR("input slice is nullptr");
        return BM_INVALID_PARAM;
    }

    auto pos = slices_.find(slice->index_);
    if (pos == slices_.end()) {
        BM_LOG_ERROR("input slice(idx:" << slice->index_ << ") not exist.");
        return BM_INVALID_PARAM;
    }

    if (pos->second.slice != slice) {
        BM_LOG_ERROR("input slice(magic:" << std::hex << slice->magic_ << ") not match.");
        return BM_INVALID_PARAM;
    }

    auto res = drv::HalGvaFree(slice->vAddress_, slice->size_);
    BM_LOG_INFO("free slice(idx:" << slice->index_ << ") size: " << slice->size_ << " return:" << res);

    slices_.erase(pos);
    return BM_OK;
}

void MemSegmentDevice::FreeMemory() noexcept
{
    while (!slices_.empty()) {
        auto slice = slices_.begin()->second.slice;
        ReleaseSliceMemory(slice);
    }

    allocatedSize_ = 0;
    sliceCount_ = 0;
    if (globalVirtualAddress_ != nullptr) {
        auto ret = drv::HalGvaUnreserveMemory();
        if (ret != 0) {
            BM_LOG_ERROR("HalGvaUnreserveMemory failed: " << ret);
        }
        globalVirtualAddress_ = nullptr;
    }
}

int MemSegmentDevice::GetDeviceId(int deviceId) noexcept
{
    if (deviceId < 0) {
        return BM_INVALID_PARAM;
    }

    if (deviceId_ >= 0) {
        if (deviceId == deviceId_) {
            return 0;
        }

        return BM_INVALID_PARAM;
    }

    auto logicDeviceId = Func::GetLogicDeviceId(deviceId);
    if (logicDeviceId < 0) {
        BM_LOG_ERROR("Failed to get logic deviceId: " << deviceId);
        return BM_ERROR;
    }

    uint32_t tgid = 0;
    auto ret = DlAclApi::RtDeviceGetBareTgid(&tgid);
    if (ret != BM_OK) {
        BM_LOG_ERROR("get bare tgid failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    deviceId_ = deviceId;
    logicDeviceId_ = logicDeviceId;
    pid_ = static_cast<int>(tgid);
    ret = FillDeviceSuperPodInfo();
    if (ret != BM_OK) {
        BM_LOG_ERROR("FillDeviceSuperPodInfo() failed: " << ret);
        return ret;
    }

    return BM_OK;
}

Result MemSegmentDevice::GetDeviceInfo() noexcept
{
    if (options_.segId < 0) {
        return BM_INVALID_PARAM;
    }

    if (InitDeviceInfo() != BM_OK) {
        return BM_ERROR;
    }
    return BM_OK;
}

int MemSegmentDevice::FillDeviceSuperPodInfo() noexcept
{
    int64_t value = 0;

    auto ret = DlAclApi::RtGetDeviceInfo(deviceId_, 0, INFO_TYPE_SDID, &value);
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
    BM_LOG_DEBUG("local server id=0x" << std::hex << serverId_);

    ret = DlAclApi::RtGetDeviceInfo(deviceId_, 0, INFO_TYPE_SUPER_POD_ID, &value);
    if (ret != BM_OK) {
        BM_LOG_ERROR("get super pod id failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    superPodId_ = static_cast<uint32_t>(value);

    if (superPodId_ == invalidSuperPodId && serverId_ == invalidServerId) {
        auto networks = NetworkGetIpAddresses();
        if (networks.empty()) {
            BM_LOG_ERROR("get local host ip address empty.");
            return BM_ERROR;
        }

        serverId_ = networks[0];
    }

    BM_LOG_DEBUG("local sdid=0x" << std::hex << sdid_ << ", local server id=0x" << std::hex << serverId_
                                 << ", spid=" << superPodId_);

    return BM_OK;
}

bool MemSegmentDevice::CanMapRemote(const HbmExportInfo &rmi) noexcept
{
    if (rmi.serverId == serverId_) {
        BM_LOG_DEBUG("map from rank(" << rmi.rankId << ") on sample host, can map.");
        return true;
    }

    if (rmi.superPodId == invalidSuperPodId || superPodId_ == invalidSuperPodId) {
        BM_LOG_INFO("map from rank(" << rmi.rankId << ") spid: " << rmi.superPodId << ", local: " << superPodId_
                                     << " cannot map.");
        return false;
    }

    return rmi.superPodId == superPodId_;
}

uint32_t MemSegmentDevice::GetRankIdByAddr(const void *addr, uint64_t size) const noexcept
{
    if (!MemoryInRange(addr, size)) {
        return UINT32_MAX;
    }
    return (reinterpret_cast<uint64_t>(addr) - reinterpret_cast<uint64_t>(globalVirtualAddress_)) / options_.size;
}

bool MemSegmentDevice::CheckSmdaReaches(uint32_t rankId) const noexcept
{
    auto pos = importMap_.find(static_cast<uint16_t>(rankId));
    if (pos == importMap_.end()) {
        return false;
    }

    if (pos->second.serverId == serverId_) {
        return true;
    }

    if (pos->second.superPodId == invalidSuperPodId || superPodId_ == invalidSuperPodId) {
        return false;
    }

    return pos->second.superPodId == superPodId_;
}
}
}
