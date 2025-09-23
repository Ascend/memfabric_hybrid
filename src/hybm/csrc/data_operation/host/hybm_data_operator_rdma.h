/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MF_HYBRID_HYBM_DATA_OPERATOR_RDMA_H
#define MF_HYBRID_HYBM_DATA_OPERATOR_RDMA_H

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
    int32_t RtMemoryCopyAsync(const void *srcVA, void *destVA, uint64_t length,
                              uint32_t kind, const ExtOptions &options);
    int32_t RtMemoryCopy2dAsync(hybm_copy_2d_params &params, uint32_t kind, const ExtOptions &options);
    int PrepareThreadLocalStream() noexcept;

private:
    uint32_t rankId_{0};
    bool inited_ = false;
    static thread_local void *stream_;
    void *rdmaSwapBaseAddr_{nullptr};
    std::shared_ptr<transport::TransportManager> transportManager_;
    std::shared_ptr<RbtreeRangePool> rdmaSwapMemoryAllocator_;
};
}
}
#endif  // MF_HYBRID_HYBM_DATA_OPERATOR_RDMA_H
