/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H
#define MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H

#include "hybm_data_operator.h"

namespace ock {
namespace mf {
class HostDataOpSDMA : public DataOperator {
public:
    explicit HostDataOpSDMA(void *stm) noexcept;

    int32_t DataCopy(const void *srcVA, void *destVA, uint64_t length, hybm_data_copy_direction direction, void *stream,
                     uint32_t flags) noexcept override;
    int32_t DataCopy2d(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch, uint64_t width,
                       uint64_t height, hybm_data_copy_direction direction, void *stream,
                       uint32_t flags) noexcept override;
    int32_t DataCopyAsync(const void *srcVA, void *destVA, uint64_t length, hybm_data_copy_direction direction,
                          void *stream, uint32_t flags) noexcept override;

    int32_t Wait(int32_t waitId) noexcept override;

private:
    int CopyHost2Gva(void *gvaAddr, const void *hostAddr, size_t count, void *stream) noexcept;
    int CopyDevice2Gva(void *gvaAddr, const void *deviceAddr, size_t count, void *stream) noexcept;
    int CopyGva2Host(void *hostAddr, const void *gvaAddr, size_t count, void *stream) noexcept;
    int CopyGva2Device(void *deviceAddr, const void *gvaAddr, size_t count, void *stream) noexcept;

    int CopyHost2Gva2d(void *gvaAddr, uint64_t dpitch, const void *hostAddr, uint64_t spitch, size_t width,
                       uint64_t height, void *stream) noexcept;
    int CopyDevice2Gva2d(void *gvaAddr, uint64_t dpitch, const void *hostAddr, uint64_t spitch, size_t width,
                         uint64_t height, void *stream) noexcept;
    int CopyGva2Host2d(void *gvaAddr, uint64_t dpitch, const void *hostAddr, uint64_t spitch, size_t width,
                       uint64_t height, void *stream) noexcept;
    int CopyGva2Device2d(void *gvaAddr, uint64_t dpitch, const void *hostAddr, uint64_t spitch, size_t width,
                         uint64_t height, void *stream) noexcept;

private:
    void *stream_;
};
}  // namespace mf
}  // namespace ock

#endif  // MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H
