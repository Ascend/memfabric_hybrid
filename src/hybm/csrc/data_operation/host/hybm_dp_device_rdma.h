/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_HYBM_DP_DEVICE_RDMA_H
#define MF_HYBRID_HYBM_DP_DEVICE_RDMA_H

#include "hybm_data_operator.h"
#include "hybm_mem_segment.h"
#include "hybm_transport_manager.h"

namespace ock {
namespace mf {
class DataOpDeviceRDMA : public DataOperator {
public:
    DataOpDeviceRDMA(uint32_t rankId, std::shared_ptr<transport::TransportManager> tm) noexcept;
    ~DataOpDeviceRDMA() override = default;
    int32_t Initialize() noexcept override;
    void UnInitialize() noexcept override;
    int32_t DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                     const ExtOptions &options) noexcept override;
    int32_t DataCopy2d(hybm_copy_2d_params &params, hybm_data_copy_direction direction,
                       const ExtOptions &options) noexcept override;
    int32_t DataCopyAsync(hybm_copy_params &params, hybm_data_copy_direction direction,
                          const ExtOptions &options) noexcept override;
    int32_t Wait(int32_t waitId) noexcept override;
private:
    uint32_t rankId_{0};
    std::shared_ptr<transport::TransportManager> transportManager_;
};
}
}

#endif  // MF_HYBRID_HYBM_DP_DEVICE_RDMA_H
