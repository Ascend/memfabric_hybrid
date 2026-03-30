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
#include <cstdint>
#include <bitset>

#include "hybm_ex_info_transfer.h"
#include "hybm_vmm_based_segment.h"
#include "dl_acl_api.h"
#include "hybm_types.h"
#include "hybm_va_manager.h"
#include "mf_num_util.h"

using namespace ock::mf;

HybmVmmBasedSegment::~HybmVmmBasedSegment()
{
    if (globalVirtualAddress_ == nullptr && localVirtualAddress_ == nullptr && slices_.empty()) {
        return;
    }

    auto ret = UnReserveMemorySpace();
    if (ret != BM_OK) {
        BM_LOG_WARN("Destructor cleanup failed, ret:" << ret
                                                      << " gva:" << reinterpret_cast<void *>(globalVirtualAddress_)
                                                      << " lva:" << reinterpret_cast<void *>(localVirtualAddress_));
    }
}

Result HybmVmmBasedSegment::ValidateOptions() noexcept
{
    auto checkAlignment = [&](uint64_t size, uint64_t align) -> bool {
        if (size == 0 || (align != 0 && size % align) != 0) {
            return false;
        }
        return true;
    };
    uint64_t align = (options_.segType == HYBM_MST_DRAM) ? GB : HYBM_LARGE_PAGE_SIZE;
    if (options_.size != 0 && !checkAlignment(options_.size, align)) {
        BM_LOG_ERROR("Invalid options segType:" << options_.segType << " size:"
                                                << options_.size << " must algin:" << align);
        return BM_INVALID_PARAM;
    }
    if (!checkAlignment(options_.maxSize, GB)) {
        BM_LOG_ERROR("Invalid options segType:" << options_.segType << " max size:"
                                                << options_.maxSize << " must align GB");
        return BM_INVALID_PARAM;
    }

    if (UINT64_MAX / options_.maxSize < options_.rankCnt) {
        BM_LOG_ERROR("Validate options error rankCnt(" << options_.rankCnt << ") size(" << options_.maxSize);
        return BM_INVALID_PARAM;
    }

    // check memory pool size upper limit 128TB for 910C
    if (options_.maxSize * options_.rankCnt > HYBM_GVM_MAX_POOL_SIZE) {
        BM_LOG_ERROR("Memory pool size > 128T. maxSize:" << options_.maxSize << ", rankCnt:" << options_.rankCnt);
        return BM_INVALID_PARAM;
    }
    return BM_OK;
}

Result HybmVmmBasedSegment::ReserveMemorySpace(void **address) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(ValidateOptions() == BM_OK, "Failed to validate options.", BM_INVALID_PARAM);
    BM_ASSERT_LOG_AND_RETURN(globalVirtualAddress_ == nullptr, "Already prepare virtual memory.", BM_NOT_INITIALIZED);
    BM_ASSERT_RETURN(address != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_LOG_AND_RETURN(options_.rankId < options_.rankCnt,
                             "rank(" << options_.rankId << ") but total " << options_.rankCnt, BM_INVALID_PARAM);

    void *base = nullptr;
    totalVirtualSize_ = options_.rankCnt * options_.maxSize;
    auto mem_type = options_.segType == HYBM_MST_HBM ? HYBM_MEM_TYPE_DEVICE : HYBM_MEM_TYPE_HOST;
    auto gvaInfo = HybmVaManager::GetInstance().AllocReserveGva(options_.rankId, totalVirtualSize_, totalVirtualSize_,
                                                                mem_type, options_.isSecondMapping);
    BM_ASSERT_RETURN(gvaInfo.va[HVM_GVA] > 0, BM_ERROR);
    globalVirtualAddress_ = (uint8_t *)reinterpret_cast<void *>(gvaInfo.va[HVM_GVA]);

    uint64_t flag = MEM_RSV_TYPE_REMOTE_MAP;
    auto ret = DlHalApi::HalMemAddressReserve(&base, totalVirtualSize_, 0,
                                              reinterpret_cast<void *>(gvaInfo.va[HVM_DVA]), flag);
    if (ret != 0 || base != reinterpret_cast<void *>(gvaInfo.va[HVM_DVA])) {
        BM_LOG_ERROR("prepare virtual memory size(" << totalVirtualSize_ << ") failed. ret: " << ret);
        return BM_MALLOC_FAILED;
    }
    localVirtualAddress_ = (uint8_t *)reinterpret_cast<void *>(gvaInfo.va[HVM_DVA]);
    allocatedSize_ = 0UL;
    sliceCount_ = 0;
    *address = globalVirtualAddress_;
    return BM_OK;
}

Result HybmVmmBasedSegment::UnReserveMemorySpace() noexcept
{
    BM_LOG_INFO("UnReserveMemorySpace gva:" << reinterpret_cast<void *>(globalVirtualAddress_)
                                            << ", lva:" << reinterpret_cast<void *>(localVirtualAddress_));
    for (auto &it : mappedGvaMem_) {
        DlHalApi::HalMemUnmap(reinterpret_cast<void *>(it.first));
        DlHalApi::HalMemRelease(it.second);
        HybmVaManager::GetInstance().RemoveOneVaInfo(it.first);
    }
    mappedGvaMem_.clear();

    while (!slices_.empty()) {
        auto slice = slices_.begin()->second.slice;
        auto ret = ReleaseSliceMemory(slice);
        if (ret != BM_OK) {
            BM_LOG_WARN("ReleaseSliceMemory failed during unreserve, ret:" << ret << " slice:" << slice);
            return ret;
        }
    }
    while (!registerSlices_.empty()) {
        auto slice = registerSlices_.begin()->second.first.slice;
        ReleaseSliceMemory(slice);
    }

    if (localVirtualAddress_ != nullptr) {
        auto ret = DlHalApi::HalMemAddressFree(reinterpret_cast<void *>(localVirtualAddress_));
        BM_LOG_INFO("free reserved address lva:" << reinterpret_cast<void *>(localVirtualAddress_)
                                                 << " return:" << ret);
        if (ret != BM_OK) {
            BM_LOG_WARN("HalMemAddressFree failed, keep reserved VA state. ret:"
                        << ret << " gva:" << reinterpret_cast<void *>(globalVirtualAddress_)
                        << " lva:" << reinterpret_cast<void *>(localVirtualAddress_));
            return BM_DL_FUNCTION_FAILED;
        }
        localVirtualAddress_ = nullptr;
    }

    if (globalVirtualAddress_ != nullptr) {
        HybmVaManager::GetInstance().FreeReserveGva((uintptr_t)globalVirtualAddress_);
        globalVirtualAddress_ = nullptr;
    }
    totalVirtualSize_ = 0UL;
    allocatedSize_ = 0UL;
    return BM_OK;
}

Result HybmVmmBasedSegment::HalMemCreateAdapterFromHost(size_t size, drv_mem_handle_t **handle, drv_mem_prop prop)
{
    Result ret = BM_ERROR;
    uint32_t performance =
        NumUtil::ExtractBits(options_.flags, HYBM_PERFORMANCE_MODE_FLAG_INDEX, HYBM_PERFORMANCE_MODE_FLAG_LEN);
    if (DlAclApi::GetAscendSocType() == AscendSocType::ASCEND_950) {
        prop = {MEM_HOST_SIDE, 0, 0, MEM_HUGE_PAGE_TYPE, MEM_DDR_TYPE, 0};
        auto start = std::chrono::high_resolution_clock::now();
        ret = DlHalApi::HalMemCreate(handle, size, &prop, 0);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        BM_LOG_INFO("Try HalMemCreate ret:" << ret << " dev:" << prop.devid << " spend time:" << duration.count()
                                            << " size:" << size);
    } else if (performance != UINT32_MAX && performance != 0) {
        uint32_t numaIndex = NumUtil::ExtractBits(options_.flags, HYBM_BIND_NUMA_FLAG_INDEX, HYBM_BIND_NUMA_FLAG_LEN);
        if (numaIndex == UINT32_MAX) {
            BM_LOG_ERROR("Failed to get numa from flag:" << (std::bitset<UINT32_WIDTH>(options_.flags))
                                                         << " start index:" << HYBM_BIND_NUMA_FLAG_INDEX
                                                         << " flag len:" << HYBM_BIND_NUMA_FLAG_LEN);
            return BM_INVALID_PARAM;
        }
        if (numaIndex == HYBM_BIND_NUMA_AUTO_AFFINITY_FLAG) {
            prop.side = MEM_HOST_SIDE;
            prop.devid = 0;
        } else {
            prop.devid = numaIndex;
        }
        auto start = std::chrono::high_resolution_clock::now();
        ret = DlHalApi::HalMemCreate(handle, size, &prop, 0);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        BM_LOG_INFO("Try HalMemCreate ret:" << ret << " numa:" << prop.devid << " spend time:" << duration.count()
                                            << " size:" << size);
    } else {
        prop.devid = -1;
        auto start = std::chrono::high_resolution_clock::now();
        ret = DlHalApi::HalMemCreate(handle, size, &prop, 0);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        BM_LOG_INFO("Try HalMemCreate ret:" << ret << " numa:" << prop.devid << " spend time:" << duration.count()
                                            << " size:" << size);
    }
    return ret;
}

Result HybmVmmBasedSegment::MallocFromHost(size_t size, uint32_t devId, drv_mem_handle_t **handle) noexcept
{
    drv_mem_prop prop{};
    prop = {MEM_HOST_NUMA_SIDE, devId, 0, MEM_GIANT_PAGE_TYPE, MEM_P2P_DDR_TYPE, 0};
    size_t granularity = 0;
    if (DlAclApi::GetAscendSocType() == AscendSocType::ASCEND_950 || // A5当前仅支持huge page
        DlHalApi::HalMemGetAllocationGranularity(&prop, MEM_ALLOC_GRANULARITY_RECOMMENDED, &granularity) != 0) {
        prop.pg_type = MEM_HUGE_PAGE_TYPE;
        BM_LOG_WARN("Not support giant page size change use huge page, memType:" << prop.mem_type);
    }
    int32_t ret = HalMemCreateAdapterFromHost(size, handle, prop);
    if (ret == HAL_OUT_OF_MEMORY_ERROR && prop.pg_type == MEM_GIANT_PAGE_TYPE) {
        BM_LOG_INFO("Try HalMemCreate use 1GB page failed, ret:" << ret << ", than try 2MB page");
        prop.pg_type = MEM_HUGE_PAGE_TYPE;
        ret = HalMemCreateAdapterFromHost(size, handle, prop);
    }
    if (ret != BM_OK) {
        BM_LOG_ERROR("Try HalMemCreate failed ret:" << ret << " numa:" << prop.devid << " spend time:"
                                                    << " size:" << size);
    }
    return ret;
}

Result HybmVmmBasedSegment::MallocFromDevice(size_t size, uint32_t devId, drv_mem_handle_t **handle) noexcept
{
    drv_mem_prop prop{};
    prop = {MEM_DEV_SIDE, static_cast<uint32_t>(devId), 0, MEM_HUGE_PAGE_TYPE, MEM_HBM_TYPE, 0};
    return DlHalApi::HalMemCreate(handle, size, &prop, 0);
}

Result HybmVmmBasedSegment::MallocEmptySlice(MemSlicePtr &slice) noexcept
{
    auto localVirtualBase = localVirtualAddress_ + options_.maxSize * options_.rankId;
    auto globalVirtualBase = globalVirtualAddress_ + options_.maxSize * options_.rankId;
    auto allocAddr = reinterpret_cast<uint64_t>(localVirtualBase + allocatedSize_);
    auto gva = reinterpret_cast<uint64_t>(globalVirtualBase + allocatedSize_);
    auto memType = options_.segType == HYBM_MST_HBM ? HYBM_MEM_TYPE_DEVICE : HYBM_MEM_TYPE_HOST;
    slice = std::make_shared<MemSlice>(sliceCount_++, memType, MEM_PT_TYPE_GVM, gva, allocAddr, 0);
    slices_.emplace(slice->index_, MemSliceStatus(slice, nullptr));
    if (!options_.shared) {
        BM_LOG_INFO("no need to share, skip export");
        return BM_OK;
    }
    HostSdmaExportInfo info;
    std::string exInfo;
    info.logicDevId = logicDeviceId_;
    info.magic = (options_.segType == HYBM_MST_DRAM) ? VMM_BASE_DRAM_SLICE_EXPORT_INFO_MAGIC
                                                     : VMM_BASE_HBM_SLICE_EXPORT_INFO_MAGIC;
    info.version = EXPORT_INFO_VERSION;
    info.gva = slice->vAddress_;
    info.deviceVa = slice->vAddress_;
    info.sliceIndex = static_cast<uint32_t>(slice->index_);
    info.rankId = options_.rankId;
    info.size = slice->size_;
    info.sdid = sdid_;
    info.serverId = serverId_;
    info.superPodId = superPodId_;
    auto ret = LiteralExInfoTranslater<HostSdmaExportInfo>{}.Serialize(info, exInfo);
    if (ret != BM_OK) {
        BM_LOG_ERROR("export info failed: " << ret);
        return BM_ERROR;
    }
    BM_LOG_INFO("Success to export vmm segment info rank:"
                << info.rankId << " superPodId:" << info.superPodId << " serverId:" << info.serverId
                << " devId:" << info.logicDevId << " segType:" << options_.segType << " size:" << info.size);
    exportMap_[slice->index_] = exInfo;
    return BM_OK;
}

Result HybmVmmBasedSegment::AllocLocalMemory(uint64_t size, MemSlicePtr &slice) noexcept
{
    if ((size % HYBM_LARGE_PAGE_SIZE) != 0UL || size + allocatedSize_ > options_.maxSize) {
        BM_LOG_ERROR("invalid allocate memory size : " << size << ", now used " << allocatedSize_ << " of "
                                                       << options_.maxSize);
        return BM_INVALID_PARAM;
    }

    BM_ASSERT_RETURN(localVirtualAddress_ != nullptr, BM_NOT_INITIALIZED);
    if (size == 0) {
        return MallocEmptySlice(slice);
    }
    auto localVirtualBase = localVirtualAddress_ + options_.maxSize * options_.rankId;
    auto globalVirtualBase = globalVirtualAddress_ + options_.maxSize * options_.rankId;
    auto allocAddr = reinterpret_cast<uint64_t>(localVirtualBase + allocatedSize_);
    auto gva = reinterpret_cast<uint64_t>(globalVirtualBase + allocatedSize_);
    drv_mem_handle_t *handle = nullptr;
    Result ret = BM_OK;
    switch (options_.segType) {
        case HYBM_MST_HBM:
            ret = MallocFromDevice(size, logicDeviceId_, &handle);
            break;
        case HYBM_MST_DRAM:
            ret = MallocFromHost(size, logicDeviceId_, &handle);
            break;
        default:
            BM_LOG_ERROR("invalid seg type: " << options_.segType);
            return BM_INVALID_PARAM;
    }
    BM_VALIDATE_RETURN(ret == BM_OK,
                       "HalMemCreate failed: " << ret << " segType:" << options_.segType << " devId:" << logicDeviceId_
                                               << " size:" << size,
                       BM_DL_FUNCTION_FAILED);

    allocatedSize_ += size;
    auto memType = options_.segType == HYBM_MST_HBM ? HYBM_MEM_TYPE_DEVICE : HYBM_MEM_TYPE_HOST;
    slice = std::make_shared<MemSlice>(sliceCount_++, memType, MEM_PT_TYPE_GVM, gva, allocAddr, size);
    BM_ASSERT_RETURN(slice != nullptr, BM_MALLOC_FAILED);
    slices_.emplace(slice->index_, MemSliceStatus(slice, reinterpret_cast<void *>(handle)));
    auto type = options_.segType == HYBM_MST_HBM ? HYBM_MEM_TYPE_DEVICE : HYBM_MEM_TYPE_HOST;
    ret =
        HybmVaManager::GetInstance().AddVaInfo({gva, slice->vAddress_, slice->vAddress_, size, type}, options_.rankId);
    if (ret != 0) {
        BM_LOG_ERROR("AddVaInfo failed, size: " << size << " ret: " << ret);
        DlHalApi::HalMemRelease(handle);
        slices_.erase(slice->index_);
        return ret;
    }
    MemShareHandle sHandle = {};
    ret = ExportInner(slice, sHandle);
    if (ret != BM_OK) {
        BM_LOG_ERROR("export failed: " << ret);
        DlHalApi::HalMemRelease(handle);
        slices_.erase(slice->index_);
        return BM_ERROR;
    }

    drv_mem_handle_t *dHandle = handle;
    if (options_.segType == HYBM_MST_DRAM && options_.shared && (options_.flags & HYBM_FLAG_DRAM_MAP_HOST_VA) == 0) {
        ret = DlHalApi::HalMemImport(MEM_HANDLE_TYPE_FABRIC, &sHandle, logicDeviceId_, &dHandle);
        BM_VALIDATE_RETURN(ret == BM_OK, "HalMemImport memory failed:" << ret, BM_ERROR);
    }
    ret = DlHalApi::HalMemMap(reinterpret_cast<void *>(allocAddr), size, 0, dHandle, 0);
    if (ret != BM_OK) {
        BM_LOG_ERROR("HalMemMap failed: " << ret);
        DlHalApi::HalMemRelease(dHandle);
        slices_.erase(slice->index_);
        return BM_DL_FUNCTION_FAILED;
    }
    if (options_.segType == HYBM_MST_DRAM && options_.shared && (options_.flags & HYBM_FLAG_DRAM_MAP_HOST_VA) != 0) {
        struct drv_mem_access_desc desc[1];
        desc[0].location.side = MEM_DEV_SIDE;
        desc[0].location.id = logicDeviceId_;
        desc[0].type = MEM_ACCESS_TYPE_READWRITE;
        ret = DlHalApi::HalMemSetAccess(reinterpret_cast<void *>(allocAddr), size, desc, 1);
        if (ret != BM_OK) {
            BM_LOG_ERROR("HalMemSetAccess failed: " << ret);
            DlHalApi::HalMemRelease(dHandle);
            slices_.erase(slice->index_);
            return BM_DL_FUNCTION_FAILED;
        }
    }
    BM_LOG_INFO("alloc mem success, type:" << memType << " addr:" << VaToInfo(allocAddr) << " size:" << size);
    return BM_OK;
}

Result HybmVmmBasedSegment::RegisterMemory(const void *addr, uint64_t size, MemSlicePtr &slice) noexcept
{
    auto ret = RegisterMemCommon(addr, size, slice);
    BM_ASSERT_RETURN(ret == BM_OK, ret);

    registerSlices_.emplace(slice->index_, std::make_pair(slice, reinterpret_cast<uint64_t>(addr)));
    BM_LOG_INFO("HybmVmmBasedSegment: RegisterMemory success, size: " << size << " addr: " << VaToInfo(addr));
    return BM_OK;
}

Result HybmVmmBasedSegment::ReleaseSliceMemory(const MemSlicePtr &slice) noexcept
{
    BM_VALIDATE_RETURN(slice != nullptr, "input slice is nullptr", BM_INVALID_PARAM);

    auto pos = slices_.find(slice->index_);
    if (pos != slices_.end()) {
        if (pos->second.slice != slice) {
            BM_LOG_ERROR("input slice(magic:" << std::hex << slice->magic_ << ") not match.");
            return BM_INVALID_PARAM;
        }
        auto res = DlHalApi::HalMemUnmap(reinterpret_cast<void *>(slice->vAddress_));
        BM_LOG_INFO("unmap slice(idx:" << slice->index_ << "), size: " << slice->size_ << " return:" << res);

        res = DlHalApi::HalMemRelease(reinterpret_cast<drv_mem_handle_t *>(pos->second.handle));
        BM_LOG_INFO("release slice(idx:" << slice->index_ << "), size: " << slice->size_ << " return:" << res);
        HybmVaManager::GetInstance().RemoveOneVaInfo(slice->vAddress_, HVM_DVA);

        slices_.erase(pos);
        return BM_OK;
    }
    auto registerPos = registerSlices_.find(slice->index_);
    if (registerPos != registerSlices_.end()) {
        if (registerPos->second.first.slice != slice) {
            BM_LOG_ERROR("input slice(magic:" << std::hex << slice->magic_ << ") not match.");
            return BM_INVALID_PARAM;
        }
        uint64_t realAddr = registerPos->second.second;
        bool isHbm = (realAddr >= HYBM_HBM_START_ADDR && realAddr < HYBM_HBM_END_ADDR);
        if (!isHbm) {
            auto ret =
                DlHalApi::HalHostUnregisterEx(reinterpret_cast<void *>(realAddr), logicDeviceId_, HOST_MEM_MAP_DEV);
            BM_LOG_INFO("unregister slice(idx:" << slice->index_ << "), size: " << slice->size_ << " return:" << ret);
        }
        registerSlices_.erase(registerPos);
        return BM_OK;
    }

    BM_LOG_ERROR("input slice(idx:" << slice->index_ << ") not exist.");
    return BM_INVALID_PARAM;
}

Result HybmVmmBasedSegment::Export(std::string &exInfo) noexcept
{
    return BM_OK;
}

Result HybmVmmBasedSegment::ExportInner(const MemSlicePtr &slice, MemShareHandle &sHandle) noexcept
{
    if (!options_.shared) {
        BM_LOG_INFO("no need to share, skip export");
        return BM_OK;
    }

    auto [gvaInfo, stat] = HybmVaManager::GetInstance().FindAllocByVa(slice->vAddress_, HVM_DVA);
    if (!stat) {
        BM_LOG_ERROR("input device va(" << slice->vAddress_ << ") not match.");
        return BM_INVALID_PARAM;
    }

    HostSdmaExportInfo info;
    std::string exInfo;
    auto pos = slices_.find(slice->index_);
    auto ret = DlHalApi::HalMemExport(reinterpret_cast<drv_mem_handle_t *>(pos->second.handle), MEM_HANDLE_TYPE_FABRIC,
                                      0, &info.shareHandle);
    if (ret != 0) {
        BM_LOG_ERROR("create shm memory key failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    if (socType_ == AscendSocType::ASCEND_910B || socType_ == AscendSocType::ASCEND_910C) {
        uint64_t shareable = 0U;
        uint32_t sId;
        ret = DlHalApi::HalMemTransShareableHandle(MEM_HANDLE_TYPE_FABRIC, &info.shareHandle, &sId, &shareable);
        BM_VALIDATE_RETURN(ret == BM_OK, "HalMemTransShareableHandle failed:" << ret, BM_ERROR);
        struct ShareHandleAttr attr = {.enableFlag = SHR_HANDLE_NO_WLIST_ENABLE, .rsv = {0}};
        ret = DlHalApi::HalMemShareHandleSetAttribute(shareable, SHR_HANDLE_ATTR_NO_WLIST_IN_SERVER, attr);
        BM_VALIDATE_RETURN(ret == BM_OK, "HalMemShareHandleSetAttribute failed:" << ret, BM_ERROR);
    }

    info.logicDevId = logicDeviceId_;
    info.magic = (options_.segType == HYBM_MST_DRAM) ? VMM_BASE_DRAM_SLICE_EXPORT_INFO_MAGIC
                                                     : VMM_BASE_HBM_SLICE_EXPORT_INFO_MAGIC;
    info.version = EXPORT_INFO_VERSION;
    info.gva = gvaInfo.base.va[HVM_GVA];
    info.deviceVa = gvaInfo.base.va[HVM_DVA];
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

    BM_LOG_INFO("Success to export vmm segment info rank:"
                << info.rankId << " superPodId:" << info.superPodId << " serverId:" << info.serverId
                << " devId:" << info.logicDevId << " segType:" << options_.segType << " size:" << info.size);
    exportMap_[slice->index_] = exInfo;
    sHandle = info.shareHandle;
    return BM_OK;
}

Result HybmVmmBasedSegment::Export(const MemSlicePtr &slice, std::string &exInfo) noexcept
{
    BM_ASSERT_RETURN(slice != nullptr, BM_INVALID_PARAM);
    if (!options_.shared) {
        BM_LOG_INFO("no need to share, skip export");
        return BM_OK;
    }
    auto pos = slices_.find(slice->index_);
    BM_VALIDATE_RETURN(pos != slices_.end(), "input slice(idx:" << slice->index_ << ") not exist.", BM_INVALID_PARAM);
    BM_VALIDATE_RETURN(pos->second.slice == slice, "input slice(magic:" << std::hex << slice->magic_ << ") not match.",
                       BM_INVALID_PARAM);

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
        BM_LOG_INFO("no need to share, skip import");
        return BM_OK;
    }
    LiteralExInfoTranslater<HostSdmaExportInfo> translator;
    uint64_t exportMagic = (options_.segType == HYBM_MST_DRAM) ? VMM_BASE_DRAM_SLICE_EXPORT_INFO_MAGIC
                                                               : VMM_BASE_HBM_SLICE_EXPORT_INFO_MAGIC;
    std::vector<HostSdmaExportInfo> deserializedInfos;
    Result ret = BM_ERROR;
    for (auto i = 0U; i < allExInfo.size(); i++) {
        HostSdmaExportInfo info{};
        ret = translator.Deserialize(allExInfo[i], info);
        if (ret != 0) {
            BM_LOG_ERROR("deserialize imported info(" << i << ") failed.");
            return BM_INVALID_PARAM;
        }
        if (info.magic != exportMagic || info.magic == ENTITY_EXPORT_INFO_MAGIC) {
            BM_LOG_WARN("import i(" << i << ") rank(" << info.rankId << ") magic(" << info.magic
                                    << ") invalid skip it.");
            continue;
        }

        if (info.magic != exportMagic) {
            BM_LOG_ERROR("import info(" << i << ") magic(" << info.magic << ") invalid.");
            return BM_INVALID_PARAM;
        }
        if (options_.segType == HYBM_MST_HBM && info.rankId != options_.rankId &&
            logicDeviceId_ != static_cast<int>(info.logicDevId) &&
            CanLocalHostReaches(info.superPodId, info.serverId, info.logicDevId)) {
            ret = DlAclApi::RtEnableP2P(deviceId_, info.logicDevId, 0);
            if (ret != 0) {
                BM_LOG_ERROR("enable device access failed:" << ret << " local_device:" << deviceId_
                                                            << " remote_device:" << (int)info.logicDevId);
                return BM_DL_FUNCTION_FAILED;
            }
        }
        if (addresses != nullptr) {
            addresses[i] = reinterpret_cast<void *>(info.gva);
        }
        if (info.size == 0) {
            continue;
        }
        deserializedInfos.emplace_back(info);
        BM_LOG_INFO("Success to import rank:" << info.rankId << " superPodId:" << info.superPodId
                                              << " serverId:" << info.serverId << " devId:" << info.logicDevId
                                              << " segType:" << options_.segType << " size:" << info.size);
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
        BM_LOG_INFO("no need to share, skip map");
        return BM_OK;
    }

    if (imports_.empty()) {
        return BM_OK;
    }
    for (auto &im : imports_) {
        if (im.rankId == options_.rankId) {
            continue;
        }

        if (im.magic == ENTITY_EXPORT_INFO_MAGIC) {
            continue;
        }

        if (im.segmentType != SEGMENT_TYPE_VMM) {
            BM_LOG_ERROR("mmap failed for invalid segment type in vmm:" << im.segmentType);
            return BM_ERROR;
        }

        if (im.size == 0) {
            BM_LOG_ERROR("mmap failed for invalid size 0");
            return BM_ERROR;
        }

        if (mappedGvaMem_.find(im.gva) != mappedGvaMem_.end()) {
            BM_LOG_INFO("remote slice on rank(" << im.rankId << ") has maped gva:" << VaToStr(im.gva));
            continue;
        }
        BM_LOG_INFO("Try to mmap rank:" << im.rankId << " superPodId:" << im.superPodId << " serverId:" << im.serverId
                                        << " devId:" << im.logicDevId << " segType:" << options_.segType << " size:"
                                        << im.size << " gva:" << VaToStr(im.gva) << " dva:" << VaToStr(im.deviceVa));
        drv_mem_handle_t *handle = nullptr;
        auto ret = DlHalApi::HalMemImport(MEM_HANDLE_TYPE_FABRIC, &im.shareHandle, logicDeviceId_, &handle);
        BM_VALIDATE_RETURN(
            ret == BM_OK, "HalMemImport memory failed:" << ret << " local sdid:" << sdid_ << " remote ssid:" << im.sdid,
            BM_ERROR);

        ret = DlHalApi::HalMemMap(reinterpret_cast<void *>(im.deviceVa), im.size, 0, handle, 0);
        if (ret != BM_OK) {
            BM_LOG_ERROR("HalMemMap memory failed:" << ret << " gva:" << VaToStr(im.gva)
                                                    << " dva:" << VaToStr(im.deviceVa) << " size:" << im.size);
            DlHalApi::HalMemRelease(handle);
            return BM_ERROR;
        }
        mappedGvaMem_.emplace(im.gva, handle);

        auto memType = im.magic == HBM_SLICE_EXPORT_INFO_MAGIC ? HYBM_MEM_TYPE_DEVICE : HYBM_MEM_TYPE_HOST;
        ret = HybmVaManager::GetInstance().AddVaInfoFromExternal({im.gva, im.deviceVa, 0, im.size, memType},
                                                                 options_.rankId, im.rankId);
        BM_ASSERT_RETURN(ret == BM_OK, ret);
    }
    imports_.clear();
    return BM_OK;
}

Result HybmVmmBasedSegment::Unmap() noexcept
{
    if (!options_.shared) {
        BM_LOG_INFO("no need to share, skip unmap");
        return BM_OK;
    }

    for (auto it : mappedGvaMem_) {
        auto deviceVa = HybmVaManager::GetInstance().TransformVa(it.first, HVM_GVA, HVM_DVA);
        DlHalApi::HalMemUnmap(reinterpret_cast<void *>(deviceVa));
        DlHalApi::HalMemRelease(it.second);
        HybmVaManager::GetInstance().RemoveOneVaInfo(it.first);
    }
    mappedGvaMem_.clear();
    return BM_OK;
}

Result HybmVmmBasedSegment::RemoveImported(const std::vector<uint32_t> &ranks) noexcept
{
    if (!options_.shared) {
        BM_LOG_INFO("no need to share, skip remove");
        return BM_OK;
    }
    for (auto &rank : ranks) {
        if (rank >= options_.rankCnt) {
            BM_LOG_ERROR("input rank is invalid! rank:" << rank << " rankSize:" << options_.rankCnt);
            return BM_INVALID_PARAM;
        }
    }

    for (auto &rank : ranks) {
        uint64_t gvaLocal = reinterpret_cast<uint64_t>(globalVirtualAddress_) + options_.maxSize * rank;
        auto it = mappedGvaMem_.lower_bound(gvaLocal);
        auto st = it;
        while (it != mappedGvaMem_.end() && (*it).first < gvaLocal + options_.maxSize) {
            auto deviceVa = HybmVaManager::GetInstance().TransformVa((*it).first, HVM_GVA, HVM_DVA);
            DlHalApi::HalMemUnmap(reinterpret_cast<void *>(deviceVa));
            DlHalApi::HalMemRelease((*it).second);
            HybmVaManager::GetInstance().RemoveOneVaInfo((*it).first);
            it++;
        }

        if (st != it) {
            mappedGvaMem_.erase(st, it);
        }
    }

    // remove imports_ infos for specified ranks
    imports_.erase(std::remove_if(imports_.begin(), imports_.end(),
                                  [&ranks](const HostSdmaExportInfo &info) {
                                      return std::find(ranks.begin(), ranks.end(), info.rankId) != ranks.end();
                                  }),
                   imports_.end());
    return BM_OK;
}

MemSlicePtr HybmVmmBasedSegment::GetMemSlice(hybm_mem_slice_t slice, bool quiet) const noexcept
{
    auto index = MemSlice::GetIndexFrom(slice);
    MemSlicePtr target = nullptr;
    auto pos = slices_.find(index);
    auto registerPos = registerSlices_.find(index);
    if (pos != slices_.end()) {
        target = pos->second.slice;
    } else if (registerPos != registerSlices_.end()) {
        target = registerPos->second.first.slice;
    }

    if (target == nullptr || !target->ValidateId(slice)) {
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

bool HybmVmmBasedSegment::GetRankIdByAddr(const void *addr, uint64_t size, uint32_t &rankId) const noexcept
{
    if (!MemoryInRange(addr, size)) {
        rankId = options_.rankId;
        return false;
    } else {
        uint64_t offset = static_cast<const uint8_t *>(addr) - static_cast<const uint8_t *>(globalVirtualAddress_);
        rankId = offset / options_.maxSize;
        return true;
    }
}

bool HybmVmmBasedSegment::CheckSdmaReaches(uint32_t rankId) const noexcept
{
    return true;
}