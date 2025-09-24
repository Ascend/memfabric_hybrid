/*
Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#include "hybm_sdma_mem_segment.h"

#include <cstdint>

#include "hybm_ex_info_transfer.h"
#include "hybm_gvm_user.h"
#include "hybm_types.h"

using namespace ock::mf;

Result MemSegmentHostSDMA::ValidateOptions() noexcept
{
    if (options_.segType != HYBM_MST_DRAM || options_.size == 0 || (options_.size % DEVICE_LARGE_PAGE_SIZE) != 0) {
        return BM_INVALID_PARAM;
    }

    return BM_OK;
}

Result MemSegmentHostSDMA::ReserveMemorySpace(void **address) noexcept
{
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

Result MemSegmentHostSDMA::AllocLocalMemory(uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept
{
    if ((size % DEVICE_LARGE_PAGE_SIZE) != 0UL || size + allocatedSize_ > options_.size) {
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

Result MemSegmentHostSDMA::Export(std::string &exInfo) noexcept
{
    return BM_OK;
}

Result MemSegmentHostSDMA::Export(const std::shared_ptr<MemSlice> &slice, std::string &exInfo) noexcept
{
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

Result MemSegmentHostSDMA::Import(const std::vector<std::string> &allExInfo) noexcept
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
    std::copy(deserializedInfos.begin(), deserializedInfos.end(), std::back_inserter(imports_));
    return BM_OK;
}

Result MemSegmentHostSDMA::Mmap() noexcept
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

Result MemSegmentHostSDMA::Unmap() noexcept
{
    BM_ASSERT_LOG_AND_RETURN(options_.shared, "hybm_gvm_mem_close requires shared memory", BM_OK);
    for (auto va : mappedMem_) {
        hybm_gvm_mem_close(va);
    }
    mappedMem_.clear();
    return BM_OK;
}

Result MemSegmentHostSDMA::RemoveImported(const std::vector<uint32_t> &ranks) noexcept
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

std::shared_ptr<MemSlice> MemSegmentHostSDMA::GetMemSlice(hybm_mem_slice_t slice) const noexcept
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

bool MemSegmentHostSDMA::MemoryInRange(const void *begin, uint64_t size) const noexcept
{
    if (begin < globalVirtualAddress_) {
        return false;
    }

    if (reinterpret_cast<const uint8_t *>(begin) + size > globalVirtualAddress_ + totalVirtualSize_) {
        return false;
    }

    return true;
}