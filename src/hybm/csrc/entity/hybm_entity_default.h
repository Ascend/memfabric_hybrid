/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_ENGINE_IMPL_H
#define MEM_FABRIC_HYBRID_HYBM_ENGINE_IMPL_H

#include <map>
#include <mutex>
#include "hybm_common_include.h"
#include "hybm_device_mem_segment.h"
#include "hybm_data_operator.h"
#include "hybm_mem_segment.h"
#include "hybm_entity.h"

#include "hybm_transport_manager.h"

namespace ock {
namespace mf {
struct EntityExportInfo {
    uint64_t magic{EXPORT_INFO_MAGIC};
    uint64_t version{0};
    uint16_t rankId{0};
    uint16_t role{0};
    uint32_t reserved{0};
    char nic[64]{};
};

struct SliceExportTransportKey {
    uint64_t magic{EXPORT_SLICE_MAGIC};
    uint16_t rankId;
    uint16_t reserved[3]{};
    uint64_t address;
    transport::TransportMemoryKey key;
    SliceExportTransportKey() : SliceExportTransportKey{0, 0} {}
    SliceExportTransportKey(uint16_t rank, uint64_t addr) : rankId{rank}, address{addr} {}
};

class MemEntityDefault : public MemEntity {
public:
    explicit MemEntityDefault(int32_t id) noexcept;
    ~MemEntityDefault() override;

    int32_t Initialize(const hybm_options *options) noexcept override;
    void UnInitialize() noexcept override;

    int32_t ReserveMemorySpace(void **reservedMem) noexcept override;
    int32_t UnReserveMemorySpace() noexcept override;

    int32_t AllocLocalMemory(uint64_t size, uint32_t flags, hybm_mem_slice_t &slice) noexcept override;
    int32_t RegisterLocalMemory(const void *ptr, uint64_t size, uint32_t flags,
                                hybm_mem_slice_t &slice) noexcept override;
    int32_t FreeLocalMemory(hybm_mem_slice_t slice, uint32_t flags) noexcept override;

    int32_t ExportExchangeInfo(ExchangeInfoWriter &desc, uint32_t flags) noexcept override;
    int32_t ExportExchangeInfo(hybm_mem_slice_t slice, ExchangeInfoWriter &desc, uint32_t flags) noexcept override;
    int32_t ImportExchangeInfo(const ExchangeInfoReader *desc, uint32_t count, void *addresses[],
                               uint32_t flags) noexcept override;
    int32_t RemoveImported(const std::vector<uint32_t>& ranks) noexcept override;
    int32_t GetExportSliceInfoSize(size_t &size) noexcept override;

    int32_t SetExtraContext(const void *context, uint32_t size) noexcept override;

    int32_t Mmap() noexcept override;
    void Unmap() noexcept override;

    bool CheckAddressInEntity(const void *ptr, uint64_t length) const noexcept override;
    int32_t CopyData(hybm_copy_params &params, hybm_data_copy_direction direction,
                     void *stream, uint32_t flags) noexcept override;
    int32_t CopyData2d(hybm_copy_2d_params &params, hybm_data_copy_direction direction,
                       void *stream, uint32_t flags) noexcept override;
    int32_t BatchCopyData(hybm_batch_copy_params &params,
                          hybm_data_copy_direction direction, void *stream, uint32_t flags) noexcept override;
    bool SdmaReaches(uint32_t remoteRank) const noexcept override;

private:
    static int CheckOptions(const hybm_options *options) noexcept;
    int UpdateHybmDeviceInfo(uint32_t extCtxSize) noexcept;
    void SetHybmDeviceInfo(HybmDeviceMeta &info);

    int32_t ExportWithSlice(hybm_mem_slice_t slice, ExchangeInfoWriter &desc, uint32_t flags) noexcept;
    int32_t ExportWithoutSlice(ExchangeInfoWriter &desc, uint32_t flags) noexcept;
    int32_t ImportForTransport(const ExchangeInfoReader desc[], uint32_t count, uint32_t flags) noexcept;

    Result InitSegment();
    Result InitHbmSegment();
    Result InitDramSegment();
    Result InitTransManager();
    Result InitDataOperator();

    void ReleaseResources();
    int32_t SetThreadAclDevice();

private:
    bool initialized = false;
    const int32_t id_; /* id of the engine */
    static thread_local bool isSetDevice_;
    hybm_options options_{};
    std::shared_ptr<MemSegment> segment_;
    std::shared_ptr<DataOperator> dataOperator_;
    std::shared_ptr<DataOperator> sdmaDataOperator_;
    transport::TransManagerPtr transportManager_;
    std::mutex importMutex_;
    std::unordered_map<uint32_t, EntityExportInfo> importedRanks_;
    std::unordered_map<uint32_t, std::unordered_map<uint64_t, transport::TransportMemoryKey>> importedMemories_;
};
using EngineImplPtr = std::shared_ptr<MemEntityDefault>;
}
}

#endif  // MEM_FABRIC_HYBRID_HYBM_ENGINE_IMPL_H
