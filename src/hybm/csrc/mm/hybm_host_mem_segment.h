/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_HYBRID_HYBM_HOST_MEM_SEGMENT_H
#define MF_HYBRID_HYBM_HOST_MEM_SEGMENT_H

#include <set>
#include "hybm_mem_segment.h"
#include "hybm_mem_common.h"

namespace ock {
namespace mf {

struct HostExportInfo {
    uint64_t magic{DRAM_SLICE_EXPORT_INFO_MAGIC};
    uint64_t version{EXPORT_INFO_VERSION};
    uint64_t mappingOffset{0};
    uint32_t sliceIndex{0};
    uint32_t rankId{0};
    uint64_t size{0};
    MemPageTblType pageTblType{};
    MemSegType memSegType{};
    MemSegInfoExchangeType exchangeType{};
};
class MemSegmentHost : public MemSegment {
public:
    explicit MemSegmentHost(const MemSegmentOptions &options, int eid) : MemSegment{options, eid} {}
    ~MemSegmentHost()
    {
        FreeMemory();
    }

    Result ValidateOptions() noexcept override;
    Result ReserveMemorySpace(void **address) noexcept override;
    Result AllocLocalMemory(uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept override;
    Result Export(std::string &exInfo) noexcept override;
    Result Export(const std::shared_ptr<MemSlice> &slice, std::string &exInfo) noexcept override;
    Result Import(const std::vector<std::string> &allExInfo) noexcept override;
    Result Mmap() noexcept override;
    Result Unmap() noexcept override;
    std::shared_ptr<MemSlice> GetMemSlice(hybm_mem_slice_t slice) const noexcept override;
    bool MemoryInRange(const void *begin, uint64_t size) const noexcept override;
    uint32_t GetRankIdByAddr(const void *addr, uint64_t size) const noexcept override;
    Result RemoveImported(const std::vector<uint32_t> &ranks) noexcept override;
    hybm_mem_type GetMemoryType() const noexcept override
    {
        return HYBM_MEM_TYPE_HOST;
    }

private:
    void FreeMemory() noexcept;
    static void LvaShmReservePhysicalMemory(void *mappedAddress, uint64_t size) noexcept;

private:
    uint8_t *globalVirtualAddress_{nullptr};
    uint64_t totalVirtualSize_{0UL};
    uint8_t *localVirtualBase_{nullptr};
    uint64_t allocatedSize_{0UL};
    uint16_t sliceCount_{0};
    std::map<uint16_t, MemSliceStatus> slices_;
    std::map<uint16_t, std::string> exportMap_;
    std::vector<HostExportInfo> imports_;
};
}
}

#endif  // MF_HYBRID_HYBM_HOST_MEM_SEGMENT_H
