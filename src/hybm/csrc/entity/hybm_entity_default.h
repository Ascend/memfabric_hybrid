/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_ENGINE_IMPL_H
#define MEM_FABRIC_HYBRID_HYBM_ENGINE_IMPL_H

#include <map>
#include "hybm_common_include.h"
#include "hybm_device_mem_segment.h"
#include "hybm_data_operator.h"
#include "hybm_mem_segment.h"
#include "hybm_entity.h"

#include "hybm_transport_manager.h"

namespace ock {
namespace mf {
struct EntityExportInfo {
    uint64_t magic{0};
    uint64_t version{0};
    uint32_t rankId{0};
    char nic[64]{};
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
    int32_t RegisterLocalMemory(const void *ptr, uint64_t size, uint32_t flags, hybm_mem_slice_t &slice) noexcept override;
    int32_t FreeLocalMemory(hybm_mem_slice_t slice, uint32_t flags) noexcept override;

    int32_t ExportExchangeInfo(hybm_exchange_info &desc, uint32_t flags) noexcept override;
    int32_t ExportExchangeInfo(hybm_mem_slice_t slice, hybm_exchange_info &desc, uint32_t flags) noexcept override;
    int32_t ImportExchangeInfo(const hybm_exchange_info *desc, uint32_t count, void *addresses[],
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
    int32_t ImportEntityExchangeInfo(const hybm_exchange_info desc[],
                                     uint32_t count, uint32_t flags) noexcept override;
    bool SdmaReaches(uint32_t remoteRank) const noexcept override;

private:
    static int CheckOptions(const hybm_options *options) noexcept;
    int UpdateHybmDeviceInfo(uint32_t extCtxSize) noexcept;
    void SetHybmDeviceInfo(HybmDeviceMeta &info);

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
    bool transportPrepared{false};
    transport::TransManagerPtr transportManager_;
    std::unordered_map<uint32_t, std::vector<transport::TransportMemoryKey>> importedMemories_;
};
using EngineImplPtr = std::shared_ptr<MemEntityDefault>;
}
}

#endif  // MEM_FABRIC_HYBRID_HYBM_ENGINE_IMPL_H
