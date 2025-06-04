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
    HostDataOpSDMA(void *stm) noexcept;

    int32_t DataCopy(const void *srcVA, void *destVA, uint64_t length, DataOpDirection direction,
                     uint32_t flags) noexcept override;

private:
    int CopyHost2Gva(void *gvaAddr, const void *hostAddr, size_t count) noexcept;
    int CopyDevice2Gva(void *gvaAddr, const void *deviceAddr, size_t count) noexcept;
    int CopyGva2Host(void *hostAddr, const void *gvaAddr, size_t count) noexcept;
    int CopyGva2Device(void *deviceAddr, const void *gvaAddr, size_t count) noexcept;

private:
    void *stream_;
};
}
}

#endif  // MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H
