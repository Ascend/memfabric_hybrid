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
#include "hybm_dev_legacy_segment.h"

#include <cstring>
#include <iomanip>
#include <fstream>
#include <sstream>
#include "dl_api.h"
#include "dl_hal_api.h"
#include "devmm_svm_gva.h"
#include "dl_acl_api.h"
#include "hybm_common_include.h"
#include "hybm_ex_info_transfer.h"
#include "hybm_gva.h"
#include "hybm_logger.h"
#include "hybm_networks_common.h"

namespace ock {
namespace mf {
Result HybmDevLegacySegment::ValidateOptions() noexcept
{
    if (options_.segType != HYBM_MST_HBM || options_.size == 0 || options_.devId < 0 ||
        (options_.size % HYBM_LARGE_PAGE_SIZE) != 0) {
        BM_LOG_ERROR("Invalid options segType:" << options_.segType << " size:" << options_.size);
        return BM_INVALID_PARAM;
    }

    if (UINT64_MAX / options_.size < options_.rankCnt) {
        BM_LOG_ERROR("Validate options error rankCnt(" << options_.rankCnt << ") size(" << options_.size);
        return BM_INVALID_PARAM;
    }
    return BM_OK;
}

uint64_t HybmDevLegacySegment::GetReserveChunkSize(size_t totalSize, size_t singleRankSize) noexcept
{
    if (totalSize == 0 || singleRankSize == 0) {
        return 0;
    }
    constexpr uint64_t maxChunk = 128ULL * GB;
    if (totalSize <= maxChunk) {
        return totalSize;
    }
    uint64_t n = totalSize / singleRankSize;
    uint64_t maxM = (128ULL * GB) / singleRankSize;
    uint64_t baseM = 1;
    uint64_t start = maxM;
    for (uint64_t m = start; m >= 1; --m) {
        if (n % m == 0) {
            baseM = m;
            break;
        }
    }
    uint64_t result = baseM * singleRankSize;
    BM_LOG_INFO("chunk size: " << (result / GB) << "G" << ", total size: " << (totalSize / GB) << "G");
    if (totalSize % result != 0) {
        BM_LOG_ERROR("chunk size: " << (result / GB) << "G" << ", total size: " << (totalSize / GB) << "G");
        return 0;
    }
    return result;
}

int32_t GvaUnreserveMemory(uint64_t address, uint64_t total, size_t singleRankSize)
{
    uint64_t ptr = address;
    size_t maxChunk = HybmDevLegacySegment::GetReserveChunkSize(total, singleRankSize);
    while (ptr < address + total) {
        auto ret = drv::HalGvaUnreserveMemory(ptr);
        BM_ASSERT_RETURN(ret == 0, ret);
        ptr += maxChunk;
    }
    return BM_OK;
}

static int32_t GvaReserveMemory(uint64_t *address, size_t size, int32_t deviceId, uint64_t flags, size_t singleRankSize)
{
    if (size == 0) {
        return BM_ERROR;
    }
    size_t maxChunk = HybmDevLegacySegment::GetReserveChunkSize(size, singleRankSize);
    std::vector<uint64_t> chunkMaps;
    size_t reserved = 0;
    while (reserved < size) {
        uint64_t currentBase = chunkMaps.empty() ? 0 : *chunkMaps.rbegin();
        size_t chunk = std::min(maxChunk, size - reserved);
        auto ret = drv::HalGvaReserveMemory(&currentBase, chunk, deviceId, flags);
        if (ret != 0 || currentBase == 0 || (!chunkMaps.empty() && currentBase + maxChunk != *chunkMaps.rbegin())) {
            BM_LOG_ERROR("current_base: " << std::hex << currentBase << ", rbegin: " << *chunkMaps.rbegin());
            for (const auto &ptr : chunkMaps) {
                drv::HalGvaUnreserveMemory(ptr);
                return BM_ERROR;
            }
        } else {
            BM_LOG_INFO("current_base: " << std::hex << currentBase << ", size: " << chunk);
        }
        reserved += chunk;
        chunkMaps.push_back(currentBase);
    }
    *address = (*chunkMaps.begin());
    return BM_OK;
}

Result HybmDevLegacySegment::ReserveMemorySpace(void **address) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(ValidateOptions() == BM_OK, "Failed to validate options.", BM_INVALID_PARAM);
    if (globalVirtualAddress_ != nullptr) {
        BM_LOG_ERROR("already prepare virtual memory.");
        return BM_ERROR;
    }
    BM_ASSERT_RETURN(address != nullptr, BM_INVALID_PARAM);

    uint64_t base = 0;
    totalVirtualSize_ = options_.rankCnt * options_.size;
    auto ret = GvaReserveMemory(&base, totalVirtualSize_, logicDeviceId_, 0ULL, options_.size);
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

Result HybmDevLegacySegment::UnReserveMemorySpace() noexcept
{
    BM_LOG_INFO("un-reserve memory space.");
    FreeMemory();
    return BM_OK;
}

Result HybmDevLegacySegment::AllocLocalMemory(uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept
{
    if ((size % HYBM_LARGE_PAGE_SIZE) != 0UL || size + allocatedSize_ > options_.size) {
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
                                       reinterpret_cast<uint64_t>(sliceAddr), size);
    slices_.emplace(slice->index_, slice);
    BM_LOG_DEBUG("allocate slice(idx:" << slice->index_ << ", size:" << slice->size_ << ").");

    return BM_OK;
}

Result HybmDevLegacySegment::RegisterMemory(const void *addr, uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept
{
    slice = std::make_shared<MemSlice>(sliceCount_++, MEM_TYPE_DEVICE_HBM, MEM_PT_TYPE_SVM,
                                       reinterpret_cast<uint64_t>(addr), size);
    slices_.emplace(slice->index_, slice);
    return BM_OK;
}

Result HybmDevLegacySegment::ReleaseSliceMemory(const std::shared_ptr<MemSlice> &slice) noexcept
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

    // If memory in range, va is allocated from memory pool, HalGvaFree should be called
    // If memory is not in range, va is register from user local device
    if (MemoryInRange(reinterpret_cast<void *>(slice->vAddress_), slice->size_)) {
        auto res = drv::HalGvaFree(slice->vAddress_, slice->size_);
        BM_LOG_INFO("free slice(idx:" << slice->index_ << "), size: " << slice->size_ << " return:" << res);
    }

    slices_.erase(pos);
    return BM_OK;
}

Result HybmDevLegacySegment::Export(std::string &exInfo) noexcept
{
    return BM_OK;
}

// export不可重入
Result HybmDevLegacySegment::Export(const std::shared_ptr<MemSlice> &slice, std::string &exInfo) noexcept
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

    HbmExportInfo info{};
    auto ret = DlAclApi::RtIpcSetMemoryName((void *)(ptrdiff_t)slice->vAddress_, slice->size_, info.shmName,
                                            sizeof(info.shmName));
    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "set memory name failed: " << ret);

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

Result HybmDevLegacySegment::GetExportSliceSize(size_t &size) noexcept
{
    size = sizeof(HbmExportInfo);
    return BM_OK;
}

// import可重入
Result HybmDevLegacySegment::Import(const std::vector<std::string> &allExInfo, void *addresses[]) noexcept
{
    std::map<uint16_t, HbmExportInfo> importMap;
    LiteralExInfoTranslater<HbmExportInfo> translator;
    std::vector<HbmExportInfo> desInfos(allExInfo.size());
    for (auto i = 0U; i < allExInfo.size(); i++) {
        auto ret = translator.Deserialize(allExInfo[i], desInfos[i]);
        if (ret != 0) {
            BM_LOG_ERROR("deserialize imported info(" << i << ") failed.");
            return BM_INVALID_PARAM;
        }
        importMap.emplace(desInfos[i].rankId, desInfos[i]);
    }
    importMap_ = std::move(importMap);

    uint32_t localIdx = UINT32_MAX;
    for (auto i = 0U; i < desInfos.size(); i++) {
        if (desInfos[i].magic != HBM_SLICE_EXPORT_INFO_MAGIC) {
            BM_LOG_ERROR("import info(" << i << ") magic(" << desInfos[i].magic << ") invalid.");
            return BM_INVALID_PARAM;
        }

        if (desInfos[i].rankId == options_.rankId) {
            localIdx = i;
        }
    }
    BM_ASSERT_RETURN(localIdx < desInfos.size(), BM_INVALID_PARAM);

    for (auto i = 0U; i < desInfos.size(); i++) {
        if (desInfos[i].rankId == options_.rankId) {
            continue;
        }

        if (CanLocalHostReaches(desInfos[i].superPodId, desInfos[i].serverId, desInfos[i].logicDeviceId)) {
            auto ret = DlAclApi::RtEnableP2P(deviceId_, desInfos[i].logicDeviceId, 0);
            if (ret != 0) {
                BM_LOG_ERROR("enable device access failed:" << ret << " local_device:" << deviceId_
                                                            << " remote_device:" << (int)desInfos[i].deviceId
                                                            << " logic_device:" << logicDeviceId_
                                                            << " remote_logic_device:" << desInfos[i].logicDeviceId);
                return BM_DL_FUNCTION_FAILED;
            }
        }

        if (!CanSdmaReaches(desInfos[i].superPodId, desInfos[i].serverId, desInfos[i].logicDeviceId)) {
            continue;
        }

        auto ret =
            DlAclApi::RtSetIpcMemorySuperPodPid(desInfos[localIdx].shmName, desInfos[i].sdid, &desInfos[i].pid, 1);
        if (ret != 0) {
            BM_LOG_ERROR("enable white list for rank(" << desInfos[i].rankId << ") failed: " << ret
                                                       << ", local rank = " << options_.rankId
                                                       << ", shmName=" << desInfos[localIdx].shmName);
            return BM_DL_FUNCTION_FAILED;
        }
    }

    return SafeCopy(desInfos.begin(), desInfos.end(), std::back_inserter(imports_));
}

Result HybmDevLegacySegment::Mmap() noexcept
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

        if (!CanSdmaReaches(im.superPodId, im.serverId, im.logicDeviceId)) {
            continue;
        }

        BM_LOG_DEBUG("remote slice on rank(" << im.rankId << ") should map to" << ", size = " << im.size);
        auto ret = drv::HalGvaOpen((uint64_t)remoteAddress, im.shmName, im.size, 0);
        if (ret != BM_OK) {
            BM_LOG_ERROR("HalGvaOpen memory failed:" << ret);
            return -1;
        }
        mappedMem_.insert((uint64_t)remoteAddress);
    }
    imports_.clear();
    return BM_OK;
}

Result HybmDevLegacySegment::Unmap() noexcept
{
    for (auto va : mappedMem_) {
        (void)drv::HalGvaClose(va, 0);
    }
    mappedMem_.clear();

    return 0;
}

Result HybmDevLegacySegment::RemoveImported(const std::vector<uint32_t> &ranks) noexcept
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

std::shared_ptr<MemSlice> HybmDevLegacySegment::GetMemSlice(hybm_mem_slice_t slice) const noexcept
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

bool HybmDevLegacySegment::MemoryInRange(const void *begin, uint64_t size) const noexcept
{
    if (begin < globalVirtualAddress_) {
        return false;
    }

    if (reinterpret_cast<const uint8_t *>(begin) + size > globalVirtualAddress_ + totalVirtualSize_) {
        return false;
    }

    return true;
}

void HybmDevLegacySegment::FreeMemory() noexcept
{
    while (!slices_.empty()) {
        auto slice = slices_.begin()->second.slice;
        ReleaseSliceMemory(slice);
    }

    allocatedSize_ = 0;
    sliceCount_ = 0;
    if (globalVirtualAddress_ != nullptr) {
        auto ret =
            GvaUnreserveMemory(reinterpret_cast<uint64_t>(globalVirtualAddress_), totalVirtualSize_, options_.size);
        if (ret != 0) {
            BM_LOG_ERROR("HalGvaUnreserveMemory failed: " << ret);
        }
        globalVirtualAddress_ = nullptr;
    }
}

void HybmDevLegacySegment::GetDeviceInfo(uint32_t &sdId, uint32_t &serverId, uint32_t &superPodId) noexcept
{
    sdId = sdid_;
    serverId = serverId_;
    superPodId = superPodId_;
}

bool HybmDevLegacySegment::GetRankIdByAddr(const void *addr, uint64_t size, uint32_t &rankId) const noexcept
{
    if (!MemoryInRange(addr, size)) {
        rankId = options_.rankId;
        return false;
    } else {
        uint64_t offset = static_cast<const uint8_t *>(addr) - static_cast<const uint8_t *>(globalVirtualAddress_);
        rankId = offset / options_.size;
        return true;
    }
}

bool HybmDevLegacySegment::CheckSmdaReaches(uint32_t rankId) const noexcept
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
} // namespace mf
} // namespace ock