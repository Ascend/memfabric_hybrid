/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H
#define MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H

#include <unordered_map>
#include "mf_rwlock.h"
#include "hybm_data_operator.h"
#include "hybm_stream.h"
#include "hybm_rbtree_range_pool.h"

namespace ock {
namespace mf {
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
    int32_t Wait(int32_t waitId) noexcept override;

    void CleanUp() noexcept override;

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
    bool IsResetStream() noexcept;

    int PrepareThreadLocalStream() noexcept;

private:
    bool inited_ = false;
    static thread_local HybmStreamPtr stream_;
    ReadWriteLock lock_;
    std::unordered_map<uint64_t, bool> streamMask_;
};
}
}

#endif  // MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H
