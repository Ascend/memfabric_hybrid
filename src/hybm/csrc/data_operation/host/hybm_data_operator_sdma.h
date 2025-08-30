/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H
#define MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H

#include "hybm_data_operator.h"

namespace ock {
namespace mf {
class HostDataOpSDMA : public DataOperator {
public:
    ~HostDataOpSDMA() override;

    int32_t Initialize() noexcept override;
    void UnInitialize() noexcept override;

    int32_t DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                     const ExtOptions &options) noexcept override;
    int32_t DataCopy2d(hybm_copy_2d_params &params, hybm_data_copy_direction direction,
                       const ExtOptions &options) noexcept override;
    int32_t DataCopyAsync(hybm_copy_params &params, hybm_data_copy_direction direction,
                          const ExtOptions &options) noexcept override;
    int32_t BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                          const ExtOptions &options) noexcept override;
    int32_t Wait(int32_t waitId) noexcept override;

private:
    int CopyHost2Gva(void *gvaAddr, const void *hostAddr, size_t count, void *stream) noexcept;
    int CopyDevice2Gva(void *gvaAddr, const void *deviceAddr, size_t count, void *stream) noexcept;
    int CopyGva2Host(void *hostAddr, const void *gvaAddr, size_t count, void *stream) noexcept;
    int CopyGva2Device(void *deviceAddr, const void *gvaAddr, size_t count, void *stream) noexcept;

    int BatchCopyDevice2Gva(void *gvaAddrs[], const void *deviceAddrs[], const size_t counts[],
                            uint32_t batchSize, void *stream) noexcept;
    int BatchCopyGva2Device(void *deviceAddrs[], const void *gvaAddrs[], const size_t counts[],
                            uint32_t batchSize, void *stream) noexcept;
    int BatchCopyHost2Gva(void *gvaAddrs[], const void *deviceAddrs[], const size_t counts[],
                          uint32_t batchSize, void *stream) noexcept;
    int BatchCopyGva2Host(void *deviceAddrs[], const void *gvaAddrs[], const size_t counts[],
                          uint32_t batchSize, void *stream) noexcept;

    int CopyHost2Gva2d(hybm_copy_2d_params &params, void *stream) noexcept;
    int CopyDevice2Gva2d(hybm_copy_2d_params &params, void *stream) noexcept;
    int CopyGva2Host2d(hybm_copy_2d_params &params, void *stream) noexcept;
    int CopyGva2Device2d(hybm_copy_2d_params &params, void *stream) noexcept;
    int CheckDevice2Gva2dStatus(hybm_copy_2d_params &params) noexcept;
    int PrepareThreadLocalStream() noexcept;

private:
    bool inited_ = false;
    static thread_local void *stream_;
};
}  // namespace mf
}  // namespace ock

#endif  // MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H
