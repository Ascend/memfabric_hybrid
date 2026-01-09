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
#ifndef MF_HYBRID_HYBM_VMM_BASED_SEGMENT_H
#define MF_HYBRID_HYBM_VMM_BASED_SEGMENT_H

#include "hybm_dev_legacy_segment.h"
#include "hybm_mem_common.h"
#include "dl_hal_api.h"

namespace ock {
namespace mf {
struct HostSdmaExportInfo {
    uint64_t magic{DRAM_SLICE_EXPORT_INFO_MAGIC};
    uint64_t version{EXPORT_INFO_VERSION};
    uint64_t vAddress{0};
    uint32_t sliceIndex{0};
    uint32_t sdid{0};
    uint32_t serverId{0};
    uint32_t superPodId{0};
    uint32_t rankId{0};
    uint32_t devId{0};
    uint64_t size{0};
    MemShareHandle shareHandle;
};

class HybmVmmBasedSegment : public MemSegment {
public:
    explicit HybmVmmBasedSegment(const MemSegmentOptions &options, int eid) : MemSegment{options, eid} {}

    ~HybmVmmBasedSegment() override {}

    Result ValidateOptions() noexcept override;
    Result ReserveMemorySpace(void **address) noexcept override;
    Result UnReserveMemorySpace() noexcept override;
    Result AllocLocalMemory(uint64_t size, MemSlicePtr &slice) noexcept override;
    Result RegisterMemory(const void *addr, uint64_t size, MemSlicePtr &slice) noexcept override;
    Result ReleaseSliceMemory(const MemSlicePtr &slice) noexcept override;
    Result Export(std::string &exInfo) noexcept override;
    Result Export(const MemSlicePtr &slice, std::string &exInfo) noexcept override;
    Result GetExportSliceSize(size_t &size) noexcept override;
    Result Import(const std::vector<std::string> &allExInfo, void *addresses[]) noexcept override;
    Result RemoveImported(const std::vector<uint32_t> &ranks) noexcept override;
    Result Mmap() noexcept override;
    Result Unmap() noexcept override;
    MemSlicePtr GetMemSlice(hybm_mem_slice_t slice) const noexcept override;
    bool MemoryInRange(const void *begin, uint64_t size) const noexcept override;
    bool GetRankIdByAddr(const void *addr, uint64_t size, uint32_t &rankId) const noexcept override;
    bool CheckSdmaReaches(uint32_t rankId) const noexcept override;

    hybm_mem_type GetMemoryType() const noexcept override
    {
        return options_.segType == HYBM_MST_HBM ? HYBM_MEM_TYPE_DEVICE : HYBM_MEM_TYPE_HOST;
    }

private:
    Result ExportInner(const MemSlicePtr &slice, MemShareHandle &sHandle) noexcept;
    Result MallocEmptySlice(MemSlicePtr &slice) noexcept;
    Result MallocFromHost(size_t size, uint32_t devId, drv_mem_handle_t **handle) noexcept;
    Result MallocFromDevice(size_t size, uint32_t devId, drv_mem_handle_t **handle) noexcept;

    std::vector<HostSdmaExportInfo> imports_;
    uint8_t *globalVirtualAddress_{nullptr};
    uint64_t totalVirtualSize_{0UL};
    uint64_t allocatedSize_{0UL};
    uint16_t sliceCount_{0};

    std::map<uint16_t, MemSliceStatus> slices_;
    std::map<uint16_t, std::string> exportMap_;
    std::map<uint64_t, drv_mem_handle_t *> mappedMem_;
};
} // namespace mf
} // namespace ock

#endif // MF_HYBRID_HYBM_VMM_BASED_SEGMENT_H