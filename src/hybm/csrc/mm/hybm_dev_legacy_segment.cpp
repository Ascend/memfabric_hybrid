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
#include "hybm_va_manager.h"

namespace ock {
namespace mf {
Result HybmDevLegacySegment::ValidateOptions() noexcept
{
    if (options_.segType != HYBM_MST_HBM || options_.maxSize == 0 || options_.devId < 0 ||
        (options_.maxSize % HYBM_LARGE_PAGE_SIZE) != 0) {
        BM_LOG_ERROR("Invalid options segType:" << options_.segType << " size:" << options_.maxSize);
        return BM_INVALID_PARAM;
    }

    if (UINT64_MAX / options_.maxSize < options_.rankCnt) {
        BM_LOG_ERROR("Validate options error rankCnt(" << options_.rankCnt << ") size(" << options_.maxSize);
        return BM_INVALID_PARAM;
    }
    return BM_OK;
}

// 这个函数的主要目的是根据 totalSize 和 singleRankSize 计算一个合适的块大小，确保块大小不超过128GB，并且能够被totalSize整除。
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
    *address = chunkMaps.back();
    return BM_OK;
}

Result HybmDevLegacySegment::ReserveMemorySpace(void **address) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(ValidateOptions() == BM_OK, "Failed to validate options.", BM_INVALID_PARAM);
    BM_ASSERT_LOG_AND_RETURN(globalVirtualAddress_ == nullptr, "Already prepare virtual memory.", BM_NOT_INITIALIZED);
    BM_ASSERT_LOG_AND_RETURN(address != nullptr, "Invalid param, address is NULL.", BM_INVALID_PARAM);
    BM_ASSERT_LOG_AND_RETURN(options_.rankId < options_.rankCnt,
                             "rank(" << options_.rankId << ") but total " << options_.rankCnt, BM_INVALID_PARAM);

    totalVirtualSize_ = options_.rankCnt * options_.maxSize;

    uint64_t base = 0;
    auto ret = GvaReserveMemory(&base, totalVirtualSize_, logicDeviceId_, 0ULL, options_.maxSize);
    if (ret != 0 || base == 0) {
        BM_LOG_ERROR("prepare virtual memory size(" << totalVirtualSize_ << ") failed. ret: " << ret);
        return BM_MALLOC_FAILED;
    }
    lvaBase_ = reinterpret_cast<uint8_t *>(base) + options_.maxSize * options_.rankId;
    if (options_.enable56BitsGva) {
        auto gvaInfo = HybmVaManager::GetInstance().AllocReserveGva(
            options_.rankId, totalVirtualSize_, totalVirtualSize_, HYBM_MEM_TYPE_DEVICE, options_.enable56BitsGva);
        globalVirtualAddress_ = (uint8_t *)reinterpret_cast<void *>(gvaInfo.va[HVM_GVA]);
    } else {
        globalVirtualAddress_ = reinterpret_cast<uint8_t *>(base);
    }

    allocatedSize_ = 0UL;
    sliceCount_ = 0;
    *address = globalVirtualAddress_;
    return BM_OK;
}

Result HybmDevLegacySegment::UnReserveMemorySpace() noexcept
{
    BM_LOG_INFO("un-reserve memory space.");
    FreeMemory();
    if (options_.enable56BitsGva) {
        HybmVaManager::GetInstance().FreeReserveGva((uintptr_t)globalVirtualAddress_);
    }
    return BM_OK;
}

Result HybmDevLegacySegment::AllocLocalMemory(uint64_t size, MemSlicePtr &slice) noexcept
{
    if ((size % HYBM_LARGE_PAGE_SIZE) != 0UL || size + allocatedSize_ > options_.maxSize) {
        BM_LOG_ERROR("invalid allocate memory size : " << size << ", now used " << allocatedSize_ << " of "
                                                       << options_.maxSize);
        return BM_INVALID_PARAM;
    }

    if (size > 0) {
        auto ret = drv::HalGvaAlloc((uint64_t)(lvaBase_ + allocatedSize_), size, 0);
        if (ret != BM_OK) {
            BM_LOG_ERROR("HalGvaAlloc memory failed: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }
    }

    auto sliceAddr = lvaBase_ + allocatedSize_;
    auto gva = reinterpret_cast<uint64_t>(globalVirtualAddress_ + options_.maxSize * options_.rankId + allocatedSize_);
    allocatedSize_ += size;
    slice = std::make_shared<MemSlice>(sliceCount_++, HYBM_MEM_TYPE_DEVICE, MEM_PT_TYPE_SVM, gva,
                                       reinterpret_cast<uint64_t>(sliceAddr), size);
    slices_.emplace(slice->index_, slice);
    if (size > 0) {
        auto ret = HybmVaManager::GetInstance().AddVaInfo(
            {gva, slice->vAddress_, slice->vAddress_, size, HYBM_MEM_TYPE_DEVICE}, options_.rankId);
        if (ret != 0) {
            BM_LOG_ERROR("AddVaInfo failed, size: " << size << " ret: " << ret);
            drv::HalGvaFree(slice->vAddress_, size);
            slices_.erase(slice->index_);
            return ret;
        }
    }
    BM_LOG_DEBUG("allocate slice(idx:" << slice->index_ << ", size:" << slice->size_ << ").");
    return BM_OK;
}

Result HybmDevLegacySegment::RegisterMemory(const void *addr, uint64_t size, MemSlicePtr &slice) noexcept
{
    auto ret = RegisterMemCommon(addr, size, slice);
    BM_ASSERT_RETURN(ret == BM_OK, ret);
    auto [it, inserted] = slices_.try_emplace(slice->index_, slice);
    if (!inserted) {
        BM_LOG_WARN("RegisterMemory: slices_ index collision (likely sliceCount_ wrap-around), "
                    "overwriting old entry. index=" << slice->index_
                    << " new_addr=0x" << std::hex << slice->vAddress_
                    << " old_addr=0x" << it->second.slice->vAddress_ << std::dec);
        it->second = MemSliceStatus(slice);
    }
    return BM_OK;
}

Result HybmDevLegacySegment::ReleaseSliceMemory(const MemSlicePtr &slice) noexcept
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
    HybmVaManager::GetInstance().RemoveOneVaInfo(slice->vAddress_, HVM_DVA);

    slices_.erase(pos);
    exportMap_.erase(slice->index_);
    return BM_OK;
}

Result HybmDevLegacySegment::Export(std::string &exInfo) noexcept
{
    return BM_OK;
}

// export不可重入
Result HybmDevLegacySegment::Export(const MemSlicePtr &slice, std::string &exInfo) noexcept
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
        LiteralExInfoTranslater<HbmExportInfo>{}.Serialize(exp->second, exInfo);
        return BM_OK;
    }

    HbmExportInfo info{};
    if (slice->size_ > 0) {
        auto ret = DlAclApi::RtIpcSetMemoryName((void *)(ptrdiff_t)slice->vAddress_, slice->size_, info.shmName,
                                                sizeof(info.shmName));
        BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "set memory name failed: " << ret << " addr:" << std::hex
                                                                         << slice->vAddress_
                                                                         << " size:" << slice->size_);
    }

    info.gva = slice->gva_;
    info.deviceVa = slice->vAddress_;
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
    auto ret = LiteralExInfoTranslater<HbmExportInfo>{}.Serialize(info, exInfo);
    if (ret != BM_OK) {
        BM_LOG_ERROR("export info failed: " << ret);
        DlAclApi::RtIpcDestroyMemoryName(info.shmName);
        return BM_ERROR;
    }

    exportMap_[slice->index_] = info;
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
    std::map<uint32_t, HbmExportInfo> importMap;
    LiteralExInfoTranslater<HbmExportInfo> translator;
    std::vector<HbmExportInfo> desInfos{};
    for (auto i = 0U; i < allExInfo.size(); i++) {
        HbmExportInfo info{};
        auto ret = translator.Deserialize(allExInfo[i], info);
        if (ret != 0) {
            BM_LOG_ERROR("deserialize imported info(" << i << ") failed.");
            return BM_INVALID_PARAM;
        }
        if (info.magic != HBM_SLICE_EXPORT_INFO_MAGIC) {
            BM_LOG_INFO("import rank(" << info.rankId << ") magic(" << info.magic << ") invalid skip it.");
            continue;
        }
        if (info.rankId == options_.rankId) {
            continue;
        }
        importMap.emplace(info.rankId, info);
        desInfos.push_back(std::move(info));
    }
    importMap_ = std::move(importMap);

    for (auto i = 0U; i < desInfos.size(); i++) {
        if (CanLocalHostReaches(desInfos[i].superPodId, desInfos[i].serverId, desInfos[i].logicDeviceId) &&
            logicDeviceId_ != static_cast<int>(desInfos[i].logicDeviceId)) { // 应当用logic id判断是否需要p2p
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
            desInfos[i].deviceVa = 0;
            continue;
        }

        if (options_.size == 0) {
            continue;
        }
        for (auto &local : exportMap_) {
            auto ret = DlAclApi::RtSetIpcMemorySuperPodPid(local.second.shmName, desInfos[i].sdid,
                                                           &desInfos[i].pid, 1);
            if (ret != 0) {
                BM_LOG_ERROR("enable white list for rank(" << desInfos[i].rankId << ") failed: " << ret
                                                           << ", local rank = " << options_.rankId
                                                           << ", shmName=" << local.second.shmName);
                return BM_DL_FUNCTION_FAILED;
            }
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

        if (im.size == 0) {
            BM_LOG_INFO("mmap rank(" << im.rankId << ") size(" << im.size << ") invalid skip it.");
            continue;
        }

        if (mappedGvaMem_.find(im.gva) != mappedGvaMem_.end()) {
            BM_LOG_INFO("remote slice on rank(" << im.rankId << ") has maped");
            continue;
        }

        if (!CanSdmaReaches(im.superPodId, im.serverId, im.logicDeviceId)) {
            continue;
        }

        BM_LOG_INFO("remote slice on rank(" << im.rankId << ") should map gva:0x" << std::hex << (void *)im.gva
                                            << "map deviceVa:0x" << (void *)im.deviceVa << ", size:" << std::dec
                                            << im.size);
        auto ret = drv::HalGvaOpen(im.deviceVa, im.shmName, im.size, 0);
        if (ret != BM_OK) {
            BM_LOG_ERROR("HalGvaOpen memory failed:" << ret);
            return -1;
        }
        mappedGvaMem_.insert(im.gva);

        // .host_va use info.deviceVa, because .host_va is the part of key for hybm_va_manager allocatedLookupMapByLva_
        ret = HybmVaManager::GetInstance().AddVaInfoFromExternal(
            {im.gva, im.deviceVa, im.deviceVa, im.size, HYBM_MEM_TYPE_DEVICE}, options_.rankId, im.rankId);
        BM_ASSERT_RETURN(ret == BM_OK, ret);
    }
    imports_.clear();
    return BM_OK;
}

Result HybmDevLegacySegment::Unmap() noexcept
{
    for (auto gva : mappedGvaMem_) {
        auto deviceVa = HybmVaManager::GetInstance().TransformVa(gva, HVM_GVA, HVM_DVA);
        (void)drv::HalGvaClose(deviceVa, 0);
        HybmVaManager::GetInstance().RemoveOneVaInfo(gva);
    }
    mappedGvaMem_.clear();

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
        uint64_t gvaLocal = reinterpret_cast<uint64_t>(globalVirtualAddress_) + options_.maxSize * rank;
        auto it = mappedGvaMem_.lower_bound(gvaLocal);
        auto st = it;
        while (it != mappedGvaMem_.end() && (*it) < gvaLocal + options_.maxSize) {
            auto deviceVa = HybmVaManager::GetInstance().TransformVa((*it), HVM_GVA, HVM_DVA);
            (void)drv::HalGvaClose(deviceVa, 0);
            HybmVaManager::GetInstance().RemoveOneVaInfo(deviceVa);
            it++;
        }

        if (st != it) {
            mappedGvaMem_.erase(st, it);
        }
    }

    // remove imports_ infos for specified ranks
    imports_.erase(std::remove_if(imports_.begin(), imports_.end(),
                                  [&ranks](const HbmExportInfo &info) {
                                      return std::find(ranks.begin(), ranks.end(), info.rankId) != ranks.end();
                                  }),
                   imports_.end());
    return 0;
}

MemSlicePtr HybmDevLegacySegment::GetMemSlice(hybm_mem_slice_t slice, bool quiet) const noexcept
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
    Unmap();

    allocatedSize_ = 0;
    sliceCount_ = 0;
    if (globalVirtualAddress_ != nullptr) {
        auto ret =
            GvaUnreserveMemory(reinterpret_cast<uint64_t>(globalVirtualAddress_), totalVirtualSize_, options_.maxSize);
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

bool HybmDevLegacySegment::CheckSdmaReaches(uint32_t rankId) const noexcept
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
