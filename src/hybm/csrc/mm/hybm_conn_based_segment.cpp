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
#include "hybm_conn_based_segment.h"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstddef>

#include "hybm_logger.h"
#include "hybm_ex_info_transfer.h"
#include "hybm_va_manager.h"
#include "dl_hal_api.h"

using namespace ock::mf;

Result HybmConnBasedSegment::ValidateOptions() noexcept
{
    if (options_.segType != HYBM_MST_DRAM || options_.maxSize == 0 || (options_.maxSize % HYBM_LARGE_PAGE_SIZE) != 0) {
        BM_LOG_ERROR("Validate options error type(" << options_.segType << ") size(" << options_.maxSize);
        return BM_INVALID_PARAM;
    }

    if (UINT64_MAX / options_.maxSize < options_.rankCnt) {
        BM_LOG_ERROR("Validate options error rankCnt(" << options_.rankCnt << ") size(" << options_.maxSize);
        return BM_INVALID_PARAM;
    }

    return BM_OK;
}

Result HybmConnBasedSegment::ReserveMemorySpace(void **address) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(ValidateOptions() == BM_OK, "Failed to validate options.", BM_INVALID_PARAM);
    BM_ASSERT_LOG_AND_RETURN(globalVirtualAddress_ == nullptr, "Already prepare virtual memory.", BM_NOT_INITIALIZED);
    BM_ASSERT_LOG_AND_RETURN(address != nullptr, "Invalid param, address is NULL.", BM_INVALID_PARAM);
    BM_ASSERT_LOG_AND_RETURN(PrepareShareMemoryFd() == BM_OK, "PrepareShareMemoryFd failed.", BM_ERROR);
    BM_ASSERT_LOG_AND_RETURN(options_.rankId < options_.rankCnt,
                             "rank(" << options_.rankId << ") but total " << options_.rankCnt, BM_INVALID_PARAM);

    uint64_t totalSize = options_.rankCnt * options_.maxSize;
    uint64_t localSize = options_.isSecondMapping ? options_.maxSize : totalSize;
    auto gvaInfo = HybmVaManager::GetInstance().AllocReserveGva(options_.rankId, totalSize, localSize,
                                                                HYBM_MEM_TYPE_HOST, options_.isSecondMapping);
    BM_ASSERT_LOG_AND_RETURN(gvaInfo.va[HVM_GVA] > 0, "Invalid param, start is 0.", BM_ERROR);
    void *startAddr = reinterpret_cast<void *>(gvaInfo.va[HVM_GVA]);
    if (!options_.isSecondMapping) {
        void *mapped = mmap(startAddr, totalSize, PROT_NONE,
                            MAP_FIXED_NOREPLACE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE, -1, 0);

        if (mapped == MAP_FAILED || (uint64_t)mapped != (uint64_t)startAddr) {
            BM_LOG_ERROR("Failed to mmap size:" << totalSize << " addr:" << startAddr << " ret:" << mapped
                                                << " error: " << errno);
            return BM_ERROR;
        }
    }
    globalVirtualAddress_ = (uint8_t *)startAddr;
    totalVirtualSize_ = totalSize;
    if (options_.isSecondMapping) {
        localVirtualBase_ = (uint8_t *)gvaInfo.va[HVM_DVA];
    } else {
        localVirtualBase_ = globalVirtualAddress_ + options_.maxSize * options_.rankId;
    }
    allocatedSize_ = 0UL;
    sliceCount_ = 0;
    *address = globalVirtualAddress_;
    return BM_OK;
}

Result HybmConnBasedSegment::UnReserveMemorySpace() noexcept
{
    BM_LOG_INFO("un-reserve memory space.");
    FreeMemory();
    return BM_OK;
}

void HybmConnBasedSegment::LvaShmReservePhysicalMemory(void *mappedAddress, uint64_t size) noexcept
{
    BM_ASSERT_RET_VOID(mappedAddress != nullptr);
    auto *pos = static_cast<uint8_t *>(mappedAddress);
    uint64_t setLength = 0;
    while (setLength < size) {
        *pos = 0;
        setLength += HYBM_LARGE_PAGE_SIZE;
        pos += HYBM_LARGE_PAGE_SIZE;
    }

    pos = static_cast<uint8_t *>(mappedAddress) + (size - 1L);
    *pos = 0;
}

Result HybmConnBasedSegment::AllocLocalMemory(uint64_t size, MemSlicePtr &slice) noexcept
{
    if ((size % HYBM_LARGE_PAGE_SIZE) != 0UL || size + allocatedSize_ > options_.maxSize) {
        BM_LOG_ERROR("invalid allocate memory size : " << size << ", now used " << allocatedSize_ << " of "
                                                       << options_.maxSize);
        return BM_INVALID_PARAM;
    }
    if (options_.isSecondMapping && options_.shmFd >= 0) {
        BM_LOG_ERROR("do not support memory pool greater 128TB, isSecondMapping: " << options_.isSecondMapping
                                                                                   << ", shmFd:" << options_.shmFd);
        return BM_INVALID_PARAM;
    }

    void *sliceAddr = localVirtualBase_ + allocatedSize_;
    auto gva = reinterpret_cast<uint64_t>(globalVirtualAddress_ + options_.maxSize * options_.rankId + allocatedSize_);
    void *mapped = nullptr;
    auto ret = MapSlice(mapped, sliceAddr, allocatedSize_, size, gva);
    if (ret != BM_OK) {
        return ret;
    }
    allocatedSize_ += size;
    slice = std::make_shared<MemSlice>(sliceCount_++, HYBM_MEM_TYPE_HOST, MEM_PT_TYPE_SVM, gva,
                                       reinterpret_cast<uint64_t>(mapped), size);
    slices_.emplace(slice->index_, slice);
    BM_LOG_DEBUG("allocate slice(idx:" << slice->index_ << ", size:" << slice->size_ << " va:" << mapped << ").");
    return BM_OK;
}

Result HybmConnBasedSegment::Export(std::string &exInfo) noexcept
{
    return BM_OK;
}

Result HybmConnBasedSegment::Export(const MemSlicePtr &slice, std::string &exInfo) noexcept
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
    if (exp != exportMap_.end()) {
        exInfo = exp->second;
        return BM_OK;
    }
    AllocatedGvaInfo gvaInfo{};
    if (slice->size_ > 0) {
        bool found = false;
        std::tie(gvaInfo, found) = HybmVaManager::GetInstance().FindAllocByVa(slice->vAddress_, HVM_HVA);
        if (!found) {
            BM_LOG_ERROR("input host va(" << slice->vAddress_ << ") not match.");
            return BM_INVALID_PARAM;
        }
    }

    HostExportInfo info;
    info.gva = gvaInfo.base.va[HVM_GVA];
    info.sliceIndex = static_cast<uint32_t>(slice->index_);
    info.rankId = options_.rankId;
    info.size = slice->size_;
    info.pageTblType = MEM_PT_TYPE_SVM;
    info.memSegType = HYBM_MST_DRAM;
    info.exchangeType = HYBM_INFO_EXG_IN_NODE;
    auto ret = LiteralExInfoTranslater<HostExportInfo>{}.Serialize(info, exInfo);
    if (ret != BM_OK) {
        BM_LOG_ERROR("export info failed: " << ret);
        return BM_ERROR;
    }

    exportMap_[slice->index_] = exInfo;
    return BM_OK;
}

Result HybmConnBasedSegment::Import(const std::vector<std::string> &allExInfo, void *addresses[]) noexcept
{
    LiteralExInfoTranslater<HostExportInfo> translator;
    std::vector<HostExportInfo> deserializedInfos{allExInfo.size()};
    for (auto i = 0U; i < allExInfo.size(); i++) {
        auto ret = translator.Deserialize(allExInfo[i], deserializedInfos[i]);
        if (ret != 0) {
            BM_LOG_ERROR("deserialize imported info(" << i << ") failed.");
            return BM_INVALID_PARAM;
        }
    }

    try {
        std::copy(deserializedInfos.begin(), deserializedInfos.end(), std::back_inserter(imports_));
    } catch (...) {
        BM_LOG_ERROR("copy failed.");
        return BM_MALLOC_FAILED;
    }
    for (const auto &import : deserializedInfos) {
        if (import.rankId == options_.rankId) {
            continue;
        }
        if (import.size == 0) {
            continue;
        }

        auto ret = HybmVaManager::GetInstance().AddVaInfoFromExternal(
            {import.gva, 0, 0, import.size, HYBM_MEM_TYPE_HOST}, options_.rankId, import.rankId);
        BM_ASSERT_RETURN(ret == BM_OK, ret);
    }
    return BM_OK;
}

Result HybmConnBasedSegment::Mmap() noexcept
{
    for (const auto &import : imports_) {
        if (import.rankId == options_.rankId) {
            continue;
        }
        mappedGvaMem_.insert(import.gva);
    }
    imports_.clear();
    return 0;
}

Result HybmConnBasedSegment::Unmap() noexcept
{
    for (auto gva : mappedGvaMem_) {
        HybmVaManager::GetInstance().RemoveOneVaInfo(gva);
    }
    mappedGvaMem_.clear();
    return 0;
}

MemSlicePtr HybmConnBasedSegment::GetMemSlice(hybm_mem_slice_t slice, bool quiet) const noexcept
{
    auto index = MemSlice::GetIndexFrom(slice);
    auto pos = slices_.find(index);
    if (pos == slices_.end()) {
        if (quiet) {
            BM_LOG_DEBUG("Failed to get slice, index(" << index << ") not find");
        } else {
            BM_LOG_ERROR("Failed to get slice, index(" << index << ") not find");
        }
        return nullptr;
    }

    auto target = pos->second.slice;
    if (!target->ValidateId(slice)) {
        if (quiet) {
            BM_LOG_DEBUG("Failed to get slice, slice is invalid index(" << index << ")");
        } else {
            BM_LOG_ERROR("Failed to get slice, slice is invalid index(" << index << ")");
        }
        return nullptr;
    }

    return target;
}

bool HybmConnBasedSegment::MemoryInRange(const void *begin, uint64_t size) const noexcept
{
    if (begin < globalVirtualAddress_) {
        return false;
    }

    if (reinterpret_cast<const uint8_t *>(begin) + size > globalVirtualAddress_ + totalVirtualSize_) {
        return false;
    }

    return true;
}

bool HybmConnBasedSegment::GetRankIdByAddr(const void *addr, uint64_t size, uint32_t &rankId) const noexcept
{
    if (!MemoryInRange(addr, size)) {
        rankId = options_.rankId;
        return false;
    } else {
        auto rankSize = options_.maxSize;
        rankId = (reinterpret_cast<uint64_t>(addr) - reinterpret_cast<uint64_t>(globalVirtualAddress_)) / rankSize;
        return true;
    }
}

void HybmConnBasedSegment::FreeMemory() noexcept
{
    while (!slices_.empty()) {
        auto slice = slices_.begin()->second.slice;
        ReleaseSliceMemory(slice);
    }

    if (localVirtualBase_ != nullptr && allocatedSize_ > 0) {
        if (munmap(localVirtualBase_, allocatedSize_) != 0) {
            BM_LOG_ERROR("Failed to unmap local memory");
        }
        localVirtualBase_ = nullptr;
    }

    if (options_.isSecondMapping) {
        globalVirtualAddress_ = localVirtualBase_ = nullptr;
    } else if (globalVirtualAddress_ != nullptr) {
        if (munmap(globalVirtualAddress_, totalVirtualSize_) != 0) {
            BM_LOG_ERROR("Failed to unmap global memory");
        }
        HybmVaManager::GetInstance().FreeReserveGva((uintptr_t)globalVirtualAddress_);
        globalVirtualAddress_ = nullptr;
    }
}

Result HybmConnBasedSegment::PrepareShareMemoryFd() const noexcept
{
    if (options_.shmFd < 0) {
        return BM_OK;
    }

    struct stat buf{};
    auto ret = fstat(options_.shmFd, &buf);
    if (ret != 0) {
        BM_LOG_ERROR("share mem fd: " << options_.shmFd << " stat failed: " << errno << ":" << strerror(errno));
        return BM_INVALID_PARAM;
    }

    if (static_cast<uint64_t>(buf.st_size) >= options_.size) {
        return BM_OK;
    }

    ret = ftruncate(options_.shmFd, static_cast<off_t>(options_.size));
    if (ret != 0) {
        BM_LOG_ERROR("share mem fd: " << options_.shmFd << " truncate from " << buf.st_size << " to " << options_.size
                                      << " failed: " << errno << ":" << strerror(errno));
        return BM_ERROR;
    }

    return BM_OK;
}

Result HybmConnBasedSegment::MapSlice(void *&mapped, void *sliceAddr, uint64_t lvOffset,
                                      uint64_t size, uint64_t gva) noexcept
{
    if (size == 0) {
        return BM_OK;
    }

    auto prot = PROT_READ | PROT_WRITE;
    auto flags = MAP_FIXED | MAP_HUGETLB;
    void *dva = nullptr;
    if (options_.shmFd < 0) {
        mapped = mmap(sliceAddr, size, prot, flags | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    } else {
        mapped = mmap(sliceAddr, size, prot, flags | MAP_SHARED, options_.shmFd, lvOffset);
    }

    if (mapped == MAP_FAILED || mapped != sliceAddr) {
        BM_LOG_WARN("Failed to alloc size:" << size << " addr:" << sliceAddr << " mapped:" << mapped
                                            << " with huge page, error:" << errno << ", " << SafeStrError(errno)
                                            << ". fallback to mmap with regular pagesize");
        flags &= ~MAP_HUGETLB;
        if (options_.shmFd < 0) {
            mapped = mmap(sliceAddr, size, prot, flags | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        } else {
            mapped = mmap(sliceAddr, size, prot, flags | MAP_SHARED, options_.shmFd, lvOffset);
        }
    }

    if (mapped == MAP_FAILED || mapped != sliceAddr) {
        BM_LOG_ERROR("Failed to alloc size:" << size << " addr:" << sliceAddr << " mapped:" << mapped
                                             << " error:" << errno << ", " << SafeStrError(errno));
        return BM_ERROR;
    }

    if (options_.dataOpType & HYBM_DOP_TYPE_DEVICE_RDMA) {
        auto ret = DlHalApi::HalHostRegister(sliceAddr, size, HOST_MEM_MAP_DEV, logicDeviceId_, &dva);
        if (ret != BM_OK) {
            BM_LOG_ERROR("register host va failed, ret:" << ret);
            munmap(sliceAddr, size);
            return BM_ERROR;
        }
    }
    int ret = HybmVaManager::GetInstance().AddVaInfo(
        {gva, (uint64_t)dva, (uint64_t)sliceAddr, size, HYBM_MEM_TYPE_HOST}, options_.rankId);
    if (ret != 0) {
        BM_LOG_ERROR("AddVaInfo failed, size: " << size << " ret: " << ret);
        DlHalApi::HalHostUnregisterEx(sliceAddr, logicDeviceId_, HOST_MEM_MAP_DEV);
        munmap(sliceAddr, size);
        return ret;
    }

    LvaShmReservePhysicalMemory(mapped, size);
    return BM_OK;
}

Result HybmConnBasedSegment::RemoveImported(const std::vector<uint32_t> &ranks) noexcept
{
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
        while (it != mappedGvaMem_.end() && (*it) < gvaLocal + options_.maxSize) {
            HybmVaManager::GetInstance().RemoveOneVaInfo(*it);
            ++it;
        }
        if (st != it) {
            mappedGvaMem_.erase(st, it);
        }
    }
    return BM_OK;
}

Result HybmConnBasedSegment::RegisterMemory(const void *addr, uint64_t size, MemSlicePtr &slice) noexcept
{
    auto ret = RegisterMemCommon(addr, size, slice);
    BM_ASSERT_RETURN(ret == BM_OK, ret);
    slices_.emplace(slice->index_, slice);
    return BM_OK;
}

Result HybmConnBasedSegment::ReleaseSliceMemory(const MemSlicePtr &slice) noexcept
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
    HybmVaManager::GetInstance().RemoveOneVaInfo(slice->gva_);
    slices_.erase(pos);
    return BM_OK;
}

Result HybmConnBasedSegment::GetExportSliceSize(size_t &size) noexcept
{
    size = sizeof(HostExportInfo);
    return BM_OK;
}