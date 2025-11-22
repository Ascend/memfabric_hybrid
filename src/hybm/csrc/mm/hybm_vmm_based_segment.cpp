/*
Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#include "hybm_vmm_based_segment.h"

#include <cstdint>

#include "hybm_ex_info_transfer.h"
#include "hybm_vmm_based_segment.h"
#include "dl_acl_api.h"
#include "hybm_gvm_user.h"
#include "hybm_types.h"

using namespace ock::mf;

#ifdef USE_VMM
Result HybmVmmBasedSegment::ValidateOptions() noexcept
{
    if (options_.size == 0 || (options_.size % HYBM_LARGE_PAGE_SIZE) != 0) {
        BM_LOG_ERROR("Invalid options segType:" << options_.segType << " size:" << options_.size);
        return BM_INVALID_PARAM;
    }

    if (UINT64_MAX / options_.size < options_.rankCnt) {
        BM_LOG_ERROR("Validate options error rankCnt(" << options_.rankCnt << ") size(" << options_.size);
        return BM_INVALID_PARAM;
    }

    return BM_OK;
}

Result HybmVmmBasedSegment::ReserveMemorySpace(void **address) noexcept
{
    static uint64_t usedOffset = 0U;
    BM_ASSERT_RETURN(address != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_LOG_AND_RETURN(ValidateOptions() == BM_OK, "Failed to validate options.", BM_INVALID_PARAM);
    if (globalVirtualAddress_ != nullptr) {
        BM_LOG_ERROR("already prepare virtual memory.");
        return BM_ERROR;
    }

    if (options_.rankId >= options_.rankCnt) {
        BM_LOG_ERROR("rank(" << options_.rankId << ") but total " << options_.rankCnt);
        return BM_INVALID_PARAM;
    }

    void *base = nullptr;
    uint64_t expectSt = HYBM_GVM_START_ADDR + usedOffset;
    totalVirtualSize_ = options_.rankCnt * options_.size;
    if (expectSt + totalVirtualSize_ > HYBM_GVM_END_ADDR) {
        BM_LOG_ERROR("reserve mem is too large! used:" << std::hex << usedOffset << " size:" << totalVirtualSize_);
        return BM_INVALID_PARAM;
    }

    auto ret = DlHalApi::HalMemAddressReserve(&base, totalVirtualSize_, 0,
        reinterpret_cast<void *>(expectSt), MEM_RSV_TYPE_REMOTE_MAP | MEM_RSV_TYPE_DEVICE_SHARE);
    if (ret != 0 || base != reinterpret_cast<void *>(expectSt)) {
        BM_LOG_ERROR("prepare virtual memory size(" << totalVirtualSize_ << ") failed. ret: " << ret);
        return BM_MALLOC_FAILED;
    }

    globalVirtualAddress_ = reinterpret_cast<uint8_t *>(base);
    allocatedSize_ = 0UL;
    sliceCount_ = 0;
    *address = base;
    usedOffset += totalVirtualSize_;
    return BM_OK;
}

Result HybmVmmBasedSegment::UnReserveMemorySpace() noexcept
{
    if (globalVirtualAddress_ != nullptr) {
        DlHalApi::HalMemAddressFree(reinterpret_cast<void *>(globalVirtualAddress_));
        globalVirtualAddress_ = nullptr;
    }
    return BM_OK;
}

Result HybmVmmBasedSegment::AllocLocalMemory(uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept
{
    if ((size % HYBM_LARGE_PAGE_SIZE) != 0UL || size + allocatedSize_ > options_.size) {
        BM_LOG_ERROR("invalid allocate memory size : " << size << ", now used " << allocatedSize_ << " of "
                                                       << options_.size);
        return BM_INVALID_PARAM;
    }

    BM_ASSERT_RETURN(globalVirtualAddress_ != nullptr, BM_NOT_INITIALIZED);

    auto localVirtualBase = globalVirtualAddress_ + options_.size * options_.rankId;
    uint64_t allocAddr = reinterpret_cast<uint64_t>(localVirtualBase + allocatedSize_);
    drv_mem_handle_t *handle = nullptr;
    drv_mem_prop prop{};
    switch (options_.segType) {
        case HYBM_MST_HBM:
            prop = {MEM_DEV_SIDE, static_cast<uint32_t>(options_.devId), 0, MEM_GIANT_PAGE_TYPE, MEM_HBM_TYPE, 0};
            break;
        case HYBM_MST_DRAM:
            prop = {MEM_HOST_SIDE, static_cast<uint32_t>(options_.devId), 0, MEM_GIANT_PAGE_TYPE, MEM_P2P_DDR_TYPE, 0};
            break;
        default:
            BM_LOG_ERROR("invalid seg type: " << options_.segType);
            return BM_INVALID_PARAM;
    }
    // check is support 1GB GIANT_PAGE
    size_t granularity = 0;
    if (DlHalApi::HalMemGetAllocationGranularity(&prop, MEM_ALLOC_GRANULARITY_RECOMMENDED, &granularity) != 0) {
        prop.pg_type = MEM_HUGE_PAGE_TYPE;
        BM_LOG_WARN("Not support MEM_GIANT_PAGE_TYPE page size change use MEM_HUGE_PAGE_TYPE");
    }
    auto ret = DlHalApi::HalMemCreate(&handle, size, &prop, 0);
    BM_VALIDATE_RETURN(ret == BM_OK, "HalMemCreate failed: " << ret, BM_DL_FUNCTION_FAILED);

    allocatedSize_ += size;
    auto memType = options_.segType == HYBM_MST_HBM ? MEM_TYPE_DEVICE_HBM : MEM_TYPE_HOST_DRAM;
    slice = std::make_shared<MemSlice>(sliceCount_++, memType, MEM_PT_TYPE_GVM, allocAddr, size);
    BM_ASSERT_RETURN(slice != nullptr, BM_MALLOC_FAILED);
    slices_.emplace(slice->index_, MemSliceStatus(slice, reinterpret_cast<void *>(handle)));

    MemShareHandle sHandle = {};
    ret = ExportInner(slice, sHandle);
    if (ret != BM_OK) {
        BM_LOG_ERROR("export failed: " << ret);
        DlHalApi::HalMemRelease(handle);
        return BM_ERROR;
    }

    drv_mem_handle_t *dHandle = handle;
    if (options_.segType == HYBM_MST_DRAM) {
        ret = DlHalApi::HalMemImport(MEM_HANDLE_TYPE_FABRIC, &sHandle, static_cast<uint32_t>(options_.devId), &dHandle);
        BM_VALIDATE_RETURN(ret == BM_OK, "HalMemImport memory failed:" << ret, BM_ERROR);
    }

    ret = DlHalApi::HalMemMap(reinterpret_cast<void *>(allocAddr), size, 0, dHandle, 0);
    if (ret != BM_OK) {
        BM_LOG_ERROR("HalMemMap failed: " << ret);
        DlHalApi::HalMemRelease(dHandle);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}

Result HybmVmmBasedSegment::RegisterMemory(const void *addr, uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept
{
    BM_LOG_ERROR("HybmVmmBasedSegment NOT SUPPORT RegisterMemory");
    return BM_NOT_SUPPORTED;
}

Result HybmVmmBasedSegment::ReleaseSliceMemory(const std::shared_ptr<MemSlice> &slice) noexcept
{
    BM_VALIDATE_RETURN(slice != nullptr, "input slice is nullptr", BM_INVALID_PARAM);

    auto pos = slices_.find(slice->index_);
    if (pos == slices_.end()) {
        BM_LOG_ERROR("input slice(idx:" << slice->index_ << ") not exist.");
        return BM_INVALID_PARAM;
    }
    if (pos->second.slice != slice) {
        BM_LOG_ERROR("input slice(magic:" << std::hex << slice->magic_ << ") not match.");
        return BM_INVALID_PARAM;
    }

    auto res = DlHalApi::HalMemUnmap(reinterpret_cast<void *>(slice->vAddress_));
    BM_LOG_INFO("unmap slice(idx:" << slice->index_ << "), size: " << slice->size_ << " return:" << res);

    res = DlHalApi::HalMemRelease(reinterpret_cast<drv_mem_handle_t *>(pos->second.handle));
    BM_LOG_INFO("release slice(idx:" << slice->index_ << "), size: " << slice->size_ << " return:" << res);

    slices_.erase(pos);
    return BM_OK;
}

Result HybmVmmBasedSegment::Export(std::string &exInfo) noexcept
{
    return BM_OK;
}

Result HybmVmmBasedSegment::ExportInner(const std::shared_ptr<MemSlice> &slice, MemShareHandle &sHandle) noexcept
{
    HostSdmaExportInfo info;
    std::string exInfo;
    auto pos = slices_.find(slice->index_);
    auto ret = DlHalApi::HalMemExport(reinterpret_cast<drv_mem_handle_t *>(pos->second.handle),
                                      MEM_HANDLE_TYPE_FABRIC, 0, &info.shareHandle);
    if (ret != 0) {
        BM_LOG_ERROR("create shm memory key failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    uint64_t shareable = 0U;
    uint32_t sId;
    ret = DlHalApi::HalMemTransShareableHandle(MEM_HANDLE_TYPE_FABRIC, &info.shareHandle, &sId, &shareable);
    BM_VALIDATE_RETURN(ret == BM_OK, "HalMemTransShareableHandle failed:" << ret, BM_ERROR);

    struct ShareHandleAttr attr = {};
    attr.enableFlag = SHR_HANDLE_NO_WLIST_ENABLE;
    ret = DlHalApi::HalMemShareHandleSetAttribute(shareable, SHR_HANDLE_ATTR_NO_WLIST_IN_SERVER, attr);
    BM_VALIDATE_RETURN(ret == BM_OK, "HalMemShareHandleSetAttribute failed:" << ret, BM_ERROR);

    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(GetDeviceInfo(), "get device info failed.");
    info.devId = options_.devId;
    info.magic = (options_.segType == HYBM_MST_DRAM) ? DRAM_SLICE_EXPORT_INFO_MAGIC : HBM_SLICE_EXPORT_INFO_MAGIC;
    info.version = EXPORT_INFO_VERSION;
    info.mappingOffset =
            slice->vAddress_ - (uint64_t) (ptrdiff_t) (globalVirtualAddress_ + options_.size * options_.rankId);
    info.sliceIndex = static_cast<uint32_t>(slice->index_);
    info.rankId = options_.rankId;
    info.size = slice->size_;
    info.sdid = sdid_;
    info.serverId = serverId_;
    info.superPodId = superPodId_;
    ret = LiteralExInfoTranslater<HostSdmaExportInfo>{}.Serialize(info, exInfo);
    if (ret != BM_OK) {
        BM_LOG_ERROR("export info failed: " << ret);
        return BM_ERROR;
    }

    exportMap_[slice->index_] = exInfo;
    sHandle = info.shareHandle;
    return BM_OK;
}

Result HybmVmmBasedSegment::Export(const std::shared_ptr<MemSlice> &slice, std::string &exInfo) noexcept
{
    BM_ASSERT_RETURN(slice != nullptr, BM_INVALID_PARAM);
    if (!options_.shared) {
        BM_LOG_INFO("hybm_gvm_get_key requires shared memory, skip");
        return BM_OK;
    }
    auto pos = slices_.find(slice->index_);
    BM_VALIDATE_RETURN(pos != slices_.end(), "input slice(idx:" << slice->index_ << ") not exist.", BM_INVALID_PARAM);
    BM_VALIDATE_RETURN(pos->second.slice == slice,
                       "input slice(magic:" << std::hex << slice->magic_ << ") not match.", BM_INVALID_PARAM);

    auto exp = exportMap_.find(slice->index_);
    if (exp != exportMap_.end()) {
        exInfo = exp->second;
        return BM_OK;
    } else {
        return BM_ERROR;
    }
}

Result HybmVmmBasedSegment::GetExportSliceSize(size_t &size) noexcept
{
    size = sizeof(HostSdmaExportInfo);
    return BM_OK;
}

Result HybmVmmBasedSegment::Import(const std::vector<std::string> &allExInfo, void *addresses[]) noexcept
{
    if (!options_.shared) {
        BM_LOG_INFO("import requires shared memory, skip");
        return BM_OK;
    }
    LiteralExInfoTranslater<HostSdmaExportInfo> translator;
    uint64_t exportMagic = (options_.segType == HYBM_MST_DRAM) ? DRAM_SLICE_EXPORT_INFO_MAGIC :
                                                                 HBM_SLICE_EXPORT_INFO_MAGIC;
    std::vector<HostSdmaExportInfo> deserializedInfos{allExInfo.size()};
    for (auto i = 0U; i < allExInfo.size(); i++) {
        auto ret = translator.Deserialize(allExInfo[i], deserializedInfos[i]);
        if (ret != 0) {
            BM_LOG_ERROR("deserialize imported info(" << i << ") failed.");
            return BM_INVALID_PARAM;
        }

        if (deserializedInfos[i].magic != exportMagic) {
            BM_LOG_ERROR("import info(" << i << ") magic(" << deserializedInfos[i].magic << ") invalid.");
            return BM_INVALID_PARAM;
        }
        if (options_.segType == HYBM_MST_HBM && deserializedInfos[i].rankId != options_.rankId &&
            CanLocalHostReaches(deserializedInfos[i].superPodId, deserializedInfos[i].serverId,
                                deserializedInfos[i].devId)) {
            ret = DlAclApi::RtEnableP2P(deviceId_, deserializedInfos[i].devId, 0);
            if (ret != 0) {
                BM_LOG_ERROR("enable device access failed:"
                                     << ret << " local_device:" << deviceId_ << " remote_device:"
                                     << (int) deserializedInfos[i].devId);
                return BM_DL_FUNCTION_FAILED;
            }
        }
    }

    try {
        std::copy(deserializedInfos.begin(), deserializedInfos.end(), std::back_inserter(imports_));
    } catch (...) {
        BM_LOG_ERROR("copy failed.");
        return BM_MALLOC_FAILED;
    }
    return BM_OK;
}

Result HybmVmmBasedSegment::Mmap() noexcept
{
    if (!options_.shared) {
        BM_LOG_INFO("map requires shared memory, skip");
        return BM_OK;
    }

    if (imports_.empty()) {
        return BM_OK;
    }
    for (auto &im: imports_) {
        if (im.rankId == options_.rankId) {
            continue;
        }

        auto remoteAddress =
                reinterpret_cast<uint64_t>(globalVirtualAddress_ + options_.size * im.rankId + im.mappingOffset);
        if (mappedMem_.find(remoteAddress) != mappedMem_.end()) {
            BM_LOG_INFO("remote slice on rank(" << im.rankId << ") has maped: " << (void *) remoteAddress);
            continue;
        }

        BM_LOG_DEBUG("remote slice on rank(" << im.rankId << ") should map to: " << (void *) remoteAddress
                                             << ", size = " << im.size);
        drv_mem_handle_t *handle = nullptr;
        auto ret = DlHalApi::HalMemImport(MEM_HANDLE_TYPE_FABRIC, &im.shareHandle,
                                          static_cast<uint32_t>(options_.devId), &handle);
        BM_VALIDATE_RETURN(ret == BM_OK, "HalMemImport memory failed:" << ret
            << " local sdid:" << sdid_ << " remote ssid:" << im.sdid, BM_ERROR);

        ret = DlHalApi::HalMemMap(reinterpret_cast<void *>(remoteAddress), im.size, 0, handle, 0);
        if (ret != BM_OK) {
            BM_LOG_ERROR("HalMemMap memory failed:" << ret);
            DlHalApi::HalMemRelease(handle);
            return BM_ERROR;
        }
        mappedMem_.emplace(remoteAddress, handle);
    }
    imports_.clear();
    return BM_OK;
}

Result HybmVmmBasedSegment::Unmap() noexcept
{
    if (!options_.shared) {
        BM_LOG_INFO("unmap requires shared memory, skip");
        return BM_OK;
    }

    for (auto pd: mappedMem_) {
        DlHalApi::HalMemUnmap(reinterpret_cast<void *>(pd.first));
        DlHalApi::HalMemRelease(pd.second);
    }
    mappedMem_.clear();
    return BM_OK;
}

Result HybmVmmBasedSegment::RemoveImported(const std::vector<uint32_t> &ranks) noexcept
{
    if (!options_.shared) {
        BM_LOG_INFO("remove requires shared memory, skip");
        return BM_OK;
    }
    for (auto &rank: ranks) {
        if (rank >= options_.rankCnt) {
            BM_LOG_ERROR("input rank is invalid! rank:" << rank << " rankSize:" << options_.rankCnt);
            return BM_INVALID_PARAM;
        }
    }

    for (auto &rank: ranks) {
        uint64_t addr = reinterpret_cast<uint64_t>(globalVirtualAddress_) + options_.size * rank;
        auto it = mappedMem_.lower_bound(addr);
        auto st = it;
        while (it != mappedMem_.end() && (*it).first < addr + options_.size) {
            DlHalApi::HalMemUnmap(reinterpret_cast<void *>((*it).first));
            DlHalApi::HalMemRelease((*it).second);
            it++;
        }

        if (st != it) {
            mappedMem_.erase(st, it);
        }
    }
    return BM_OK;
}

std::shared_ptr<MemSlice> HybmVmmBasedSegment::GetMemSlice(hybm_mem_slice_t slice) const noexcept
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

bool HybmVmmBasedSegment::MemoryInRange(const void *begin, uint64_t size) const noexcept
{
    if (begin < globalVirtualAddress_) {
        return false;
    }

    if (reinterpret_cast<const uint8_t *>(begin) + size > globalVirtualAddress_ + totalVirtualSize_) {
        return false;
    }

    return true;
}

void HybmVmmBasedSegment::GetRankIdByAddr(const void *addr, uint64_t size, uint32_t &rankId) const noexcept
{
    if (!MemoryInRange(addr, size)) {
        rankId = options_.rankId;
    } else {
        uint64_t offset = static_cast<const uint8_t *>(addr) - static_cast<const uint8_t *>(globalVirtualAddress_);
        rankId = offset / options_.size;
    }
}


bool HybmVmmBasedSegment::CheckSmdaReaches(uint32_t rankId) const noexcept
{
    return true;
}

Result HybmVmmBasedSegment::GetDeviceInfo() noexcept
{
    if (options_.devId < 0) {
        return BM_INVALID_PARAM;
    }

    if (InitDeviceInfo() != BM_OK) {
        return BM_ERROR;
    }
    return BM_OK;
}
#else
Result HybmVmmBasedSegment::ValidateOptions() noexcept
{
    if (options_.segType != HYBM_MST_DRAM || options_.size == 0 || (options_.size % HYBM_LARGE_PAGE_SIZE) != 0) {
        BM_LOG_ERROR("Invalid options segType:" << options_.segType << " size:" << options_.size);
        return BM_INVALID_PARAM;
    }

    if (UINT64_MAX / options_.size < options_.rankCnt) {
        BM_LOG_ERROR("Validate options error rankCnt(" << options_.rankCnt << ") size(" << options_.size);
        return BM_INVALID_PARAM;
    }

    return BM_OK;
}

Result HybmVmmBasedSegment::ReserveMemorySpace(void **address) noexcept
{
    BM_ASSERT_RETURN(address != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_LOG_AND_RETURN(ValidateOptions() == BM_OK, "Failed to validate options.", BM_INVALID_PARAM);
    if (globalVirtualAddress_ != nullptr) {
        BM_LOG_ERROR("already prepare virtual memory.");
        return BM_ERROR;
    }

    if (options_.rankId >= options_.rankCnt) {
        BM_LOG_ERROR("rank(" << options_.rankId << ") but total " << options_.rankCnt);
        return BM_INVALID_PARAM;
    }

    uint64_t base = 0;
    totalVirtualSize_ = options_.rankCnt * options_.size;
    auto ret = hybm_gvm_reserve_memory(&base, totalVirtualSize_, options_.shared);
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

Result HybmVmmBasedSegment::AllocLocalMemory(uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept
{
    if ((size % HYBM_LARGE_PAGE_SIZE) != 0UL || size + allocatedSize_ > options_.size) {
        BM_LOG_ERROR("invalid allocate memory size : " << size << ", now used " << allocatedSize_ << " of "
                                                       << options_.size);
        return BM_INVALID_PARAM;
    }

    BM_ASSERT_RETURN(globalVirtualAddress_ != nullptr, BM_NOT_INITIALIZED);

    auto localVirtualBase = globalVirtualAddress_ + options_.size * options_.rankId;
    uint64_t allocAddr = reinterpret_cast<uint64_t>(localVirtualBase + allocatedSize_);
    auto ret = hybm_gvm_mem_alloc(allocAddr, size);
    if (ret != BM_OK) {
        BM_LOG_ERROR("HalGvaAlloc memory failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    allocatedSize_ += size;
    slice = std::make_shared<MemSlice>(sliceCount_++, MEM_TYPE_HOST_DRAM, MEM_PT_TYPE_GVM, allocAddr, size);
    BM_ASSERT_RETURN(slice != nullptr, BM_MALLOC_FAILED);
    slices_.emplace(slice->index_, slice);
    BM_LOG_DEBUG("allocate slice(idx:" << slice->index_ << ", size:" << slice->size_ << ", address:0x" << std::hex
                                       << slice->vAddress_ << ").");

    return BM_OK;
}

Result HybmVmmBasedSegment::ReleaseSliceMemory(const std::shared_ptr<MemSlice> &slice) noexcept
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

    auto res = hybm_gvm_mem_free(slice->vAddress_);
    BM_LOG_INFO("free slice(idx:" << slice->index_ << "), size: " << slice->size_ << " return:" << res);

    slices_.erase(pos);
    return BM_OK;
}

Result HybmVmmBasedSegment::Export(std::string &exInfo) noexcept
{
    return BM_OK;
}

Result HybmVmmBasedSegment::Export(const std::shared_ptr<MemSlice> &slice, std::string &exInfo) noexcept
{
    BM_ASSERT_RETURN(slice != nullptr, BM_INVALID_PARAM);
    if (!options_.shared) {
        BM_LOG_INFO("hybm_gvm_get_key requires shared memory, skip");
        return BM_OK;
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
    if (exp != exportMap_.end()) {
        exInfo = exp->second;
        return BM_OK;
    }

    HostSdmaExportInfo info;
    auto ret = hybm_gvm_get_key((uint64_t)slice->vAddress_, &info.shmKey);
    if (ret != 0) {
        BM_LOG_ERROR("create shm memory key failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(GetDeviceInfo(), "get device info failed.");
    info.version = EXPORT_INFO_VERSION;
    info.mappingOffset =
        slice->vAddress_ - (uint64_t)(ptrdiff_t)(globalVirtualAddress_ + options_.size * options_.rankId);
    info.sliceIndex = static_cast<uint32_t>(slice->index_);
    info.rankId = options_.rankId;
    info.size = slice->size_;
    info.sdid = sdid_;
    ret = LiteralExInfoTranslater<HostSdmaExportInfo>{}.Serialize(info, exInfo);
    if (ret != BM_OK) {
        BM_LOG_ERROR("export info failed: " << ret);
        return BM_ERROR;
    }

    exportMap_[slice->index_] = exInfo;
    return BM_OK;
}

Result HybmVmmBasedSegment::Import(const std::vector<std::string> &allExInfo, void *addresses[]) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(options_.shared, "hybm_gvm_set_whitelist requires shared memory", BM_OK);
    LiteralExInfoTranslater<HostSdmaExportInfo> translator;
    std::vector<HostSdmaExportInfo> deserializedInfos{allExInfo.size()};
    for (auto i = 0U; i < allExInfo.size(); i++) {
        auto ret = translator.Deserialize(allExInfo[i], deserializedInfos[i]);
        if (ret != 0) {
            BM_LOG_ERROR("deserialize imported info(" << i << ") failed.");
            return BM_INVALID_PARAM;
        }
    }
    uint32_t localIdx = UINT32_MAX;
    for (auto i = 0U; i < deserializedInfos.size(); i++) {
        if (deserializedInfos[i].magic != DRAM_SLICE_EXPORT_INFO_MAGIC) {
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
        auto ret = hybm_gvm_set_whitelist(deserializedInfos[localIdx].shmKey, deserializedInfos[i].sdid);
        if (ret != 0) {
            BM_LOG_ERROR("enable white list for rank("
                         << deserializedInfos[i].rankId << ") failed: " << ret << ", local rank = " << options_.rankId
                         << ", shmkey=" << std::hex << deserializedInfos[localIdx].shmKey);
            return BM_DL_FUNCTION_FAILED;
        }
    }
    try {
        std::copy(deserializedInfos.begin(), deserializedInfos.end(), std::back_inserter(imports_));
    } catch (...) {
        BM_LOG_ERROR("copy failed.");
        return BM_MALLOC_FAILED;
    }
    return BM_OK;
}

Result HybmVmmBasedSegment::Mmap() noexcept
{
    if (!options_.shared) {
        BM_LOG_INFO("hybm_gvm_mem_open requires shared memory, skip");
        return BM_OK;
    }

    if (imports_.empty()) {
        return BM_OK;
    }
    for (auto &im : imports_) {
        if (im.rankId == options_.rankId) {
            continue;
        }

        auto remoteAddress =
            reinterpret_cast<uint64_t>(globalVirtualAddress_ + options_.size * im.rankId + im.mappingOffset);
        if (mappedMem_.find(remoteAddress) != mappedMem_.end()) {
            BM_LOG_INFO("remote slice on rank(" << im.rankId << ") has maped: " << (void *)remoteAddress);
            continue;
        }

        BM_LOG_DEBUG("remote slice on rank(" << im.rankId << ") should map to: " << (void *)remoteAddress
                                             << ", size = " << im.size);
        auto ret = hybm_gvm_mem_open(remoteAddress, im.shmKey);
        if (ret != BM_OK) {
            BM_LOG_ERROR("HalGvaOpen memory failed:" << ret);
            return -1;
        }
        mappedMem_.insert((uint64_t)remoteAddress);
    }
    imports_.clear();
    return BM_OK;
}

Result HybmVmmBasedSegment::Unmap() noexcept
{
    BM_ASSERT_LOG_AND_RETURN(options_.shared, "hybm_gvm_mem_close requires shared memory", BM_OK);
    for (auto va : mappedMem_) {
        hybm_gvm_mem_close(va);
    }
    mappedMem_.clear();
    return BM_OK;
}

Result HybmVmmBasedSegment::RemoveImported(const std::vector<uint32_t> &ranks) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(options_.shared, "hybm_gvm_mem_close requires shared memory", BM_OK);
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
            hybm_gvm_mem_close(*it);
            it++;
        }

        if (st != it) {
            mappedMem_.erase(st, it);
        }
    }
    return BM_OK;
}

std::shared_ptr<MemSlice> HybmVmmBasedSegment::GetMemSlice(hybm_mem_slice_t slice) const noexcept
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

bool HybmVmmBasedSegment::MemoryInRange(const void *begin, uint64_t size) const noexcept
{
    if (begin < globalVirtualAddress_) {
        return false;
    }

    if (reinterpret_cast<const uint8_t *>(begin) + size > globalVirtualAddress_ + totalVirtualSize_) {
        return false;
    }

    return true;
}
#endif