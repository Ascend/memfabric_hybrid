/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_HYBM_DEVICE_USER_MEM_SEG_H
#define MF_HYBRID_HYBM_DEVICE_USER_MEM_SEG_H

#include <bitset>
#include "hybm_mem_segment.h"
#include "hybm_device_mem_segment.h"

namespace ock {
namespace mf {
struct RegisterSlice {
    std::shared_ptr<MemSlice> slice;
    std::string name;
    RegisterSlice() = default;
    RegisterSlice(std::shared_ptr<MemSlice> s, std::string n) noexcept : slice(std::move(s)), name(std::move(n)) {}
};

struct HbmExportDeviceInfo {
    uint32_t sdid{0};
    uint32_t pid{0};
    uint32_t serverId{0};
    uint32_t superPodId{0};
    uint32_t rankId{0};
    uint16_t deviceId{0};
    uint16_t reserved{0};
};

struct HbmExportSliceInfo {
    uint64_t address{0};
    uint64_t size{0};
    uint32_t serverId{0};
    uint32_t superPodId{0};
    uint16_t rankId{0};
    uint16_t reserved{0};
    uint32_t deviceId{0};
    char name[DEVICE_SHM_NAME_SIZE]{};
};

class MemSegmentDeviceUseMem : public MemSegmentDevice {
public:
    MemSegmentDeviceUseMem(const MemSegmentOptions &options, int eid) noexcept;
    ~MemSegmentDeviceUseMem() override;
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
    void CloseMemory() noexcept;
    hybm_mem_type GetMemoryType() const noexcept override
    {
        return HYBM_MEM_TYPE_DEVICE;
    }
    bool CheckSmdaReaches(uint32_t rankId) const noexcept override;
    void GetRankIdByAddr(const void *addr, uint64_t size, uint32_t &rankId) const noexcept override;

private:
    Result ImportDeviceInfo(const std::string &info) noexcept;
    Result ImportSliceInfo(const std::string &info, std::shared_ptr<MemSlice> &remoteSlice) noexcept;

private:
    uint16_t sliceCount_{0};
    std::bitset<16> enablePeerDevices_;
    std::map<uint16_t, RegisterSlice> registerSlices_;
    std::map<uint16_t, RegisterSlice> remoteSlices_;
    std::map<uint64_t, uint64_t, std::greater<uint64_t>> addressedSlices_;
    std::map<uint32_t, HbmExportDeviceInfo> importedDeviceInfo_;
    std::map<std::string, HbmExportSliceInfo> importedSliceInfo_;
    std::vector<void *> registerAddrs_{};
    std::vector<std::string> memNames_{};
};
}
}

#endif  // MF_HYBRID_HYBM_DEVICE_USER_MEM_SEG_H
