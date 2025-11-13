/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "hybm_conn_based_segment.h"

#include <sys/mman.h>

#include "hybm_logger.h"
#include "hybm_ex_info_transfer.h"

using namespace ock::mf;

Result HybmConnBasedSegment::ValidateOptions() noexcept
{
    if (options_.segType != HYBM_MST_DRAM || options_.size == 0 || (options_.size % HYBM_LARGE_PAGE_SIZE) != 0) {
        BM_LOG_ERROR("Validate options error type(" << options_.segType << ") size(" << options_.size);
        return BM_INVALID_PARAM;
    }

    if (UINT64_MAX / options_.size < options_.rankCnt) {
        BM_LOG_ERROR("Validate options error rankCnt(" << options_.rankCnt << ") size(" << options_.size);
        return BM_INVALID_PARAM;
    }

    return BM_OK;
}

Result HybmConnBasedSegment::ReserveMemorySpace(void **address) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(ValidateOptions() == BM_OK, "Failed to validate options.", BM_INVALID_PARAM);
    BM_ASSERT_LOG_AND_RETURN(globalVirtualAddress_ == nullptr, "Already prepare virtual memory.", BM_NOT_INITIALIZED);
    BM_ASSERT_LOG_AND_RETURN(address != nullptr, "Invalid param, address is NULL.", BM_INVALID_PARAM);

    void *startAddr = reinterpret_cast<void *>(HYBM_HOST_GVA_START_ADDR);
    uint64_t totalSize = options_.rankCnt * options_.size;
    void *mapped = mmap(startAddr, totalSize, PROT_READ | PROT_WRITE,
                        MAP_FIXED | MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE, -1, 0);
    if (mapped == MAP_FAILED) {
        BM_LOG_ERROR("Failed to mmap size:" << totalSize << " error: " << errno);
        return BM_ERROR;
    }
    globalVirtualAddress_ = (uint8_t *) startAddr;
    totalVirtualSize_ = totalSize;
    localVirtualBase_ = globalVirtualAddress_ + options_.size * options_.rankId;
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

Result HybmConnBasedSegment::AllocLocalMemory(uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept
{
    if ((size % HYBM_LARGE_PAGE_SIZE) != 0UL || size + allocatedSize_ > options_.size) {
        BM_LOG_ERROR("invalid allocate memory size : " << size << ", now used " << allocatedSize_ << " of "
                                                       << options_.size);
        return BM_INVALID_PARAM;
    }

    void *sliceAddr = localVirtualBase_ + allocatedSize_;
    void *mapped = mmap(sliceAddr, size, PROT_READ | PROT_WRITE,
                        MAP_FIXED | MAP_ANONYMOUS | MAP_HUGETLB | MAP_PRIVATE, -1, 0);
    if (mapped == MAP_FAILED) {
        BM_LOG_ERROR("Failed to alloc size:" << size << " error:" << errno << ", " << SafeStrError(errno));
        return BM_ERROR;
    }
    LvaShmReservePhysicalMemory(sliceAddr, size);
    allocatedSize_ += size;
    slice = std::make_shared<MemSlice>(sliceCount_++, MEM_TYPE_HOST_DRAM, MEM_PT_TYPE_SVM,
                                       reinterpret_cast<uint64_t>(sliceAddr), size);
    slices_.emplace(slice->index_, slice);
    BM_LOG_DEBUG("allocate slice(idx:" << slice->index_ << ", size:" << slice->size_ << ").");

    return BM_OK;
}

Result HybmConnBasedSegment::Export(std::string &exInfo) noexcept
{
    return BM_OK;
}

Result HybmConnBasedSegment::Export(const std::shared_ptr<MemSlice> &slice, std::string &exInfo) noexcept
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

    HostExportInfo info;
    info.mappingOffset = slice->vAddress_ - (uint64_t)(localVirtualBase_);
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
    try {
        std::copy(deserializedInfos.begin(), deserializedInfos.end(), std::back_inserter(imports_));
    } catch (...) {
        BM_LOG_ERROR("copy failed.");
        return BM_MALLOC_FAILED;
    }
    return BM_OK;
}

Result HybmConnBasedSegment::Mmap() noexcept
{
    imports_.clear();
    return 0;
}

Result HybmConnBasedSegment::Unmap() noexcept
{
    return 0;
}


std::shared_ptr<MemSlice> HybmConnBasedSegment::GetMemSlice(hybm_mem_slice_t slice) const noexcept
{
    auto index = MemSlice::GetIndexFrom(slice);
    auto pos = slices_.find(index);
    if (pos == slices_.end()) {
        BM_LOG_ERROR("Failed to get slice, index(" << index << ") not find");
        return nullptr;
    }

    auto target = pos->second.slice;
    if (!target->ValidateId(slice)) {
        BM_LOG_ERROR("Failed to get slice, slice is invalid index(" << index << ")");
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

void HybmConnBasedSegment::GetRankIdByAddr(const void *addr, uint64_t size, uint32_t &rankId) const noexcept
{
    if (!MemoryInRange(addr, size)) {
        rankId = options_.rankId;
    } else {
        rankId = (reinterpret_cast<uint64_t>(addr) - reinterpret_cast<uint64_t>(globalVirtualAddress_)) / options_.size;
    }
}

void HybmConnBasedSegment::FreeMemory() noexcept
{
    if (localVirtualBase_ != nullptr) {
        if (munmap(localVirtualBase_, options_.size) != 0) {
            BM_LOG_ERROR("Failed to unmap local memory");
        }
        localVirtualBase_ = nullptr;
    }
    if (globalVirtualAddress_ != nullptr) {
        if (munmap(globalVirtualAddress_, totalVirtualSize_) != 0) {
            BM_LOG_ERROR("Failed to unmap global memory");
        }
        globalVirtualAddress_ = nullptr;
    }
}

Result HybmConnBasedSegment::RemoveImported(const std::vector<uint32_t> &ranks) noexcept
{
    return 0;
}

Result HybmConnBasedSegment::RegisterMemory(const void *addr, uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept
{
    return BM_OK;
}

Result HybmConnBasedSegment::ReleaseSliceMemory(const std::shared_ptr<MemSlice> &slice) noexcept
{
    return BM_OK;
}

Result HybmConnBasedSegment::GetExportSliceSize(size_t &size) noexcept
{
    return BM_OK;
}