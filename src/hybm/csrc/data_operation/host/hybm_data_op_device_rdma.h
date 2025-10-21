/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_HYBM_DATA_OP_DEVICE_RDMA_H
#define MF_HYBRID_HYBM_DATA_OP_DEVICE_RDMA_H

#include <cstdint>

#include "hybm_data_operator.h"
#include "hybm_transport_manager.h"
#include "hybm_rbtree_range_pool.h"

namespace ock {
namespace mf {
class DataOpDeviceRDMA : public DataOperator {
public:
    DataOpDeviceRDMA(uint32_t rankId, std::shared_ptr<transport::TransportManager> tm) noexcept;
    ~DataOpDeviceRDMA() override;
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
    int32_t CopyLH2GH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    int32_t CopyLH2GD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    int32_t CopyLD2GH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    int32_t CopyLD2GD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    int32_t CopyGH2LH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    int32_t CopyGD2LH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    int32_t CopyGH2LD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    int32_t CopyGD2LD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    int32_t CopyGH2GH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    int32_t CopyGD2GH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    int32_t CopyGH2GD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    int32_t CopyGD2GD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    int32_t CopyLH2LH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    int32_t CopyLD2LD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    int32_t CopyLH2LD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    int32_t CopyLD2LH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    int32_t CopyRDMA(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;

    int32_t BatchCopyLH2GD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    int32_t BatchCopyGD2LH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    int32_t BatchCopyLD2GD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    int32_t BatchCopyLD2GH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    int32_t BatchCopyGH2LD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    int32_t BatchCopyGD2LD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;

    int32_t BatchDataCopyDefault(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                const ExtOptions &options) noexcept;

    int32_t AllocSwapMemory();
    void FreeSwapMemory();

private:
    bool inited_{false};
    uint32_t rankId_{0};
    std::shared_ptr<transport::TransportManager> transportManager_;
    void *rdmaSwapBaseAddr_{nullptr};
    std::shared_ptr<RbtreeRangePool> rdmaSwapMemoryAllocator_;
};
}  // namespace mf
}  // namespace ock

#endif  // MF_HYBRID_HYBM_DATA_OP_DEVICE_RDMA_H
