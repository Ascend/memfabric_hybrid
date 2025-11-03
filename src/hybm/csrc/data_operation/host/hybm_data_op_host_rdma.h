/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_HYBRID_HYBM_DATA_OP_HOST_RDMA_H
#define MF_HYBRID_HYBM_DATA_OP_HOST_RDMA_H

#include <unordered_map>
#include "hybm_data_operator.h"
#include "hybm_mem_segment.h"
#include "hybm_transport_manager.h"
#include "hybm_rbtree_range_pool.h"

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

    int32_t CopyHost2Gva2d(hybm_copy_2d_params &params, const ExtOptions &options);
    int32_t CopyGva2Host2d(hybm_copy_2d_params &params, const ExtOptions &options);
    int32_t CopyDevice2Gva2d(hybm_copy_2d_params &params, const ExtOptions &options);
    int32_t CopyGva2Device2d(hybm_copy_2d_params &params, const ExtOptions &options);
    int32_t CopyGva2Gva2d(hybm_copy_2d_params &params, const ExtOptions &options);

    int BatchCopyLD2LH(void *hostAddrs[], void *deviceAddrs[], const uint64_t counts[],
                       uint32_t batchSize, const ExtOptions &options) noexcept;
    int BatchCopyLH2LD(void *deviceAddrs[], void *hostAddrs[], const uint64_t counts[],
                       uint32_t batchSize, const ExtOptions &options) noexcept;
    int BatchCopyLD2GH(void *gvaAddrs[], void *deviceAddrs[], const uint64_t counts[],
                       uint32_t batchSize, const ExtOptions &options) noexcept;
    int BatchCopyGH2LD(void *deviceAddrs[], void *gvaAddrs[], const uint64_t counts[],
                       uint32_t batchSize, const ExtOptions &options) noexcept;
    int BatchCopyLH2GH(void *gvaAddrs[], void *hostAddrs[], const uint64_t counts[],
                       uint32_t batchSize, const ExtOptions &options) noexcept;
    int BatchCopyGH2LH(void *hostAddrs[], void *gvaAddrs[], const uint64_t counts[],
                       uint32_t batchSize, const ExtOptions &options) noexcept;

    void ClassifyDataAddr(void **globalAddrs, void **localAddrs, const uint64_t *counts, uint32_t batchSize,
                          std::unordered_map<uint32_t, CopyDescriptor> &rmtRankMap,
                          std::unordered_map<uint32_t, CopyDescriptor> &localRankMap) noexcept;
    int BatchWriteLD2RH(uint32_t rmtRankId, CopyDescriptor &rmtCopyDescriptor, const ExtOptions &options) noexcept;
    int BatchReadRH2LD(uint32_t rmtRankId, CopyDescriptor &rmtCopyDescriptor, const ExtOptions &options) noexcept;

private:
    bool inited_{false};
    uint32_t rankId_{0};
    void *rdmaSwapBaseAddr_{nullptr};
    std::shared_ptr<transport::TransportManager> transportManager_;
    std::shared_ptr<RbtreeRangePool> rdmaSwapMemoryAllocator_;
};
}
}
#endif // MF_HYBRID_HYBM_DATA_OP_HOST_RDMA_H
