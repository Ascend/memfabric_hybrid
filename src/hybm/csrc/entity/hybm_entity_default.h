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
#ifndef MEM_FABRIC_HYBRID_HYBM_ENGINE_IMPL_H
#define MEM_FABRIC_HYBRID_HYBM_ENGINE_IMPL_H

#include <map>
#include <mutex>
#include "hybm_common_include.h"
#include "hybm_dev_legacy_segment.h"
#include "hybm_data_operator.h"
#include "hybm_mem_segment.h"
#include "hybm_entity.h"

#include "hybm_transport_manager.h"

namespace ock {
namespace mf {
struct EntityExportInfo {
    uint64_t magic{ENTITY_EXPORT_INFO_MAGIC};
    uint64_t version{EXPORT_INFO_VERSION};
    uint16_t rankId{0};
    uint16_t role{0};
    uint32_t reserved{0};
    char nic[64]{};
    char tag[32]{};
};
struct SliceExportTransportKey {
    uint64_t magic;
    uint16_t rankId;
    uint16_t reserved[3]{};
    uint64_t address;
    transport::TransportMemoryKey key;
    SliceExportTransportKey() : SliceExportTransportKey{0, 0, 0} {}
    SliceExportTransportKey(uint64_t mag, uint16_t rank, uint64_t addr)
        : magic{mag}, rankId{rank}, address{addr}, key{0}
    {}
};

class MemEntityDefault : public MemEntity {
public:
    explicit MemEntityDefault(int32_t id) noexcept;
    ~MemEntityDefault() override;

    int32_t Initialize(const hybm_options *options) noexcept override;
    void UnInitialize() noexcept override;

    int32_t ReserveMemorySpace() noexcept override;
    int32_t UnReserveMemorySpace() noexcept override;
    void *GetReservedMemoryPtr(hybm_mem_type memType) noexcept override;

    int32_t AllocLocalMemory(uint64_t size, hybm_mem_type mType, uint32_t flags,
                             hybm_mem_slice_t &slice) noexcept override;
    int32_t RegisterLocalMemory(const void *ptr, uint64_t size, uint32_t flags,
                                hybm_mem_slice_t &slice) noexcept override;
    int32_t FreeLocalMemory(hybm_mem_slice_t slice, uint32_t flags) noexcept override;

    int32_t ExportExchangeInfo(ExchangeInfoWriter &desc, uint32_t flags) noexcept override;
    int32_t ExportExchangeInfo(hybm_mem_slice_t slice, ExchangeInfoWriter &desc, uint32_t flags) noexcept override;
    int32_t ImportExchangeInfo(const ExchangeInfoReader desc[], uint32_t count, void *addresses[],
                               uint32_t flags) noexcept override;
    int32_t ImportEntityExchangeInfo(const ExchangeInfoReader desc[], uint32_t count, uint32_t flags) noexcept override;
    int32_t RemoveImported(const std::vector<uint32_t> &ranks) noexcept override;
    int32_t GetExportSliceInfoSize(size_t &size) noexcept override;

    int32_t SetExtraContext(const void *context, uint32_t size) noexcept override;

    int32_t Mmap() noexcept override;
    void Unmap() noexcept override;

    bool CheckAddressInEntity(const void *ptr, uint64_t length) const noexcept override;
    int32_t CopyData(hybm_copy_params &params, hybm_data_copy_direction direction, void *stream,
                     uint32_t flags) noexcept override;
    int32_t BatchCopyData(hybm_batch_copy_params &params, hybm_data_copy_direction direction, void *stream,
                          uint32_t flags) noexcept override;
    int32_t Wait() noexcept override;
    bool SdmaReaches(uint32_t remoteRank) const noexcept override;
    hybm_data_op_type CanReachDataOperators(uint32_t remoteRank) const noexcept override;

private:
    static int CheckOptions(const hybm_options *options) noexcept;
    int LoadExtendLibrary() noexcept;
    int UpdateHybmDeviceInfo(uint32_t extCtxSize) noexcept;
    void SetHybmDeviceInfo(HybmDeviceMeta &info);
    int ImportForTransport(const ExchangeInfoReader desc[], uint32_t count) noexcept;
    void GenCopyExtOption(void *&src, void *&dest, uint64_t length, ExtOptions &options) noexcept;

    Result InitSegment();
    Result InitHbmSegment();
    Result InitDramSegment();
    Result InitTransManager();
    Result InitDataOperator();
    Result InitTagManager();

    void ReleaseResources();
    int32_t SetThreadAclDevice();
    int32_t ExportWithoutSlice(ExchangeInfoWriter &desc, uint32_t flags);
    int32_t ImportForTagManager();
    int32_t ImportForTransportManager();
    int32_t ImportForTransportPrecheck(const ExchangeInfoReader *desc, uint32_t &count, bool &importInfoEntity);

private:
    static thread_local bool isSetDevice_;
    bool initialized_{false};
    const int32_t id_; /* id of the engine */
    hybm_options options_{};
    void *hbmGva_{nullptr};
    void *dramGva_{nullptr};
    std::shared_ptr<MemSegment> hbmSegment_{nullptr};
    std::shared_ptr<MemSegment> dramSegment_{nullptr};
    std::shared_ptr<DataOperator> dataOperator_;
    bool transportPrepared_{false};
    std::mutex importMutex_;
    transport::TransManagerPtr transportManager_;
    std::unordered_map<uint32_t, EntityExportInfo> importedRanks_;
    std::unordered_map<uint32_t, std::vector<transport::TransportMemoryKey>> importedMemories_;
    HybmEntityTagInfoPtr tagManager_;
};
using EngineImplPtr = std::shared_ptr<MemEntityDefault>;
} // namespace mf
} // namespace ock

#endif // MEM_FABRIC_HYBRID_HYBM_ENGINE_IMPL_H