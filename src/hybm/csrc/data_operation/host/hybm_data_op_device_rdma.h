/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef MF_HYBRID_HYBM_DATA_OP_DEVICE_RDMA_H
#define MF_HYBRID_HYBM_DATA_OP_DEVICE_RDMA_H

#include <cstdint>
#include <unordered_map>

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
    int32_t BatchCopyLH2GH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    int32_t BatchCopyGH2GH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    int32_t BatchCopyGH2GD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    int32_t BatchCopyGH2LH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    int32_t BatchCopyGD2GH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    int32_t BatchCopyGD2GD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;

    int32_t BatchDataCopyDefault(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                const ExtOptions &options) noexcept;
    int32_t BatchDataCopyLocal(hybm_batch_copy_params &params, int32_t direction,
                               const ExtOptions &options) noexcept;
    int32_t BatchDataCopyLocalSync(hybm_batch_copy_params &params, int32_t direction,
                                   const ExtOptions &options) noexcept;
    int32_t BatchDataCopyLocalAsync(hybm_batch_copy_params &params, int32_t direction,
                                    const ExtOptions &options) noexcept;
    int32_t BatchDataCopyLocalBatch(hybm_batch_copy_params &params, int32_t direction,
                                    const ExtOptions &options) noexcept;

    int32_t AllocSwapMemory();
    void FreeSwapMemory();

    void ClassifyDataAddr(void **globalAddrs, void **localAddrs, const uint64_t *counts, uint32_t batchSize,
                          std::unordered_map<uint32_t, CopyDescriptor> &registered,
                          std::unordered_map<uint32_t, CopyDescriptor> &localed,
                          std::unordered_map<uint32_t, CopyDescriptor> &notRegistered,
                          uint32_t globalRankId) noexcept;
    int32_t BatchCopyWrite(hybm_batch_copy_params &params, const ExtOptions &options,
                           hybm_data_copy_direction direction) noexcept;
    int32_t BatchCopyRead(hybm_batch_copy_params &params, const ExtOptions &options,
                          hybm_data_copy_direction direction) noexcept;
    int32_t BatchCopyG2G(hybm_batch_copy_params &params, const ExtOptions &options,
                         hybm_data_copy_direction direction) noexcept;
    int32_t SafePut(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options, bool isLocalHost);
    int32_t SafeGet(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options, bool isLocalHost);

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
