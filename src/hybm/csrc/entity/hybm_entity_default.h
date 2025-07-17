/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_ENGINE_IMPL_H
#define MEM_FABRIC_HYBRID_HYBM_ENGINE_IMPL_H

#include <map>
#include "hybm_common_include.h"
#include "hybm_devide_mem_segment.h"
#include "hybm_data_operator.h"
#include "hybm_mem_segment.h"
#include "hybm_entity.h"
#include "hybm_trans_manager.h"

namespace ock {
namespace mf {
class MemEntityDefault : public MemEntity {
public:
    explicit MemEntityDefault(int32_t id) noexcept;
    ~MemEntityDefault() override;

    int32_t Initialize(const hybm_options *options) noexcept override;
    void UnInitialize() noexcept override;

    int32_t ReserveMemorySpace(void **reservedMem) noexcept override;
    int32_t UnReserveMemorySpace() noexcept override;

    int32_t AllocLocalMemory(uint64_t size, uint32_t flags, hybm_mem_slice_t &slice) noexcept override;
    void FreeLocalMemory(hybm_mem_slice_t slice, uint32_t flags) noexcept override;

    int32_t ExportExchangeInfo(hybm_exchange_info &desc, uint32_t flags) noexcept override;
    int32_t ExportExchangeInfo(hybm_mem_slice_t slice, hybm_exchange_info &desc, uint32_t flags) noexcept override;
    int32_t ImportExchangeInfo(const hybm_exchange_info *desc, uint32_t count, uint32_t flags) noexcept override;
    int32_t RemoveImported(const std::vector<uint32_t>& ranks) noexcept override;

    int32_t SetExtraContext(const void *context, uint32_t size) noexcept override;

    int32_t Mmap() noexcept override;
    void Unmap() noexcept override;

    bool CheckAddressInEntity(const void *ptr, uint64_t length) const noexcept override;
    int32_t CopyData(const void *src, void *dest, uint64_t length, hybm_data_copy_direction direction,
                     void *stream, uint32_t flags) noexcept override;
    int32_t CopyData2d(const void *src, uint64_t spitch, void *dest, uint64_t dpitch, uint64_t width, uint64_t height,
                       hybm_data_copy_direction direction, void *stream, uint32_t flags) noexcept override;
    int32_t TransportInit(uint32_t rankId, const std::string &nic) noexcept override;
    int32_t TransportRegisterMr(uint64_t address, uint64_t size,
                                hybm_mr_key *lkey, hybm_mr_key *rkey) noexcept override;
    int32_t TransportSetMr(const std::vector<hybm_transport_mr_info> &mrs) noexcept override;
    int32_t TransportGetAddress(std::string &nic) noexcept override;
    int32_t TransportSetAddress(const std::vector<std::string> &nics) noexcept override;
    int32_t TransportMakeConnect() noexcept override;
    int32_t TransportAiQPInfoAddress(uint32_t shmId, void **address) noexcept override;

private:
    static int CheckOptions(const hybm_options *options) noexcept;

    void SetHybmDeviceInfo(HybmDeviceMeta &info);

private:
    bool initialized;
    const int32_t id_; /* id of the engine */
    hybm_options options_{};
    void *stream_{nullptr};
    std::shared_ptr<MemSegment> segment_;
    std::shared_ptr<DataOperator> dataOperator_;
    std::shared_ptr<HybmTransManager> transportManager_;
};
using EngineImplPtr = std::shared_ptr<MemEntityDefault>;
}
}

#endif  // MEM_FABRIC_HYBRID_HYBM_ENGINE_IMPL_H
