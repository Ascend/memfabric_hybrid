/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
*/
#ifndef MF_HYBRID_HYBM_DATA_OPERATOR_RDMA_H
#define MF_HYBRID_HYBM_DATA_OPERATOR_RDMA_H

#include "hybm_data_operator.h"
#include "hybm_mem_segment.h"
#include "hybm_transport_manager.h"
#include "rbtree_range_pool.h"

namespace ock {
namespace mf {

class HostDataOpRDMA : public DataOperator {
public:
    HostDataOpRDMA(uint32_t rankId, std::shared_ptr<transport::TransportManager> &transportManager) noexcept
        :rankId_(rankId), transportManager_(transportManager) {};

    ~HostDataOpRDMA() override;

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
    int32_t CopyHost2Gva(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options);
    int32_t CopyGva2Host(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options);
    int32_t CopyDevice2Gva(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options);
    int32_t CopyGva2Device(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options);
    int32_t CopyGva2Gva(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options);

    int32_t CopyHost2Gva2d(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch,
                           uint64_t width, uint64_t height, const ExtOptions &options);
    int32_t CopyGva2Host2d(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch,
                           uint64_t width, uint64_t height, const ExtOptions &options);
    int32_t CopyDevice2Gva2d(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch,
                             uint64_t width, uint64_t height, const ExtOptions &options);
    int32_t CopyGva2Device2d(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch,
                             uint64_t width, uint64_t height, const ExtOptions &options);
    int32_t CopyGva2Gva2d(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch,
                          uint64_t width, uint64_t height, const ExtOptions &options);
    int32_t RtMemoryCopyAsync(const void *srcVA, void *destVA, uint64_t length,
                              uint32_t kind, const ExtOptions &options);
    int32_t RtMemoryCopy2dAsync(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch,
                                uint64_t width, uint64_t height,  uint32_t kind, const ExtOptions &options);

private:
    uint32_t rankId_{0};
    bool inited_ = false;
    void *stream_{nullptr};
    void *rdmaSwapBaseAddr_{nullptr};
    std::shared_ptr<transport::TransportManager> transportManager_;
    std::shared_ptr<RbtreeRangePool> rdmaSwapMemoryAllocator_;
};
}
}
#endif  // MF_HYBRID_HYBM_DATA_OPERATOR_RDMA_H
