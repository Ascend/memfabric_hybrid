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
#include "hybm_va_manager.h"
#include <sstream>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <set>

namespace ock {
namespace mf {
static constexpr uint64_t MB_OFFSET = 20UL;
Result HybmVaManager::Initialize(AscendSocType socType) noexcept
{
#if defined(ASCEND_NPU)
    if (socType == AscendSocType::ASCEND_UNKNOWN) {
        BM_LOG_ERROR("soc type is unknown.");
        return BM_INVALID_PARAM;
    }
#endif
    return BM_OK;
}

Result HybmVaManager::AddVaInfo(const AllocatedGvaInfo &info)
{
    if (info.base.gva == 0) {
        BM_LOG_ERROR("AddVaInfo failed: invalid parameters. gva=" << VaToStr(info.base.gva)
                                                                  << ",  lva=" << VaToStr(info.base.lva)
                                                                  << ", size=" << VaToStr(info.base.size));
        return BM_ERROR;
    }
    if (info.base.size == 0) {
        BM_LOG_INFO("gva:" << info.base.gva  << " size:0, skip add va info");
        return BM_OK;
    }
    if (info.base.memType >= HYBM_MEM_TYPE_BUTT) {
        BM_LOG_ERROR("AddVaInfo failed: invalid memType=" << info.base.memType);
        return BM_ERROR;
    }
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto old = CheckOverlap(info.base.gva, info.base.size, info.base.memType);
    if (old.first && old.second != info) {
        BM_LOG_ERROR("AddVaInfo failed: address overlap. gva="
                     << VaToStr(info.base.gva) << ", size=" << VaToStr(info.base.size)
                     << ", localRankId=" << info.RankId() << ", importedRankId=" << info.RankId()
                     << ", memType=" << info.base.memType << ", old: " << old.second);
        return BM_ERROR;
    }
    if (old.second == info) {
        return BM_OK;
    }
    allocatedLookupMapByGva_[info.base.gva] = info;
    allocatedLookupMapByLva_[info.base.lva] = info;
    allocatedLookupMapByMemType_[info.base.memType].push_back(info);
    return BM_OK;
}

Result HybmVaManager::AddVaInfoFromExternal(const BaseAllocatedGvaInfo &baseInfo, uint32_t localRankId)
{
    return AddVaInfoFromExternal(baseInfo, localRankId, INVALID_RANK_ID);
}

Result HybmVaManager::AddVaInfoFromExternal(const BaseAllocatedGvaInfo &baseInfo, uint32_t localRankId,
                                            uint32_t importedRankId)
{
    AllocatedGvaInfo info(baseInfo.gva, baseInfo.size, localRankId, importedRankId, baseInfo.memType, baseInfo.lva);
    info.registered = importedRankId == INVALID_RANK_ID;
    const auto ret = AddVaInfo(info);
    BM_ASSERT_RETURN(ret == BM_OK, ret);
    BM_LOG_INFO("AddVaInfoFromExternal success: " << info);
    return BM_OK;
}

Result HybmVaManager::AddVaInfo(const BaseAllocatedGvaInfo &baseInfo, uint32_t localRankId)
{
    AllocatedGvaInfo info(baseInfo.gva, baseInfo.size, localRankId, baseInfo.memType, baseInfo.lva);
    const auto ret = AddVaInfo(info);
    BM_ASSERT_RETURN(ret == BM_OK, ret);
    BM_LOG_INFO("AddVaInfo success: " << info);
    return BM_OK;
}

void HybmVaManager::RemoveOneVaInfo(uint64_t va)
{
    auto gva = va;
    if (!IsGva(va)) {
        gva = GetGvaByLva(va);
    }
    if (gva == 0) {
        return;
    }
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = allocatedLookupMapByGva_.find(gva);
    if (it == allocatedLookupMapByGva_.end()) {
        BM_LOG_WARN("RemoveOneVaInfo failed: address not found. gva=" << VaToStr(gva));
        return;
    }
    const AllocatedGvaInfo &info = it->second;
    BM_LOG_DEBUG("Removing VaInfo: " << info);
    auto lvaIt = allocatedLookupMapByLva_.find(info.base.lva);
    if (lvaIt != allocatedLookupMapByLva_.end() && lvaIt->second.base.gva == gva) {
        allocatedLookupMapByLva_.erase(lvaIt);
    }
    auto typeIt = allocatedLookupMapByMemType_.find(info.base.memType);
    if (typeIt != allocatedLookupMapByMemType_.end()) {
        auto &typeList = typeIt->second;
        typeList.erase(std::remove_if(typeList.begin(), typeList.end(),
                                      [gva](const AllocatedGvaInfo &item) { return item.base.gva == gva; }),
                       typeList.end());
        if (typeList.empty()) {
            allocatedLookupMapByMemType_.erase(typeIt);
        }
    }
    allocatedLookupMapByGva_.erase(it);
    BM_LOG_INFO("RemoveOneVaInfo success: gva=" << VaToStr(gva));
}

uint64_t HybmVaManager::GetGvaByLva(uint64_t lva)
{
    BM_ASSERT_RETURN(lva > 0, 0);
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = allocatedLookupMapByLva_.upper_bound(lva);
    if (it != allocatedLookupMapByLva_.begin()) {
        --it;
        if (it->second.ContainsByLva(lva)) {
            // Calculate the corresponding GVA
            uint64_t offset = lva - it->second.base.lva;
            uint64_t gva = it->second.base.gva + offset;
            BM_LOG_DEBUG("GetGvaByLva range mapping: lva=" << VaToStr(lva) << " -> gva=" << VaToStr(gva)
                                                           << " (offset=" << VaToStr(offset) << ")");
            return gva;
        }
    }
    BM_LOG_DEBUG("GetGvaByLva: no mapping found for lva=" << VaToStr(lva));
    return 0;
}

uint64_t HybmVaManager::GetLvaByGva(uint64_t gva)
{
    BM_ASSERT_RETURN(gva > 0, 0);
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = allocatedLookupMapByGva_.upper_bound(gva);
    if (it != allocatedLookupMapByGva_.begin()) {
        --it;
        if (it->second.Contains(gva)) {
            uint64_t offset = gva - it->second.base.gva;
            uint64_t lva = it->second.base.lva + offset;
            BM_LOG_DEBUG("GetLvaByGva: gva=" << VaToStr(gva) << " -> lva=" << VaToStr(lva)
                                             << " (offset=" << VaToStr(offset) << ")");
            return lva;
        }
    }
    BM_LOG_DEBUG("GetLvaByGva: no mapping found for gva=" << VaToStr(gva));
    return 0;
}

bool HybmVaManager::IsGva(uint64_t va)
{
    BM_ASSERT_RETURN(va > 0, false);
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = allocatedLookupMapByGva_.upper_bound(va);
    if (it != allocatedLookupMapByGva_.begin()) {
        --it;
    }
    if (it->second.Contains(va)) {
        BM_LOG_DEBUG("IsGva: va=" << VaToStr(va) << " is a valid GVA");
        return true;
    }
    BM_LOG_DEBUG("IsGva: va=" << VaToStr(va) << " is not a valid GVA");
    return false;
}

hybm_mem_type HybmVaManager::GetMemType(uint64_t va)
{
    BM_ASSERT_RETURN(va > 0, HYBM_MEM_TYPE_DEVICE);
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto allocIt = allocatedLookupMapByGva_.upper_bound(va);
    if (allocIt != allocatedLookupMapByGva_.begin()) {
        --allocIt;
    }
    if (allocIt->second.Contains(va)) {
        BM_LOG_DEBUG("GetMemType: va=" << VaToStr(va) << " memType=" << allocIt->second.base.memType);
        return allocIt->second.base.memType;
    }
    auto allocItL = allocatedLookupMapByLva_.upper_bound(va);
    if (allocItL != allocatedLookupMapByLva_.begin()) {
        --allocItL;
    }
    if (allocItL->second.ContainsByLva(va)) {
        BM_LOG_DEBUG("GetMemType: va=" << VaToStr(va) << " memType=" << allocIt->second.base.memType);
        return allocIt->second.base.memType;
    }
    auto reservedIt = reservedLookupMapByGva_.upper_bound(va);
    if (reservedIt != reservedLookupMapByGva_.begin()) {
        --reservedIt;
    }
    if (reservedIt->second.Contains(va)) {
        BM_LOG_DEBUG("GetMemType: va=" << VaToStr(va) << " memType=" << reservedIt->second.memType << " (reserved)");
        return reservedIt->second.memType;
    }
    BM_LOG_DEBUG("GetMemType: va=" << VaToStr(va) << " not found, returning default type");
    return HYBM_MEM_TYPE_BUTT;
}

std::pair<uint32_t, bool> HybmVaManager::GetRank(uint64_t gva)
{
    if (gva == 0) {
        BM_LOG_WARN("GetRank: va=0 is invalid");
        return {0, false};
    }
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = allocatedLookupMapByGva_.upper_bound(gva);
    if (it != allocatedLookupMapByGva_.begin()) {
        --it;
    }
    if (it->second.Contains(gva)) {
        BM_LOG_DEBUG("GetRank: va=" << VaToStr(gva) << " localRankId=" << it->second.RankId());
        return {it->second.RankId(), true};
    }
    BM_LOG_DEBUG("GetRank: va=" << VaToStr(gva) << " not found");
    return {0, false};
}

bool HybmVaManager::IsValidAddr(uint64_t va)
{
    bool isValid = IsGva(va) || (GetGvaByLva(va) != 0);
    BM_LOG_DEBUG("IsValidAddr: va=" << VaToStr(va) << " is " << (isValid ? "valid" : "invalid"));
    return isValid;
}

ReservedGvaInfo HybmVaManager::AllocReserveGva(uint32_t localRankId, uint64_t size, hybm_mem_type memType)
{
    ReservedGvaInfo result;
    if (size == 0) {
        BM_LOG_ERROR("AllocReserveGva failed: size=0");
        return result;
    }
    std::unique_lock<std::shared_mutex> lock(mutex_);
    uint64_t startAddr = reserveStart_;
    uint64_t endAddr = reserveEnd_;
    BM_LOG_DEBUG("AllocReserveGva: searching in HOST range " << VaToStr(startAddr) << "-" << VaToStr(endAddr));
    // Find free space
    auto [freeAddr, found] = FindFreeSpace(startAddr, endAddr, size);
    if (!found) {
        BM_LOG_ERROR("AllocReserveGva failed: no free space found for size=" << VaToStr(size));
        return result;
    }
    result.start = freeAddr;
    result.size = size;
    result.memType = memType;
    result.localRankId = localRankId;
    reservedLookupMapByGva_[freeAddr] = result;
    BM_LOG_INFO("AllocReserveGva success: " << result);
    return result;
}

void HybmVaManager::FreeReserveGva(uint64_t addr)
{
    if (addr == 0) {
        BM_LOG_WARN("FreeReserveGva failed: invalid addr=0");
        return;
    }
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = reservedLookupMapByGva_.find(addr);
    if (it == reservedLookupMapByGva_.end()) {
        BM_LOG_WARN("FreeReserveGva failed: reserved space not found at addr=" << VaToStr(addr));
        return;
    }
    const ReservedGvaInfo &info = it->second;
    BM_LOG_INFO("FreeReserveGva: " << info);
    reservedLookupMapByGva_.erase(it);
    BM_LOG_DEBUG("FreeReserveGva success: addr=" << VaToStr(addr));
}

void HybmVaManager::DumpReservedGvaInfo() const
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    BM_LOG_DEBUG("Total reserved spaces: " << reservedLookupMapByGva_.size());
    if (reservedLookupMapByGva_.empty()) {
        BM_LOG_WARN("No reserved spaces found.");
        return;
    }
    int index = 1;
    uint64_t totalSizeMB = 0;
    for (const auto &pair : reservedLookupMapByGva_) {
        const ReservedGvaInfo &info = pair.second;
        BM_LOG_DEBUG(index << ". " << info);
        totalSizeMB += (info.size >> MB_OFFSET);
        index++;
    }
    BM_LOG_DEBUG("Total reserved size: " << totalSizeMB << "MB");
}

void HybmVaManager::DumpAllocatedGvaInfo() const
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    BM_LOG_DEBUG("Total allocated spaces: " << allocatedLookupMapByGva_.size());
    if (allocatedLookupMapByGva_.empty()) {
        BM_LOG_WARN("No allocated spaces found.");
        return;
    }
    int index = 1;
    uint64_t totalSizeMB = 0;
    for (const auto &pair : allocatedLookupMapByGva_) {
        const AllocatedGvaInfo &info = pair.second;
        BM_LOG_DEBUG(index << ". " << info);
        totalSizeMB += (info.base.size >> MB_OFFSET);
        index++;
    }
    BM_LOG_DEBUG("Total allocated size: " << totalSizeMB << "MB");
    std::map<hybm_mem_type, uint64_t> sizeByType;
    std::map<hybm_mem_type, size_t> countByType;
    for (const auto &pair : allocatedLookupMapByGva_) {
        const AllocatedGvaInfo &info = pair.second;
        sizeByType[info.base.memType] += (info.base.size >> MB_OFFSET);
        countByType[info.base.memType]++;
    }
    for (const auto &pair : sizeByType) {
        hybm_mem_type memType = pair.first;
        uint64_t sizeMB = pair.second;
        size_t count = countByType[memType];
        BM_LOG_DEBUG(memType << ": " << count << " allocations, total size: " << sizeMB << "MB");
    }
}

std::pair<bool, AllocatedGvaInfo> HybmVaManager::CheckOverlap(const uint64_t gva, const uint64_t size,
                                                              const hybm_mem_type memType)
{
    const uint64_t end = gva + size;
    auto typeIt = allocatedLookupMapByMemType_.find(memType);
    if (typeIt != allocatedLookupMapByMemType_.end()) {
        for (const auto &info : typeIt->second) {
            if ((gva >= info.base.gva && gva < info.End()) || (gva <= info.base.gva && end > info.base.gva)) {
                BM_LOG_INFO("CheckOverlap: overlap with allocated space " << info);
                return {true, info};
            }
        }
    }
    return {false, {}};
}

std::pair<uint64_t, bool> HybmVaManager::FindFreeSpace(uint64_t start, uint64_t end, uint64_t size) const
{
    if (size == 0 || start >= end || size > (end - start)) {
        BM_LOG_ERROR("FindFreeSpace: invalid parameters. start=" << VaToStr(start) << ", end=" << VaToStr(end)
                                                                 << ", size=" << VaToStr(size));
        return {0, false};
    }
    std::vector<std::pair<uint64_t, uint64_t>> usedRanges;
    for (const auto &pair : reservedLookupMapByGva_) {
        const ReservedGvaInfo &info = pair.second;
        if (info.start >= start && info.start < end) {
            usedRanges.emplace_back(info.start, info.End());
        }
    }
    BM_LOG_DEBUG("FindFreeSpace: found " << usedRanges.size() << " used ranges in target area");
    if (usedRanges.empty()) {
        BM_LOG_DEBUG("FindFreeSpace: no used ranges, using start address " << VaToStr(start));
        return {start, true};
    }
    std::sort(usedRanges.begin(), usedRanges.end());
    uint64_t current = start;
    for (const auto &range : usedRanges) {
        if (range.first > current) {
            uint64_t freeSize = range.first - current;
            if (freeSize >= size) {
                BM_LOG_DEBUG("FindFreeSpace: found free space at " << VaToStr(current)
                                                                   << ", size=" << VaToStr(freeSize));
                return {current, true};
            }
            BM_LOG_DEBUG("FindFreeSpace: free space at " << VaToStr(current) << " too small (size=" << VaToStr(freeSize)
                                                         << ", needed=" << VaToStr(size) << ")");
        }
        if (range.second > current) {
            current = range.second;
        }
    }
    if (end - current >= size) {
        BM_LOG_DEBUG("FindFreeSpace: found free space at end, addr=" << VaToStr(current));
        return {current, true};
    }
    BM_LOG_ERROR("FindFreeSpace: no suitable free space found, size: " << VaToStr(size));
    return {0, false};
}

std::pair<AllocatedGvaInfo, bool> HybmVaManager::FindAllocByGva(uint64_t gva) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = allocatedLookupMapByGva_.upper_bound(gva);
    if (it != allocatedLookupMapByGva_.begin()) {
        --it;
    }
    if (it->second.Contains(gva)) {
        return {it->second, true};
    }
    return {AllocatedGvaInfo{}, false};
}

std::pair<AllocatedGvaInfo, bool> HybmVaManager::FindAllocByLva(uint64_t lva) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = allocatedLookupMapByLva_.upper_bound(lva);
    if (it != allocatedLookupMapByLva_.begin()) {
        --it;
    }
    if (it->second.ContainsByLva(lva)) {
        return {it->second, true};
    }
    return {AllocatedGvaInfo{}, false};
}

std::pair<ReservedGvaInfo, bool> HybmVaManager::FindReservedByAddr(uint64_t addr) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = reservedLookupMapByGva_.upper_bound(addr);
    if (it != reservedLookupMapByGva_.begin()) {
        --it;
    }
    if (it->second.Contains(addr)) {
        return {it->second, true};
    }
    return {ReservedGvaInfo{}, false};
}

size_t HybmVaManager::GetAllocCount() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    size_t count = allocatedLookupMapByGva_.size();
    BM_LOG_DEBUG("GetAllocCount: " << count);
    return count;
}

size_t HybmVaManager::GetReservedCount() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    size_t count = reservedLookupMapByGva_.size();
    BM_LOG_DEBUG("GetReservedCount: " << count);
    return count;
}

void HybmVaManager::ClearAll()
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    BM_LOG_DEBUG("Alloc count before clear: " << allocatedLookupMapByGva_.size());
    BM_LOG_DEBUG("Reserved count before clear: " << reservedLookupMapByGva_.size());
    allocatedLookupMapByGva_.clear();
    allocatedLookupMapByLva_.clear();
    reservedLookupMapByGva_.clear();
    allocatedLookupMapByMemType_.clear();
}
} // namespace mf
} // namespace ock