/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_DEVICE_MEM_SEGMENT_H
#define MEM_FABRIC_HYBRID_HYBM_DEVICE_MEM_SEGMENT_H

#include <cstdint>
#include <map>
#include <set>
#include <string>

#include "hybm_mem_common.h"
#include "hybm_mem_segment.h"

namespace ock {
namespace mf {
constexpr uint32_t invalidSuperPodId = 0xFFFFFFFFU;
constexpr uint32_t invalidServerId = 0x3FFU;

struct HbmExportInfo {
    uint64_t magic{HBM_SLICE_EXPORT_INFO_MAGIC};
    uint64_t version{EXPORT_INFO_VERSION};
    uint64_t mappingOffset{0};
    uint32_t sliceIndex{0};
    uint32_t sdid{0};
    uint32_t serverId{0};
    uint32_t superPodId{0};
    int32_t logicDeviceId{0};
    int pid{0};
    uint32_t rankId{0};
    uint64_t size{0};
    int entityId{0};
    MemPageTblType pageTblType{};
    MemSegType memSegType{};
    MemSegInfoExchangeType exchangeType{};
    uint8_t deviceId{0};
    char shmName[DEVICE_SHM_NAME_SIZE + 1U]{};
};

class HybmDevLegacySegment : public MemSegment {
public:
    explicit HybmDevLegacySegment(const MemSegmentOptions &options, int eid) : MemSegment{options, eid} {}
    ~HybmDevLegacySegment() override
    {
        FreeMemory();
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
    bool GetRankIdByAddr(const void *addr, uint64_t size, uint32_t &rankId) const noexcept override;
    hybm_mem_type GetMemoryType() const noexcept override
    {
        return HYBM_MEM_TYPE_DEVICE;
    }

    bool CheckSmdaReaches(uint32_t rankId) const noexcept override;

public:
    static int SetDeviceInfo(int deviceId) noexcept;
    static void GetDeviceInfo(uint32_t &sdId, uint32_t &serverId, uint32_t &superPodId) noexcept;

protected:
    Result GetDeviceInfo() noexcept;
    void FreeMemory() noexcept;

protected:
    uint8_t *globalVirtualAddress_{nullptr};
    uint64_t totalVirtualSize_{0UL};
    uint64_t allocatedSize_{0UL};
    uint16_t sliceCount_{0};
    std::map<uint16_t, MemSliceStatus> slices_;
    std::map<uint16_t, std::string> exportMap_;
    std::set<uint64_t> mappedMem_;
    std::vector<HbmExportInfo> imports_;
    std::map<uint16_t, HbmExportInfo> importMap_;
};
} // namespace mf
} // namespace ock

#endif // MEM_FABRIC_HYBRID_HYBM_DEVICE_MEM_SEGMENT_H
