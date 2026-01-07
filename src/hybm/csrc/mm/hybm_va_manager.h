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
#ifndef MEM_FABRIC_HYBRID_HYBM_VA_MANAGER_H
#define MEM_FABRIC_HYBRID_HYBM_VA_MANAGER_H

#include <map>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <iomanip>
#include "hybm_common_include.h"

namespace ock {
namespace mf {
inline std::ostream &operator<<(std::ostream &os, hybm_mem_type obj)
{
    switch (obj) {
        case HYBM_MEM_TYPE_DEVICE:
            return os << "DEVICE";
        case HYBM_MEM_TYPE_HOST:
            return os << "HOST";
        case HYBM_MEM_TYPE_BUTT:
            return os << "BUTT";
        default:
            return os << "UNKNOWN(" << static_cast<unsigned>(obj) << ")";
    }
}

template<typename T>
std::string VaToStr(T v)
{
    uint64_t v64 = 0;
    if constexpr (std::is_pointer_v<T>) {
        v64 = static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(v));
    } else {
        v64 = static_cast<uint64_t>(v);
    }
    std::stringstream ss;
    ss << "0x" << std::hex << v64;
    return ss.str();
}

// Reserved virtual address ranges; GVA ranges of all types must not overlap.
struct ReservedGvaInfo {
    uint64_t start{};                            // >0
    uint64_t size{};                             // >0
    hybm_mem_type memType{HYBM_MEM_TYPE_DEVICE}; // Must be set
    uint32_t localRankId{};                      // Must be set >=0; currently only one localRankId per process
    friend std::ostream &operator<<(std::ostream &os, const ReservedGvaInfo &obj)
    {
        os << "ReservedGvaInfo{start: " << VaToStr(obj.start) << ", size: " << VaToStr(obj.size)
           << ", memType: " << obj.memType << ", localRankId: " << obj.localRankId << "}";
        return os;
    }

    [[nodiscard]] bool Contains(const uint64_t addr) const
    {
        return addr >= start && addr < start + size;
    }

    [[nodiscard]] uint64_t End() const
    {
        return start + size;
    }
};
constexpr uint32_t INVALID_RANK_ID = std::numeric_limits<uint32_t>::max();
struct BaseAllocatedGvaInfo {
    uint64_t gva{};                              // >0
    uint64_t size{};                             // >0
    hybm_mem_type memType{HYBM_MEM_TYPE_DEVICE}; // Must be set
    uint64_t lva{};
};
// Actually allocated memory segments. LVA is an address directly accessible by the XPU, which may equal GVA.
// For the current segment, lva == gva.
// For registered memory: the user provides LVA, and the registered address becomes GVA.
// For imported memory: lva = 0.
struct AllocatedGvaInfo {
    BaseAllocatedGvaInfo base;
    bool registered{false}; // Indicates memory allocated by the localRankId itself, not externally registered
    uint32_t localRankId{INVALID_RANK_ID};    // Must be set >=0
    uint32_t importedRankId{INVALID_RANK_ID}; // can be set >=0

    AllocatedGvaInfo() = default;
    AllocatedGvaInfo(uint64_t gva, uint64_t size, uint32_t localRankId, hybm_mem_type type, uint64_t lva)
        : base{gva, size, type, lva}, registered(false), localRankId(localRankId)
    {}

    AllocatedGvaInfo(uint64_t gva, uint64_t size, uint32_t localRankId, hybm_mem_type type, uint64_t lva,
                     bool registeredFlag)
        : base{gva, size, type, lva}, registered(registeredFlag), localRankId(localRankId)
    {}

    AllocatedGvaInfo(uint64_t gva, uint64_t size, uint32_t localRankId, uint32_t importedRankId, hybm_mem_type type,
                     uint64_t lva)
        : base{gva, size, type, lva}, registered(false), localRankId(localRankId), importedRankId(importedRankId)
    {}

    // Checks if gvaInfo lies entirely within this segment.
    [[nodiscard]] bool In(const AllocatedGvaInfo &gvaInfo) const
    {
        return gvaInfo.base.gva >= base.gva && (gvaInfo.base.gva + gvaInfo.base.size) <= (base.gva + base.size);
    }

    [[nodiscard]] bool Contains(uint64_t addr) const
    {
        return addr >= base.gva && addr < base.gva + base.size;
    }

    [[nodiscard]] bool ContainsByLva(uint64_t addr) const
    {
        if (base.lva == 0) {
            return false;
        }
        return addr >= base.lva && addr < base.lva + base.size;
    }

    [[nodiscard]] uint64_t End() const
    {
        return base.gva + base.size;
    }

    [[nodiscard]] uint32_t RankId() const noexcept
    {
        return (importedRankId != INVALID_RANK_ID) ? importedRankId : localRankId;
    }

    friend std::ostream &operator<<(std::ostream &os, const AllocatedGvaInfo &obj)
    {
        os << "AllocatedGvaInfo{gva: " << VaToStr(obj.base.gva) << ", size: " << VaToStr(obj.base.size)
           << ", rankId: " << obj.RankId() << ", registered: " << (obj.registered ? "true" : "false")
           << ", memType: " << obj.base.memType << ", lva: " << VaToStr(obj.base.lva) << "}";
        return os;
    }

    [[nodiscard]] std::string ToString() const
    {
        auto type = base.memType == hybm_mem_type::HYBM_MEM_TYPE_HOST ? "H" : "D";
        auto remote = base.lva > 0 ? "L" : "R";
        std::stringstream os;
        os << "{gva: " << VaToStr(base.gva) << ", ";
        os << remote << type << "(";
        os << localRankId << "), lva: " << VaToStr(base.lva) << "}";
        return os.str();
    }
};

inline bool operator==(const AllocatedGvaInfo &lhs, const AllocatedGvaInfo &rhs)
{
    return lhs.base.gva == rhs.base.gva && lhs.base.size == rhs.base.size && lhs.RankId() == rhs.RankId();
}

inline bool operator!=(const AllocatedGvaInfo &lhs, const AllocatedGvaInfo &rhs)
{
    return !(lhs == rhs);
}

/*
 * Virtual address management and maintenance.
 * Address types include DRAM and HBM. There are two kinds of ranges: AllocatedGvaInfo and ReservedGvaInfo.
 * Memory segments of the same type must not overlap.
 * LVA is an address in the current process that the XPU can directly access, and it may be equal to GVA.
 */
class HybmVaManager {
public:
    HybmVaManager(const HybmVaManager &) = delete;
    HybmVaManager &operator=(const HybmVaManager &) = delete;

    static HybmVaManager &GetInstance()
    {
        static HybmVaManager instance;
        return instance;
    }
    Result Initialize(AscendSocType socType) noexcept;
    Result AddVaInfoFromExternal(const BaseAllocatedGvaInfo &baseInfo, uint32_t localRankId);
    Result AddVaInfoFromExternal(const BaseAllocatedGvaInfo &baseInfo, uint32_t localRankId, uint32_t importedRankId);
    Result AddVaInfo(const BaseAllocatedGvaInfo &baseInfo, uint32_t localRankId);
    Result AddVaInfo(const AllocatedGvaInfo &info);
    void RemoveOneVaInfo(uint64_t va);

    // Returns 0 if not found.
    uint64_t GetGvaByLva(uint64_t lva);
    // Returns 0 if not found.
    uint64_t GetLvaByGva(uint64_t gva);
    std::pair<AllocatedGvaInfo, bool> FindAllocByGva(uint64_t gva) const;
    std::pair<AllocatedGvaInfo, bool> FindAllocByLva(uint64_t lva) const;

    // Checks if 'va' falls within any AllocatedGvaInfo range.
    bool IsGva(uint64_t va);
    hybm_mem_type GetMemType(uint64_t va); // Supports both LVA and GVA
    std::pair<uint32_t, bool> GetRank(uint64_t gva);
    // Checks if 'va' is within any AllocatedGvaInfo range (either LVA or GVA).
    bool IsValidAddr(uint64_t va);

    // =============ReservedGvaInfo Management==============================
    ReservedGvaInfo AllocReserveGva(uint32_t localRankId, uint64_t size, hybm_mem_type memType);
    void FreeReserveGva(uint64_t addr);

    void DumpReservedGvaInfo() const;
    void DumpAllocatedGvaInfo() const;

    size_t GetAllocCount() const;
    size_t GetReservedCount() const;
    void ClearAll();

private:
    HybmVaManager() = default;

    ~HybmVaManager() = default;

    std::pair<bool, AllocatedGvaInfo> CheckOverlap(uint64_t gva, uint64_t size, hybm_mem_type memType);

    std::pair<uint64_t, bool> FindFreeSpace(uint64_t start, uint64_t end, uint64_t size) const;

    std::pair<ReservedGvaInfo, bool> FindReservedByAddr(uint64_t addr) const;

private:
    mutable std::shared_mutex mutex_{};

    std::map<uint64_t, AllocatedGvaInfo> allocatedLookupMapByGva_{};
    std::map<uint64_t, AllocatedGvaInfo> allocatedLookupMapByLva_{};

    std::map<uint64_t, ReservedGvaInfo> reservedLookupMapByGva_{};

    std::map<hybm_mem_type, std::vector<AllocatedGvaInfo>> allocatedLookupMapByMemType_{};

    uint64_t reserveStart_{HYBM_GVM_START_ADDR};
    uint64_t reserveEnd_{HYBM_GVM_END_ADDR};
};

template<typename T>
std::string VaToInfo(T v)
{
    uint64_t v64 = 0;
    if constexpr (std::is_pointer_v<T>) {
        v64 = static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(v));
    } else {
        v64 = static_cast<uint64_t>(v);
    }
    auto info = HybmVaManager::GetInstance().FindAllocByGva(v64);
    if (info.second) {
        return info.first.ToString();
    }
    info = HybmVaManager::GetInstance().FindAllocByLva(v64);
    if (info.second) {
        return info.first.ToString();
    }
    return VaToStr(v64);
}
} // namespace mf
} // namespace ock

#endif