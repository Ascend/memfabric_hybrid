/*
Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#ifndef MF_HYBRID_HYBM_SDMA_MEM_SEGMENT_H
#define MF_HYBRID_HYBM_SDMA_MEM_SEGMENT_H

#include "hybm_device_mem_segment.h"
#include "hybm_mem_common.h"

namespace ock {
namespace mf {

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

class MemSegmentHostSDMA : public MemSegmentDevice {
public:
    explicit MemSegmentHostSDMA(const MemSegmentOptions &options, int eid) : MemSegmentDevice{options, eid},
        shareKey_{0}
    {
    }

    ~MemSegmentHostSDMA()
    {
        FreeMemory();
    }

    Result ValidateOptions() noexcept override;
    Result ReserveMemorySpace(void **address) noexcept override;
    // Result UnreserveMemorySpace() noexcept override;
    Result AllocLocalMemory(uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept override;
    // Result ReleaseSliceMemory(const std::shared_ptr<MemSlice> &slice) noexcept override;
    Result Export(std::string &exInfo) noexcept override;
    Result Export(const std::shared_ptr<MemSlice> &slice, std::string &exInfo) noexcept override;
    Result Import(const std::vector<std::string> &allExInfo) noexcept override;
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
}
}

#endif // MF_HYBRID_HYBM_SDMA_MEM_SEGMENT_H
