/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H
#define MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H

#include "hybm_data_operator.h"
#include "hybm_stream.h"
#include "rbtree_range_pool.h"

namespace ock {
namespace mf {
class HostDataOpSDMA : public DataOperator {
public:
    HostDataOpSDMA(void *stm) noexcept;
    ~HostDataOpSDMA() override = default;

    int32_t Initialize() noexcept override;
    void UnInitialize() noexcept override;

    int32_t DataCopy(const void *srcVA, void *destVA, uint64_t length, hybm_data_copy_direction direction,
                     const ExtOptions &options) noexcept override;

    int32_t DataCopy2d(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch,
                       uint64_t width,uint64_t height, hybm_data_copy_direction direction,
                       const ExtOptions &options) noexcept override;
    int32_t DataCopyAsync(const void* srcVA, void* destVA, uint64_t length, hybm_data_copy_direction direction,
                          const ExtOptions &options) noexcept override;
    int32_t BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                          const ExtOptions &options) noexcept override;

    int32_t Wait(int32_t waitId) noexcept override;

private:
    int CopyLH2GD(void* gvaAddr, const void* hostAddr, size_t count, void* stream) noexcept;
    int CopyLD2GD(void* gvaAddr, const void* deviceAddr, size_t count, void* stream) noexcept;
    int CopyGD2LH(void* hostAddr, const void* gvaAddr, size_t count, void* stream) noexcept;
    int CopyGD2LD(void* deviceAddr, const void* gvaAddr, size_t count, void* stream) noexcept;
    int CopyGD2GH(void* destVA, const void* srcVA, uint64_t length, void* stream) noexcept;
    int CopyGD2GD(void* gvaAddr, const void* deviceAddr, size_t count, void* stream) noexcept;
    int CopyGH2GD(void* destVA, const void* srcVA, uint64_t length, void* stream) noexcept;
    int CopyLD2GH(void* destVA, const void* srcVA, uint64_t length, void* stream) noexcept;
    int CopyLH2GH(void* destVA, const void* srcVA, uint64_t length, void* stream) noexcept;
    int CopyGH2LD(void* destVA, const void* srcVA, uint64_t length, void* stream) noexcept;
    int CopyGH2LH(void* destVA, const void* srcVA, uint64_t length, void* stream) noexcept;
    int CopyGH2GH(void* destVA, const void* srcVA, uint64_t length, void* stream) noexcept;

    void InitG2GStreamTask(StreamTask &task) noexcept;
    int CopyG2G(void *destVA, const void *srcVA, size_t count) noexcept;
    int CopyG2G2d(void* destVA, uint64_t dpitch, const void* srcVA, uint64_t spitch,
                  size_t width, uint64_t height) noexcept;
    int BatchCopyG2G(void *destVAs[], const void *srcVAs[], const uint32_t counts[], uint32_t batchSize) noexcept;

    int CopyLD2GHAsync(void *destVA, const void *srcVA, uint64_t length, void *stream) noexcept;
    int CopyGH2LDAsync(void *destVA, const void *srcVA, uint64_t length, void *stream) noexcept;
    int CopyG2GAsync(void *destVA, const void *srcVA, size_t count) noexcept;

    int CopyLH2GD2d(void* gvaAddr, uint64_t dpitch, const void* hostAddr, uint64_t spitch, size_t width,
                    uint64_t height, void* stream) noexcept;
    int CopyLD2GD2d(void* gvaAddr, uint64_t dpitch, const void* hostAddr, uint64_t spitch, size_t width,
                    uint64_t height, void* stream) noexcept;
    int CopyGD2LH2d(void* gvaAddr, uint64_t dpitch, const void* hostAddr, uint64_t spitch, size_t width,
                    uint64_t height, void* stream) noexcept;
    int CopyGD2LD2d(void* gvaAddr, uint64_t dpitch, const void* hostAddr, uint64_t spitch, size_t width,
                    uint64_t height, void* stream) noexcept;
    int CopyLD2GH2d(void* gvaAddr, uint64_t dpitch, const void* deviceAddr, uint64_t spitch, size_t width,
                    uint64_t height, void* stream) noexcept;
    int CopyGH2LD2d(void* deviceAddr, uint64_t dpitch, const void* gvaAddr, uint64_t spitch, size_t width,
                    uint64_t height, void* stream) noexcept;
    int BatchCopyLD2GH(void *gvaAddrs[], const void *deviceAddrs[], const uint32_t counts[],
                       uint32_t batchSize, void *stream) noexcept;
    int BatchCopyGH2LD(void *deviceAddrs[], const void *gvaAddrs[], const uint32_t counts[],
                       uint32_t batchSize, void *stream) noexcept;
    int BatchCopyLD2GD(void *gvaAddrs[], const void *deviceAddrs[], const uint32_t counts[],
                       uint32_t batchSize, void *stream) noexcept;
    int BatchCopyGD2LD(void *deviceAddrs[], const void *gvaAddrs[], const uint32_t counts[],
                       uint32_t batchSize, void *stream) noexcept;

private:
    void *stream_;
    bool inited_ = false;
    HybmStreamPtr hybmStream_ = nullptr;
    void *sdmaSwapMemAddr_ = nullptr;
    std::shared_ptr<RbtreeRangePool> sdmaSwapMemoryAllocator_;
};
}
}

#endif  // MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H
