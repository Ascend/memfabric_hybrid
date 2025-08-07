/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_DEVICE_MEM_SEGMENT_H
#define MEM_FABRIC_HYBRID_HYBM_DEVICE_MEM_SEGMENT_H

#include <cstdint>
#include <map>
#include <set>
#include "hybm_mem_common.h"
#include "hybm_mem_segment.h"

namespace ock {
namespace mf {

struct HbmExportInfo {
    uint64_t magic;
    uint64_t version;
    uint64_t mappingOffset{0};
    uint32_t sliceIndex{0};
    uint32_t sdid{0};
    int pid{0};
    uint32_t rankId{0};
    uint64_t size{0};
    int entityId{0};
    MemPageTblType pageTblType;
    MemSegType memSegType;
    MemSegInfoExchangeType exchangeType;
    uint8_t deviceId{0};
    char shmName[DEVICE_SHM_NAME_SIZE + 1U]{};
};

struct MemSliceStatus {
    std::shared_ptr<MemSlice> slice;

    explicit MemSliceStatus(std::shared_ptr<MemSlice> s) noexcept : slice{std::move(s)} {}
};

class MemSegmentDevice : public MemSegment {
public:
    explicit MemSegmentDevice(const MemSegmentOptions &options, int eid) : MemSegment{options, eid} {}
    ~MemSegmentDevice()
    {
        FreeMemory();
    }

    Result ValidateOptions() noexcept override;
    Result ReserveMemorySpace(void **address) noexcept override;
    Result UnreserveMemorySpace() noexcept override;
    Result AllocLocalMemory(uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept override;
    Result RegisterMemory(const void *addr, uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept override;
    Result ReleaseSliceMemory(const std::shared_ptr<MemSlice> &slice) noexcept override;
    Result Export(std::string &exInfo) noexcept override;
    Result Export(const std::shared_ptr<MemSlice> &slice, std::string &exInfo) noexcept override;
    Result GetExportSliceSize(size_t &size) noexcept override;
    Result Import(const std::vector<std::string> &allExInfo, void *addresses[]) noexcept override;
    Result RemoveImported(const std::vector<uint32_t>& ranks) noexcept override;
    Result Mmap() noexcept override;
    Result Unmap() noexcept override;
    std::shared_ptr<MemSlice> GetMemSlice(hybm_mem_slice_t slice) const noexcept override;
    bool MemoryInRange(const void *begin, uint64_t size) const noexcept override;

private:
    Result GetDeviceInfo() noexcept;
    void FreeMemory() noexcept;

private:
    uint8_t *globalVirtualAddress_{nullptr};
    uint64_t totalVirtualSize_{0UL};
    uint64_t allocatedSize_{0UL};
    uint16_t sliceCount_{0};
    std::map<uint16_t, MemSliceStatus> slices_;
    std::map<uint16_t, std::string> exportMap_;
    std::set<uint64_t> mappedMem_;
    std::vector<HbmExportInfo> imports_;
};
}
}

#endif  // MEM_FABRIC_HYBRID_HYBM_DEVICE_MEM_SEGMENT_H
