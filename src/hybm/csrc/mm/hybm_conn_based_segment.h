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
#ifndef MF_HYBRID_HYBM_CONN_BASED_SEGMENT_H
#define MF_HYBRID_HYBM_CONN_BASED_SEGMENT_H

#include <set>
#include "hybm_mem_segment.h"
#include "hybm_mem_common.h"

namespace ock {
namespace mf {
struct HostExportInfo {
    uint64_t magic{DRAM_SLICE_EXPORT_INFO_MAGIC};
    uint64_t version{EXPORT_INFO_VERSION};
    uint64_t gva{0};
    uint32_t sliceIndex{0};
    uint32_t rankId{0};
    uint64_t size{0};
    MemPageTblType pageTblType{};
    MemSegType memSegType{};
    MemSegInfoExchangeType exchangeType{};
    char padding_[UNIFIED_EXCHANGE_SEG_INFO_SIZE - 56]{};
};

static_assert(sizeof(HostExportInfo) == UNIFIED_EXCHANGE_SEG_INFO_SIZE,
              "HostExportInfo must be UNIFIED_EXCHANGE_SEG_INFO_SIZE(192) bytes");

class HybmConnBasedSegment : public MemSegment {
public:
    explicit HybmConnBasedSegment(const MemSegmentOptions &options, int eid) : MemSegment{options, eid} {}
    ~HybmConnBasedSegment() override
    {
        FreeMemory();
    }

    Result ValidateOptions() noexcept override;
    Result ReserveMemorySpace(void **address) noexcept override;
    Result UnReserveMemorySpace() noexcept override;
    Result AllocLocalMemory(uint64_t size, MemSlicePtr &slice) noexcept override;
    Result Export(std::string &exInfo) noexcept override;
    Result Export(const MemSlicePtr &slice, std::string &exInfo) noexcept override;
    Result Import(const std::vector<std::string> &allExInfo, void *addresses[]) noexcept override;
    Result Mmap() noexcept override;
    Result Unmap() noexcept override;
    MemSlicePtr GetMemSlice(hybm_mem_slice_t slice, bool quiet) const noexcept override;
    bool MemoryInRange(const void *begin, uint64_t size) const noexcept override;
    Result RemoveImported(const std::vector<uint32_t> &ranks) noexcept override;
    Result RegisterMemory(const void *addr, uint64_t size, MemSlicePtr &slice) noexcept override;
    Result ReleaseSliceMemory(const MemSlicePtr &slice) noexcept override;
    Result GetExportSliceSize(size_t &size) noexcept override;
    hybm_mem_type GetMemoryType() const noexcept override
    {
        return HYBM_MEM_TYPE_HOST;
    }

private:
    void FreeMemory() noexcept;
    Result PrepareShareMemoryFd() const noexcept;
    Result MapSlice(void *&mapped, void *sliceAddr, uint64_t lvOffset, uint64_t size, uint64_t gva,
                    MemAllocMethod &allocMethod) noexcept;
    static void LvaShmReservePhysicalMemory(void *mappedAddress, uint64_t size) noexcept;
    void* AllocMemory(void *sliceAddr, uint64_t lvOffset, uint64_t size, MemAllocMethod &allocMethod);
    static void FreeAllocatedMemory(void *ptr, uint64_t size, MemAllocMethod allocMethod) noexcept;

private:
    uint8_t *globalVirtualAddress_{nullptr};
    uint64_t totalVirtualSize_{0UL};
    uint8_t *localVirtualBase_{nullptr};
    uint64_t allocatedSize_{0UL};
    std::map<uint32_t, MemSliceStatus> slices_;
    std::map<uint32_t, std::string> exportMap_;
    std::vector<HostExportInfo> imports_;
    std::set<uint64_t> mappedGvaMem_; // gva sets
};
} // namespace mf
} // namespace ock

#endif // MF_HYBRID_HYBM_CONN_BASED_SEGMENT_H
