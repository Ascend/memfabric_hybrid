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
    if (info.base.size == 0) {
        BM_LOG_INFO("gva:" << info.base.va[HVM_GVA] << " size:0, skip add va info");
        return BM_OK;
    }
    if (info.base.memType >= HYBM_MEM_TYPE_BUTT) {
        BM_LOG_ERROR("AddVaInfo failed: invalid memType=" << info.base.memType);
        return BM_ERROR;
    }
    std::unique_lock<std::shared_mutex> lock(mutex_);
    for (uint32_t i = 0; i < HVM_BUTT; i++) {
        if (info.base.va[i] != 0) {
            auto old = CheckOverlap(info.base.va[i], info.base.size, i);
            if (old.first && old.second != info) {
                BM_LOG_ERROR("AddVaInfo failed: address overlap. gva="
                             << VaToStr(info.base.va[0]) << " va:" << VaToStr(info.base.va[i])
                             << ", size=" << VaToStr(info.base.size) << ", vaType=" << i
                             << ", localRankId=" << info.localRankId << ", importedRankId=" << info.importedRankId
                             << ", memType=" << info.base.memType << ", old: " << old.second);
                return BM_ERROR;
            }
        }
    }

    for (uint32_t i = 0; i < HVM_BUTT; i++) {
        if (info.base.va[i] != 0) {
            allocatedMap_[i][info.base.va[i]] = info;
        }
    }
    return BM_OK;
}

Result HybmVaManager::AddVaInfoFromExternal(const BaseAllocatedGvaInfo &baseInfo, uint32_t localRankId)
{
    return AddVaInfoFromExternal(baseInfo, localRankId, INVALID_RANK_ID);
}

Result HybmVaManager::AddVaInfoFromExternal(const BaseAllocatedGvaInfo &baseInfo, uint32_t localRankId,
                                            uint32_t importedRankId)
{
    AllocatedGvaInfo info(baseInfo, localRankId, importedRankId);
    const auto ret = AddVaInfo(info);
    BM_ASSERT_RETURN(ret == BM_OK, ret);
    BM_LOG_INFO("AddVaInfoFromExternal success: " << info);
    return BM_OK;
}

Result HybmVaManager::AddVaInfo(const BaseAllocatedGvaInfo &baseInfo, uint32_t localRankId)
{
    AllocatedGvaInfo info(baseInfo, localRankId);
    const auto ret = AddVaInfo(info);
    BM_ASSERT_RETURN(ret == BM_OK, ret);
    BM_LOG_INFO("AddVaInfo success: " << info);
    return BM_OK;
}

void HybmVaManager::RemoveOneVaInfo(uint64_t va, uint32_t type)
{
    if (va == 0 || type >= HVM_BUTT) {
        return;
    }
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = allocatedMap_[type].find(va);
    if (it == allocatedMap_[type].end()) {
        BM_LOG_WARN("RemoveOneVaInfo failed: address not found. va=" << VaToStr(va) << " type=" << type);
        return;
    }
    const AllocatedGvaInfo &info = it->second;
    BM_LOG_DEBUG("Removing VaInfo: " << info);
    for (uint32_t i = 0; i < HVM_BUTT; i++) {
        if (i == type) {
            continue;
        }
        auto its = allocatedMap_[i].find(info.base.va[i]);
        if (its != allocatedMap_[i].end() && its->second.base.va[i] == info.base.va[i]) {
            allocatedMap_[i].erase(its);
        }
    }
    allocatedMap_[type].erase(it);
    BM_LOG_INFO("RemoveOneGvaInfo success: gva=" << VaToStr(va) << " type=" << type);
}

uint64_t HybmVaManager::TransformVa(uint64_t va, uint32_t inputType, uint32_t outputType)
{
    BM_VALIDATE_RETURN(va > 0 && inputType < HVM_BUTT && outputType < HVM_BUTT,
                       "input is invalid, va=" << VaToStr(va) << " Itype=" << inputType << " Otype=" << outputType, 0);

    if (inputType == outputType) {
        return va;
    }

    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (allocatedMap_[inputType].empty()) {
        BM_LOG_WARN("No allocated spaces found.");
        return 0;
    }
    auto it = allocatedMap_[inputType].upper_bound(va);
    if (it != allocatedMap_[inputType].begin()) {
        --it;
        if (it->second.Contains(va, inputType) && it->second.base.va[outputType] != 0) {
            // Calculate the corresponding GVA
            uint64_t offset = va - it->second.base.va[inputType];
            uint64_t outputVa = it->second.base.va[outputType] + offset;
            BM_LOG_DEBUG("GetGvaByLva range mapping: input_va=" << VaToStr(va) << " -> output_va=" << VaToStr(outputVa)
                                                                << " (offset=" << VaToStr(offset) << ")");
            return outputVa;
        }
    }
    BM_LOG_DEBUG("TransformVa: no mapping found for lva=" << VaToStr(va));
    return 0;
}

hybm_mem_type HybmVaManager::GetGvaMemType(uint64_t va)
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (allocatedMap_[HVM_GVA].empty()) {
        return HYBM_MEM_TYPE_BUTT;
    }
    auto it = allocatedMap_[HVM_GVA].upper_bound(va);
    if (it != allocatedMap_[HVM_GVA].begin()) {
        --it;
    }
    if (it->second.Contains(va, HVM_GVA)) {
        BM_LOG_DEBUG("GetMemType: va=" << VaToStr(va) << " memType=" << it->second.base.memType);
        return it->second.base.memType;
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
    if (allocatedMap_[HVM_GVA].empty()) {
        BM_LOG_WARN("No allocated spaces found.");
        return {0, false};
    }
    auto it = allocatedMap_[HVM_GVA].upper_bound(gva);
    if (it != allocatedMap_[HVM_GVA].begin()) {
        --it;
    }
    if (it->second.Contains(gva, HVM_GVA)) {
        BM_LOG_DEBUG("GetRank: va=" << VaToStr(gva) << " localRankId=" << it->second.RankId());
        return {it->second.RankId(), true};
    }
    BM_LOG_DEBUG("GetRank: va=" << VaToStr(gva) << " not found");
    return {0, false};
}

ock::mf::HybmVaManager::AddressCategory ock::mf::HybmVaManager::ClassifyAddress(uint64_t va)
{
    auto gvaType = GetGvaMemType(va);
    if (gvaType != HYBM_MEM_TYPE_BUTT) {
        if (gvaType == HYBM_MEM_TYPE_DEVICE)
            return GLOBAL_DEVICE;
        if (gvaType == HYBM_MEM_TYPE_HOST)
            return GLOBAL_HOST;
    }
    if (va >= HYBM_DEVICE_VA_START && va < (HYBM_DEVICE_VA_START + HYBM_DEVICE_VA_SIZE)) {
        return LOCAL_DEVICE;
    }
    return LOCAL_HOST;
}

hybm_data_copy_direction HybmVaManager::InferCopyDirection(uint64_t srcVa, uint64_t dstVa)
{
    auto src = static_cast<int>(ClassifyAddress(srcVa));
    auto dst = static_cast<int>(ClassifyAddress(dstVa));
    if (src >= 0 && src < ADDRESS_CATEGORY_BUTT && dst >= 0 && dst < ADDRESS_CATEGORY_BUTT) {
        return COPY_DIRECTION_TABLE[src][dst];
    }
    return HYBM_DATA_COPY_DIRECTION_BUTT;
}

bool HybmVaManager::IsValidAddr(uint64_t va)
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (uint32_t i = 0; i < HVM_BUTT; i++) {
        if (allocatedMap_[i].empty()) {
            BM_LOG_WARN("No allocated spaces found.");
            continue;
        }
        auto it = allocatedMap_[i].upper_bound(va);
        if (it != allocatedMap_[i].begin()) {
            --it;
        }
        if (it->second.Contains(va, i)) {
            BM_LOG_DEBUG("GetMemType: va=" << VaToStr(va) << " vaMgrType:" << i
                                           << " memType=" << it->second.base.memType);
            return true;
        }
    }
    return false;
}

ReservedGvaInfo HybmVaManager::AllocReserveGva(uint32_t localRankId, uint64_t size, uint64_t localSize,
                                               hybm_mem_type memType, bool secondMapping)
{
    ReservedGvaInfo result;
    uint32_t t = HVM_DVA; // reserve device va all
    BM_VALIDATE_RETURN(size != 0, "size must > 0.", result);
    BM_VALIDATE_RETURN(localSize != 0 || secondMapping, "local size must > 0 when no second mapping.", result);

    std::unique_lock<std::shared_mutex> lock(mutex_);
    uint64_t lva = 0;
    if (localSize > 0) {
        lva = AllocReserveLva(localRankId, localSize, t);
        if (lva == 0) {
            return result;
        }
    }

    uint64_t gva = lva;
    if (secondMapping) {
        uint64_t startAddr = HYBM_GVM_START_ADDR_4P;
        uint64_t endAddr = HYBM_GVM_END_ADDR_8P;
        uint64_t upperLimit = endAddr - startAddr;
        if (size > upperLimit) {
            BM_LOG_ERROR("Failed to reserve size:" << size << ", upper limit size:" << upperLimit);
            return result;
        }

        BM_LOG_DEBUG("secondMapping searching in GVA range " << VaToStr(startAddr) << "-" << VaToStr(endAddr));
        // Find free space
        auto [freeAddr, found] = FindFreeSpace(startAddr, endAddr, size, HVM_GVA);
        if (!found) {
            BM_LOG_ERROR("AllocReserveGva failed: no free space found for size=" << VaToStr(size));
            return result;
        }
        gva = freeAddr;
    }

    result.va[HVM_GVA] = gva;
    result.va[t] = lva;
    result.size = size;
    result.memType = memType;
    result.localRankId = localRankId;
    reservedMap_[HVM_GVA][result.va[HVM_GVA]] = result;
    reservedMap_[t][result.va[t]] = result;
    BM_LOG_INFO("AllocReserveGva success: " << result);
    return result;
}

uint64_t HybmVaManager::AllocReserveLva(uint32_t localRankId, uint64_t size, uint32_t type)
{
    if (size == 0) {
        BM_LOG_ERROR("AllocReserveLva failed: size=0");
        return 0;
    }
    uint64_t upperLimit = HYBM_GVM_END_ADDR - HYBM_GVM_START_ADDR;
    uint64_t startAddr = HYBM_GVM_START_ADDR;
    uint64_t endAddr = HYBM_GVM_END_ADDR;
    if (size > upperLimit) {
        BM_LOG_ERROR("Failed to reserve size:" << size << ", upper limit size:" << upperLimit);
        return 0;
    }

    BM_LOG_DEBUG("AllocReserveLva, searching in HOST range " << VaToStr(startAddr) << "-" << VaToStr(endAddr));
    // Find free space
    auto [freeAddr, found] = FindFreeSpace(startAddr, endAddr, size, type);
    if (!found) {
        BM_LOG_ERROR("AllocReserveLva failed: no free space found for size=" << VaToStr(size));
        return 0;
    }

    BM_LOG_INFO("AllocReserveLva success: " << VaToStr(freeAddr));
    return freeAddr;
}

void HybmVaManager::FreeReserveGva(uint64_t addr)
{
    if (addr == 0) {
        BM_LOG_WARN("FreeReserveGva failed: invalid addr=0");
        return;
    }
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = reservedMap_[HVM_GVA].find(addr);
    if (it == reservedMap_[HVM_GVA].end()) {
        BM_LOG_WARN("FreeReserveGva failed: reserved space not found at addr=" << VaToStr(addr));
        return;
    }
    const ReservedGvaInfo &info = it->second;
    BM_LOG_INFO("FreeReserveGva: " << info);
    auto type = (info.va[HVM_DVA] != 0 ? HVM_DVA : HVM_HVA);
    reservedMap_[type].erase(info.va[type]);
    reservedMap_[HVM_GVA].erase(it);
    BM_LOG_DEBUG("FreeReserveGva success: addr=" << VaToStr(addr));
}

void HybmVaManager::DumpReservedGvaInfo() const
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    BM_LOG_DEBUG("Total reserved spaces: " << reservedMap_[HVM_GVA].size());
    if (reservedMap_[HVM_GVA].empty()) {
        BM_LOG_WARN("No reserved spaces found.");
        return;
    }
    int index = 1;
    uint64_t totalSizeMB = 0;
    for (const auto &pair : reservedMap_[HVM_GVA]) {
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
    BM_LOG_DEBUG("Total allocated spaces: " << allocatedMap_[HVM_GVA].size());
    if (allocatedMap_[HVM_GVA].empty()) {
        BM_LOG_WARN("No allocated spaces found.");
        return;
    }
    int index = 1;
    uint64_t totalSizeMB = 0;
    for (const auto &pair : allocatedMap_[HVM_GVA]) {
        const AllocatedGvaInfo &info = pair.second;
        BM_LOG_DEBUG(index << ". " << info);
        totalSizeMB += (info.base.size >> MB_OFFSET);
        index++;
    }
    BM_LOG_DEBUG("Total allocated size: " << totalSizeMB << "MB");
    std::map<hybm_mem_type, uint64_t> sizeByType;
    std::map<hybm_mem_type, size_t> countByType;
    for (const auto &pair : allocatedMap_[HVM_GVA]) {
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

std::pair<bool, AllocatedGvaInfo> HybmVaManager::CheckOverlap(const uint64_t va, const uint64_t size, uint32_t type)
{
    const uint64_t end = va + size;
    if (allocatedMap_[type].empty()) {
        return {false, {}};
    }
    auto it = allocatedMap_[type].upper_bound(va);
    if (it != allocatedMap_[type].end()) {
        uint64_t st = it->first;
        uint64_t ed = it->second.base.size + st;
        if ((st <= va && va < ed) || (va <= st && st < end)) {
            return {true, it->second};
        }
    }

    if (it != allocatedMap_[type].begin()) {
        it--;
        uint64_t st = it->first;
        uint64_t ed = it->second.base.size + st;
        if ((st <= va && va < ed) || (va <= st && st < end)) {
            return {true, it->second};
        }
    }
    return {false, {}};
}

std::pair<uint64_t, bool> HybmVaManager::FindFreeSpace(uint64_t start, uint64_t end, uint64_t size, uint32_t type)
{
    if (size == 0 || start >= end || size > (end - start)) {
        BM_LOG_ERROR("FindFreeSpace: invalid parameters. start=" << VaToStr(start) << ", end=" << VaToStr(end)
                                                                 << ", size=" << VaToStr(size));
        return {0, false};
    }
    std::vector<std::pair<uint64_t, uint64_t>> usedRanges;
    for (const auto &pair : reservedMap_[type]) {
        const ReservedGvaInfo &info = pair.second;
        if (info.va[type] >= start && info.va[type] < end) {
            usedRanges.emplace_back(info.va[type], info.va[type] + info.size);
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
    if (current <= end && size <= end - current) {
        BM_LOG_DEBUG("FindFreeSpace: found free space at end, addr=" << VaToStr(current));
        return {current, true};
    }
    BM_LOG_ERROR("FindFreeSpace: no suitable free space found, size: " << VaToStr(size));
    return {0, false};
}

std::pair<AllocatedGvaInfo, bool> HybmVaManager::FindAllocByVa(uint64_t va, uint32_t type) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (allocatedMap_[type].empty()) {
        BM_LOG_WARN("No allocated spaces found.");
        return {AllocatedGvaInfo{}, false};
    }
    auto it = allocatedMap_[type].upper_bound(va);
    if (it != allocatedMap_[type].begin()) {
        --it;
    }
    if (it->second.Contains(va, type)) {
        return {it->second, true};
    }
    return {AllocatedGvaInfo{}, false};
}

size_t HybmVaManager::GetAllocCount() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    size_t count = allocatedMap_[HVM_GVA].size();
    BM_LOG_DEBUG("GetAllocCount: " << count);
    return count;
}

size_t HybmVaManager::GetReservedCount() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    size_t count = reservedMap_[HVM_GVA].size();
    BM_LOG_DEBUG("GetReservedCount: " << count);
    return count;
}

void HybmVaManager::ClearAll()
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    BM_LOG_DEBUG("Alloc count before clear: " << allocatedMap_[HVM_GVA].size());
    BM_LOG_DEBUG("Reserved count before clear: " << reservedMap_[HVM_GVA].size());
    for (uint32_t i = 0; i < HVM_BUTT; i++) {
        allocatedMap_[i].clear();
        reservedMap_[i].clear();
    }
}
} // namespace mf
} // namespace ock