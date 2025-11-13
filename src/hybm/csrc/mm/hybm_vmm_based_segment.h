/*
Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#ifndef MF_HYBRID_HYBM_VMM_BASED_SEGMENT_H
#define MF_HYBRID_HYBM_VMM_BASED_SEGMENT_H

#include "hybm_dev_legacy_segment.h"
#include "hybm_mem_common.h"
#include "dl_hal_api.h"

namespace ock {
namespace mf {
#ifdef USE_VMM
struct HostSdmaExportInfo {
    uint64_t magic{DRAM_SLICE_EXPORT_INFO_MAGIC};
    uint64_t version{EXPORT_INFO_VERSION};
    uint64_t mappingOffset{0};
    uint32_t sliceIndex{0};
    uint32_t sdid{0};
    uint32_t rankId{0};
    uint32_t devId{0};
    uint64_t size{0};
    MemShareHandle shareHandle;
};

class HybmVmmBasedSegment : public MemSegment {
public:
    explicit HybmVmmBasedSegment(const MemSegmentOptions &options, int eid) : MemSegment{options, eid}
    {
    }

    ~HybmVmmBasedSegment() override
    {
    }

    Result ValidateOptions() noexcept override;
    Result ReserveMemorySpace(void **address) noexcept override;
    Result UnReserveMemorySpace() noexcept override;
    Result AllocLocalMemory(uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept override;
    Result RegisterMemory(const void *addr, uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept override;
    Result ReleaseSliceMemory(const std::shared_ptr<MemSlice> &slice) noexcept override;
    Result Export(std::string &exInfo) noexcept override;
    Result Export(const std::shared_ptr<MemSlice> &slice, std::string &exInfo) noexcept override;
    Result GetExportSliceSize(size_t &size) noexcept override;
    Result Import(const std::vector<std::string> &allExInfo, void *addresses[]) noexcept override;
    Result RemoveImported(const std::vector<uint32_t> &ranks) noexcept override;
    Result Mmap() noexcept override;
    Result Unmap() noexcept override;
    std::shared_ptr<MemSlice> GetMemSlice(hybm_mem_slice_t slice) const noexcept override;
    bool MemoryInRange(const void *begin, uint64_t size) const noexcept override;
    void GetRankIdByAddr(const void *addr, uint64_t size, uint32_t &rankId) const noexcept override;
    bool CheckSmdaReaches(uint32_t rankId) const noexcept override;

    hybm_mem_type GetMemoryType() const noexcept override
    {
        return HYBM_MEM_TYPE_HOST;
    }

private:
    Result GetDeviceInfo() noexcept;
    Result ExportInner(const std::shared_ptr<MemSlice> &slice, MemShareHandle &sHandle) noexcept;

    std::vector<HostSdmaExportInfo> imports_;
    uint8_t *globalVirtualAddress_{nullptr};
    uint64_t totalVirtualSize_{0UL};
    uint64_t allocatedSize_{0UL};
    uint16_t sliceCount_{0};

    std::map<uint16_t, MemSliceStatus> slices_;
    std::map<uint16_t, std::string> exportMap_;
    std::map<uint64_t, drv_mem_handle_t *> mappedMem_;
};
#else
struct HostSdmaExportInfo {
    uint64_t magic{DRAM_SLICE_EXPORT_INFO_MAGIC};
    uint64_t version{EXPORT_INFO_VERSION};
    uint64_t mappingOffset{0};
    uint32_t sliceIndex{0};
    uint32_t sdid{0};
    uint32_t rankId{0};
    uint64_t size{0};
    uint64_t shmKey{0};
};

class HybmVmmBasedSegment : public HybmDevLegacySegment {
public:
    explicit HybmVmmBasedSegment(const MemSegmentOptions &options, int eid) : HybmDevLegacySegment{options, eid},
        shareKey_{0}
    {
    }

    ~HybmVmmBasedSegment() override
    {
        FreeMemory();
    }

    Result ValidateOptions() noexcept override;
    Result ReserveMemorySpace(void **address) noexcept override;
    Result AllocLocalMemory(uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept override;
    Result ReleaseSliceMemory(const std::shared_ptr<MemSlice> &slice) noexcept override;
    Result Export(std::string &exInfo) noexcept override;
    Result Export(const std::shared_ptr<MemSlice> &slice, std::string &exInfo) noexcept override;
    Result Import(const std::vector<std::string> &allExInfo, void *addresses[]) noexcept override;
    Result RemoveImported(const std::vector<uint32_t> &ranks) noexcept override;
    Result Mmap() noexcept override;
    Result Unmap() noexcept override;
    std::shared_ptr<MemSlice> GetMemSlice(hybm_mem_slice_t slice) const noexcept override;
    bool MemoryInRange(const void *begin, uint64_t size) const noexcept override;

    // Result RegisterMemory(const void *addr, uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept override;
    // Result GetExportSliceSize(size_t &size) noexcept override;

    hybm_mem_type GetMemoryType() const noexcept override
    {
        return HYBM_MEM_TYPE_HOST;
    }

private:
    uint32_t rankIndex_{0U};
    uint32_t rankCount_{0U};
    uint64_t shareKey_;
    std::vector<HostSdmaExportInfo> imports_;
};
#endif
}
}

#endif // MF_HYBRID_HYBM_VMM_BASED_SEGMENT_H
