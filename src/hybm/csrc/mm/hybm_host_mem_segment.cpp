/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm_host_mem_segment.h"

#include <sys/mman.h>

#include "hybm_logger.h"
#include "hybm_ex_info_transfer.h"

using namespace ock::mf;

namespace {
constexpr uint64_t HYBM_HOST_GVA_START_ADDR = 0x2000000000000ULL;
}

Result MemSegmentHost::ValidateOptions() noexcept
{
    if (options_.segType != HYBM_MST_DRAM || options_.size == 0 || (options_.size % DEVICE_LARGE_PAGE_SIZE) != 0) {
        BM_LOG_ERROR("Validate options error type(" << options_.segType << ") size(" << options_.size);
        return BM_INVALID_PARAM;
    }

    return BM_OK;
}

Result MemSegmentHost::ReserveMemorySpace(void **address) noexcept
{
    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(globalVirtualAddress_ == nullptr, "Already prepare virtual memory.");
    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(address != nullptr, "Invalid param, address is NULL.");

    void *startAddr = (void *) HYBM_HOST_GVA_START_ADDR;
    uint64_t totalSize = options_.rankCnt * options_.size;
    void *mapped = mmap(startAddr, totalSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (mapped == MAP_FAILED) {
        BM_LOG_ERROR("Failed to mmap startAddr:" << startAddr << " size:" << totalSize << " error:" << strerror(errno));
        return BM_ERROR;
    }
    // TODO Register MrInfo
    globalVirtualAddress_ = (uint8_t *) startAddr;
    totalVirtualSize_ = totalSize;
    localVirtualBase_ = globalVirtualAddress_ + options_.size * options_.rankId;
    allocatedSize_ = 0UL;
    sliceCount_ = 0;
    *address = (void *) globalVirtualAddress_;
    return BM_OK;
}

void MemSegmentHost::LvaShmReservePhysicalMemory(void *mappedAddress, uint64_t size) noexcept
{
    auto *pos = (uint8_t *) (mappedAddress);
    uint64_t setLength = 0;
    while (setLength < size) {
        *pos = 0;
        setLength += DEVICE_LARGE_PAGE_SIZE;
        pos += DEVICE_LARGE_PAGE_SIZE;
    }

    pos = (uint8_t *) (mappedAddress) + (size - 1L);
    *pos = 0;
}

Result MemSegmentHost::AllocLocalMemory(uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept
{
    if ((size % DEVICE_LARGE_PAGE_SIZE) != 0UL || size + allocatedSize_ > options_.size) {
        BM_LOG_ERROR("invalid allocate memory size : " << size << ", now used " << allocatedSize_ << " of "
                                                       << options_.size);
        return BM_INVALID_PARAM;
    }

    auto sliceAddr = localVirtualBase_ + allocatedSize_;
    void *mapped = mmap(sliceAddr, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_HUGETLB | MAP_FIXED, -1, 0);
    if (mapped == MAP_FAILED) {
        BM_LOG_ERROR("Failed to alloc startAddr:" << sliceAddr << " size:" << size << " error:" << strerror(errno));
        return BM_ERROR;
    }
    LvaShmReservePhysicalMemory(sliceAddr, size);
    allocatedSize_ += size;
    slice = std::make_shared<MemSlice>(sliceCount_++, MEM_TYPE_HOST_DRAM, MEM_PT_TYPE_SVM,
                                       (uint64_t)(ptrdiff_t)(void *)sliceAddr, size);
    slices_.emplace(slice->index_, slice);
    BM_LOG_DEBUG("allocate slice(idx:" << slice->index_ << ", size:" << slice->size_ << ", address:0x" << std::hex
                                       << slice->vAddress_ << ").");

    return BM_OK;
}

Result MemSegmentHost::Export(std::string &exInfo) noexcept
{
    return BM_OK;
}

Result MemSegmentHost::Export(const std::shared_ptr<MemSlice> &slice, std::string &exInfo) noexcept
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

    HostExportInfo info{};
    info.magic = EXPORT_INFO_MAGIC;
    info.version = EXPORT_INFO_VERSION;
    info.mappingOffset = slice->vAddress_ - (uint64_t)(localVirtualBase_);
    info.sliceIndex = static_cast<uint32_t>(slice->index_);
    info.rankId = options_.rankId;
    info.size = slice->size_;
    info.pageTblType = MEM_PT_TYPE_SVM;
    info.memSegType = HYBM_MST_DRAM;
    info.exchangeType = HYBM_INFO_EXG_IN_NODE;
    info.oneSideKey = oneSideKey_;
    auto ret = LiteralExInfoTranslater<HostExportInfo>{}.Serialize(info, exInfo);
    if (ret != BM_OK) {
        BM_LOG_ERROR("export info failed: " << ret);
        return BM_ERROR;
    }

    exportMap_[slice->index_] = exInfo;
    return BM_OK;
}

Result MemSegmentHost::Import(const std::vector<std::string> &allExInfo) noexcept
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
        if (deserializedInfos[i].magic != EXPORT_INFO_MAGIC) {
            BM_LOG_ERROR("import info(" << i << ") magic(" << deserializedInfos[i].magic << ") invalid.");
            return BM_INVALID_PARAM;
        }

        if (deserializedInfos[i].rankId == options_.rankId) {
            localIdx = i;
        }
    }
    BM_ASSERT_RETURN(localIdx < deserializedInfos.size(), BM_INVALID_PARAM);
    std::copy(deserializedInfos.begin(), deserializedInfos.end(), std::back_inserter(imports_));
    return BM_OK;
}

Result MemSegmentHost::Mmap() noexcept
{
    return 0;
}

Result MemSegmentHost::Unmap() noexcept
{
    return 0;
}


std::shared_ptr<MemSlice> MemSegmentHost::GetMemSlice(hybm_mem_slice_t slice) const noexcept
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

bool MemSegmentHost::MemoryInRange(const void *begin, uint64_t size) const noexcept
{
    if (begin < globalVirtualAddress_) {
        return false;
    }

    if ((const uint8_t *)begin + size >= globalVirtualAddress_ + totalVirtualSize_) {
        return false;
    }

    return true;
}

void MemSegmentHost::FreeMemory() noexcept
{
    if (localVirtualBase_ != nullptr) {
        munmap(localVirtualBase_, options_.size);
        localVirtualBase_ = nullptr;
    }
    if (globalVirtualAddress_ != nullptr) {
        munmap(globalVirtualAddress_, totalVirtualSize_);
        globalVirtualAddress_ = nullptr;
    }
}
