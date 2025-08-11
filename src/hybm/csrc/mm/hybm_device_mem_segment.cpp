/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <cstring>
#include <iomanip>
#include "dl_api.h"
#include "dl_hal_api.h"
#include "dl_acl_api.h"
#include "devmm_svm_gva.h"
#include "hybm_logger.h"
#include "hybm_ex_info_transfer.h"
#include "hybm_device_mem_segment.h"

namespace ock {
namespace mf {

static const uint64_t EXPORT_INFO_MAGIC = 0xAABB1234FFFFEEEEUL;
static const uint64_t EXPORT_INFO_VERSION = 0x1UL;

Result MemSegmentDevice::ValidateOptions() noexcept
{
    if (options_.segType != HYBM_MST_HBM || options_.size == 0 || options_.devId < 0 ||
        (options_.size % DEVICE_LARGE_PAGE_SIZE) != 0) {
        return BM_INVALID_PARAM;
    }

    return BM_OK;
}

Result MemSegmentDevice::ReserveMemorySpace(void **address) noexcept
{
    if (globalVirtualAddress_ != nullptr) {
        BM_LOG_ERROR("already prepare virtual memory.");
        return BM_ERROR;
    }

    uint64_t base = 0;
    totalVirtualSize_ = options_.rankCnt * options_.size;
    auto ret = drv::HalGvaReserveMemory(&base, totalVirtualSize_, options_.devId, 0ULL);
    if (ret != 0 || base == 0) {
        BM_LOG_ERROR("prepare virtual memory size(" << totalVirtualSize_ << ") failed. ret: " << ret);
        return BM_MALLOC_FAILED;
    }

    globalVirtualAddress_ = reinterpret_cast<uint8_t *>(base);
    allocatedSize_ = 0UL;
    sliceCount_ = 0;
    *address = reinterpret_cast<void *>(base);
    return BM_OK;
}

Result MemSegmentDevice::UnreserveMemorySpace() noexcept
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

    auto sliceAddr = localVirtualBase + allocatedSize_;
    allocatedSize_ += size;
    slice = std::make_shared<MemSlice>(sliceCount_++, MEM_TYPE_DEVICE_HBM, MEM_PT_TYPE_SVM,
                                       static_cast<uint64_t>(reinterpret_cast<std::ptrdiff_t>(sliceAddr)), size);
    slices_.emplace(slice->index_, slice);
    BM_LOG_DEBUG("allocate slice(idx:" << slice->index_ << ", size:" << slice->size_ << ").");

    return BM_OK;
}

Result MemSegmentDevice::RegisterMemory(const void *addr, uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept
{
    BM_LOG_ERROR("MemSegmentDevice NOT SUPPORT RegisterMemory");
    return BM_NOT_SUPPORTED;
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
    BM_LOG_INFO("free slice(idx:" << slice->index_ << "), size: " << slice->size_ << " return:" << res);

    slices_.erase(pos);
    return BM_OK;
}

Result MemSegmentDevice::Export(std::string &exInfo) noexcept
{
    BM_LOG_ERROR("MemSegmentDevice not supported export device info.");
    return BM_ERROR;
}

// export不可重入
Result MemSegmentDevice::Export(const std::shared_ptr<MemSlice> &slice, std::string &exInfo) noexcept
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

    auto exp = exportMap_.find(slice->index_);
    if (exp != exportMap_.end()) {  // RtIpcSetMemoryName不支持重复调用
        exInfo = exp->second;
        return BM_OK;
    }

    HbmExportInfo info{};
    auto ret = DlAclApi::RtIpcSetMemoryName((void *)(ptrdiff_t)slice->vAddress_, slice->size_, info.shmName,
                                            sizeof(info.shmName));
    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "set memory name failed: " << ret);

    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(GetDeviceInfo(), "get device info failed.");
    info.magic = EXPORT_INFO_MAGIC;
    info.version = EXPORT_INFO_VERSION;
    info.mappingOffset =
        slice->vAddress_ - (uint64_t)(ptrdiff_t)(globalVirtualAddress_ + options_.size * options_.rankId);
    info.sliceIndex = static_cast<uint32_t>(slice->index_);
    info.deviceId = options_.devId;
    info.pid = static_cast<int>(pid_);
    info.rankId = options_.rankId;
    info.size = slice->size_;
    info.entityId = entityId_;
    info.sdid = sdid_;
    info.pageTblType = MEM_PT_TYPE_SVM;
    info.memSegType = HYBM_MST_HBM;
    info.exchangeType = HYBM_INFO_EXG_IN_NODE;
    ret = LiteralExInfoTranslater<HbmExportInfo>{}.Serialize(info, exInfo);
    if (ret != BM_OK) {
        auto acl_ret = DlAclApi::RtIpcDestroyMemoryName(info.shmName);
        BM_LOG_ERROR("export info failed: " << ret << ", ipc destroy memory name " << acl_ret);
        return BM_ERROR;
    }

    exportMap_[slice->index_] = exInfo;
    return BM_OK;
}

Result MemSegmentDevice::GetExportSliceSize(size_t &size) noexcept
{
    size = sizeof(HbmExportInfo);
    return BM_OK;
}

// import可重入
Result MemSegmentDevice::Import(const std::vector<std::string> &allExInfo, void *addresses[]) noexcept
{
    LiteralExInfoTranslater<HbmExportInfo> translator;
    std::vector<HbmExportInfo> deserializedInfos{allExInfo.size()};
    for (auto i = 0U; i < allExInfo.size(); i++) {
        auto ret = translator.Deserialize(allExInfo[i], deserializedInfos[i]);
        if (ret != 0) {
            BM_LOG_ERROR("deserialize imported info(" << i << ") failed.");
            return BM_INVALID_PARAM;
        }
    }

    uint32_t localIdx = UINT32_MAX;
    for (auto i = 0U; i < deserializedInfos.size(); i++) {
        if (deserializedInfos[i].magic != EXPORT_INFO_MAGIC) {
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

        if (deserializedInfos[i].deviceId != options_.devId) {
            auto ret = DlAclApi::AclrtDeviceEnablePeerAccess(deserializedInfos[i].deviceId, 0);
            if (ret != 0) {
                BM_LOG_ERROR("enable device access failed:" << ret << " local_device:" << options_.devId
                                                            << " remote_device:" << (int)deserializedInfos[i].deviceId);
                return BM_DL_FUNCTION_FAILED;
            }
        }

        auto ret = DlAclApi::RtSetIpcMemorySuperPodPid(deserializedInfos[localIdx].shmName, deserializedInfos[i].sdid,
                                                       &deserializedInfos[i].pid, 1);
        if (ret != 0) {
            BM_LOG_ERROR("enable white list for rank(" << deserializedInfos[i].rankId << ") failed: " << ret
                                                       << ", local rank = " << options_.rankId
                                                       << ", shmName=" << deserializedInfos[localIdx].shmName);
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
            BM_LOG_INFO("remote slice on rank(" << im.rankId << ") already mapped.");
            continue;
        }

        BM_LOG_DEBUG("remote slice on rank(" << im.rankId << ") prepare to map, size = " << im.size);
        auto ret = drv::HalGvaOpen((uint64_t)remoteAddress, im.shmName, im.size, 0);
        if (ret != BM_OK) {
            BM_LOG_ERROR("HalGvaOpen memory failed:" << ret);
            return BM_DL_FUNCTION_FAILED;
        }
        mappedMem_.insert((uint64_t)remoteAddress);
    }
    imports_.clear();
    return BM_OK;
}

Result MemSegmentDevice::Unmap() noexcept
{
    for (auto va : mappedMem_) {
        int32_t ret = drv::HalGvaClose(va, 0);
        if (ret != 0) {
            BM_LOG_ERROR("HalGvaClose memory failed:" << ret);
        }
    }
    mappedMem_.clear();
    return BM_OK;
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
            int32_t ret = drv::HalGvaClose((*it), 0);
            if (ret != 0) {
                BM_LOG_ERROR("HalGvaClose failed " << ret);
            }
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

    if (static_cast<const uint8_t *>(begin) + size >= globalVirtualAddress_ + totalVirtualSize_) {
        return false;
    }

    return true;
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
        auto ret = drv::HalGvaUnreserveMemory((uint64_t)globalVirtualAddress_);
        if (ret != 0) {
            BM_LOG_ERROR("HalGvaUnreserveMemory failed: " << ret);
        }
        globalVirtualAddress_ = nullptr;
    }
}

Result MemSegmentDevice::GetDeviceInfo() noexcept
{
    if (options_.devId < 0) {
        return BM_INVALID_PARAM;
    }

    if (InitDeviceInfo() != BM_OK) {
        return BM_ERROR;
    }
    return BM_OK;
}
}  // namespace mf
}  // namespace ock
