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

#ifndef MF_HYBRID_HYBM_DEV_USER_LEGACY_SEGMENT_H
#define MF_HYBRID_HYBM_DEV_USER_LEGACY_SEGMENT_H

#include <bitset>
#include "hybm_mem_segment.h"
#include "hybm_dev_legacy_segment.h"

namespace ock {
namespace mf {
constexpr size_t MAX_PEER_DEVICES = 16;
struct RegisterSlice {
    MemSlicePtr slice;
    std::string name;
    RegisterSlice() = default;
    RegisterSlice(MemSlicePtr s, std::string n) noexcept : slice(std::move(s)), name(std::move(n)) {}
};

struct HbmExportDeviceInfo {
    uint64_t magic{ENTITY_EXPORT_INFO_MAGIC};
    uint32_t segmentType{SEGMENT_TYPE_USER_DEV};
    uint32_t sdid{0};
    uint32_t pid{0};
    uint32_t serverId{0};
    uint32_t superPodId{0};
    uint32_t rankId{0};
    uint16_t userDeviceId{0};
    uint16_t logicDeviceId{0};
    uint32_t reserved{0};

    // Padding to make total size UNIFIED_EXCHANGE_SEG_INFO_SIZE(192) bytes
    char padding_[UNIFIED_EXCHANGE_SEG_INFO_SIZE - 40]{};
};
static_assert(sizeof(HbmExportDeviceInfo) == UNIFIED_EXCHANGE_SEG_INFO_SIZE,
              "HbmExportDeviceInfo must be UNIFIED_EXCHANGE_SEG_INFO_SIZE(192) bytes,"
              " compatible with HostSdmaExportInfo");
static_assert(offsetof(HbmExportDeviceInfo, segmentType) == SEGMENT_TYPE_OFFSET, "segmentType offset mismatch!");

struct UserHbmExportSliceInfo {
    uint64_t magic{HBM_SLICE_EXPORT_INFO_MAGIC};
    uint32_t segmentType{SEGMENT_TYPE_USER_DEV};
    uint32_t serverId{0};
    uint64_t gvaOffset{0}; // gva offset
    uint64_t address{0};   // lva (host_va or device_va)
    uint64_t size{0};
    uint32_t superPodId{0};
    uint32_t rankId{0};
    uint32_t userDeviceId{0};
    uint32_t logicDeviceId{0};
    char name[DEVICE_SHM_NAME_SIZE + 1]{};

    // Padding to make total size UNIFIED_EXCHANGE_SEG_INFO_SIZE(192) bytes
    char padding_[UNIFIED_EXCHANGE_SEG_INFO_SIZE - 121]{};
};
static_assert(sizeof(UserHbmExportSliceInfo) == UNIFIED_EXCHANGE_SEG_INFO_SIZE,
              "UserHbmExportSliceInfo must be 200 bytes, compatible with HostSdmaExportInfo");
static_assert(offsetof(UserHbmExportSliceInfo, segmentType) == SEGMENT_TYPE_OFFSET, "segmentType offset mismatch!");

class HybmDevUserLegacySegment : public HybmDevLegacySegment {
public:
    HybmDevUserLegacySegment(const MemSegmentOptions &options, int eid) noexcept;
    ~HybmDevUserLegacySegment() override;
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
    MemSlicePtr GetMemSlice(hybm_mem_slice_t slice, bool quiet) const noexcept override;
    bool MemoryInRange(const void *begin, uint64_t size) const noexcept override;
    void CloseMemory() noexcept;
    hybm_mem_type GetMemoryType() const noexcept override
    {
        return HYBM_MEM_TYPE_DEVICE;
    }
    bool CheckSdmaReaches(uint32_t rankId) const noexcept override;

private:
    Result ImportDeviceInfo(const std::string &info) noexcept;
    Result ImportSliceInfo(const std::string &info, MemSlicePtr &remoteSlice) noexcept;
    static void RollbackIpcMemory(void *addresses[], uint32_t count);
    void RemoveSliceInfo(const uint32_t rankId) noexcept;

private:
    std::mutex mutex_;
    std::bitset<MAX_PEER_DEVICES> enablePeerDevices_;
    std::map<uint32_t, RegisterSlice> registerSlices_;
    std::map<uint32_t, RegisterSlice> remoteSlices_;
    std::map<uint32_t, std::vector<MemSlicePtr>> rankToRemoteSlices_;
    std::map<uint32_t, HbmExportDeviceInfo> importedDeviceInfo_;
    std::map<std::string, UserHbmExportSliceInfo> importedSliceInfo_;
    std::set<void *> registerAddrs_{};
    std::vector<std::string> memNames_{};
};
} // namespace mf
} // namespace ock

#endif // MF_HYBRID_HYBM_DEV_USER_LEGACY_SEGMENT_H
