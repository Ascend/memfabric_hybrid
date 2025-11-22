/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H
#define MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H

#include "hybm_data_operator.h"
#include "hybm_stream.h"
#include "hybm_rbtree_range_pool.h"

namespace ock {
namespace mf {
#ifdef USE_VMM
class HostDataOpSDMA : public DataOperator {
public:
    explicit HostDataOpSDMA() noexcept;
    ~HostDataOpSDMA() override;

    int32_t Initialize() noexcept override;
    void UnInitialize() noexcept override;

    int32_t DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                     const ExtOptions &options) noexcept override;
    int32_t DataCopyAsync(hybm_copy_params &params, hybm_data_copy_direction direction,
                          const ExtOptions &options) noexcept override;
    int32_t BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                          const ExtOptions &options) noexcept override;
    int32_t DataCopy2d(hybm_copy_2d_params &params, hybm_data_copy_direction direction,
                       const ExtOptions &options) noexcept override;
    int32_t Wait(int32_t waitId) noexcept override;

private:
    int CopyLH2GD(void* gvaAddr, const void* hostAddr, size_t count, void* stream) noexcept;
    int CopyGD2LH(void* hostAddr, const void* gvaAddr, size_t count, void* stream) noexcept;
    int CopyLH2GH(void* destVA, const void* srcVA, uint64_t length, void* stream) noexcept;
    int CopyGH2LH(void* destVA, const void* srcVA, uint64_t length, void* stream) noexcept;

    void InitG2GStreamTask(StreamTask &task) noexcept;
    int CopyG2G(void *destVA, const void *srcVA, size_t count) noexcept;
    int BatchCopyG2G(void *destVAs[], void *srcVAs[], const uint64_t counts[], uint32_t batchSize) noexcept;

    int CopyG2GAsync(void *destVA, const void *srcVA, size_t count) noexcept;

    int BatchCopyLH2GD(void *gvaAddrs[], void *hostAddrs[], const uint64_t counts[],
                       uint32_t batchSize, void *stream) noexcept;
    int BatchCopyGD2LH(void *hostAddrs[], void *gvaAddrs[], const uint64_t counts[],
                       uint32_t batchSize, void *stream) noexcept;
    int BatchCopyLH2GH(void *gvaAddrs[], void *hostAddrs[], const uint64_t counts[],
                       uint32_t batchSize, void *stream) noexcept;
    int BatchCopyGH2LH(void *hostAddrs[], void *gvaAddrs[], const uint64_t counts[],
                       uint32_t batchSize, void *stream) noexcept;

private:
    bool inited_ = false;
};
#else
class HostDataOpSDMA : public DataOperator {
public:
    explicit HostDataOpSDMA() noexcept;
    ~HostDataOpSDMA() override;

    int32_t Initialize() noexcept override;
    void UnInitialize() noexcept override;

    int32_t DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                     const ExtOptions &options) noexcept override;
    int32_t DataCopyAsync(hybm_copy_params &params, hybm_data_copy_direction direction,
                          const ExtOptions &options) noexcept override;
    int32_t BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                          const ExtOptions &options) noexcept override;
    int32_t DataCopy2d(hybm_copy_2d_params &params, hybm_data_copy_direction direction,
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
    int BatchCopyG2G(void *destVAs[], void *srcVAs[], const uint64_t counts[], uint32_t batchSize) noexcept;

    int CopyLD2GHAsync(void *destVA, const void *srcVA, uint64_t length, void *stream) noexcept;
    int CopyGH2LDAsync(void *destVA, const void *srcVA, uint64_t length, void *stream) noexcept;
    int CopyG2GAsync(void *destVA, const void *srcVA, size_t count) noexcept;
    int BatchCopyLH2GD(void *destinations[], void *sources[], const uint64_t counts[], uint32_t batchSize,
                       void *stream) noexcept;
    int BatchCopyGD2LH(void *destinations[], void *sources[], const uint64_t counts[], uint32_t batchSize,
                       void *stream) noexcept;
    int BatchCopyLD2GH(void *destinations[], void *sources[], const uint64_t counts[], uint32_t batchSize,
                       void *stream) noexcept;
    int BatchCopyGH2LD(void *destinations[], void *sources[], const uint64_t counts[], uint32_t batchSize,
                       void *stream) noexcept;
    int BatchCopyLD2GD(void *destinations[], void *sources[], const uint64_t counts[], uint32_t batchSize,
                       void *stream) noexcept;
    int BatchCopyGD2LD(void *destinations[], void *sources[], const uint64_t counts[], uint32_t batchSize,
                       void *stream) noexcept;
    int BatchCopyLH2GH(void *destinations[], void *sources[], const uint64_t counts[], uint32_t batchSize,
                       void *stream) noexcept;
    int BatchCopyGH2LH(void *destinations[], void *sources[], const uint64_t counts[], uint32_t batchSize,
                       void *stream) noexcept;
    int BatchCopyGH2GH(void *destinations[], void *sources[], const uint64_t counts[], uint32_t batchSize,
                       void *stream) noexcept;
    int BatchCopyGH2GD(void *destinations[], void *sources[], const uint64_t counts[], uint32_t batchSize,
                       void *stream) noexcept;
    int BatchCopyGD2GH(void *destinations[], void *sources[], const uint64_t counts[], uint32_t batchSize,
                       void *stream) noexcept;
    int BatchCopyGD2GD(void *destinations[], void *sources[], const uint64_t counts[], uint32_t batchSize,
                       void *stream) noexcept;

    int BatchWriteNotRegisterData(CopyDescriptor &notRegistered, void *stream, uint32_t direction) noexcept;
    int BatchReadNotRegisterData(CopyDescriptor &notRegistered, void *stream, uint32_t direction) noexcept;

    void ClassifyDataAddr(void **globalAddrs, void **localAddrs, const uint64_t *counts, uint32_t batchSize,
                          CopyDescriptor &registered, CopyDescriptor &notRegistered) noexcept;

private:
    bool inited_ = false;
    void *sdmaSwapMemAddr_ = nullptr;
    std::shared_ptr<RbtreeRangePool> sdmaSwapMemoryAllocator_;
};
#endif
}
}

#endif  // MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H
