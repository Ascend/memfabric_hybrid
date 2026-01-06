/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
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
    Result Initialize() noexcept override;
    void UnInitialize() noexcept override;
    Result DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                    const ExtOptions &options) noexcept override;
    Result DataCopyAsync(hybm_copy_params &params, hybm_data_copy_direction direction,
                         const ExtOptions &options) noexcept override;
    Result BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                         const ExtOptions &options) noexcept override;
    Result Wait(int32_t waitId) noexcept override;

private:
    Result CopyLH2GH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    Result CopyLH2GD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    Result CopyLD2GH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    Result CopyLD2GD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    Result CopyGH2LH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    Result CopyGD2LH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    Result CopyGH2LD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    Result CopyGD2LD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    Result CopyGH2GH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    Result CopyGD2GH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    Result CopyGH2GD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    Result CopyGD2GD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    Result CopyLH2LH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    Result CopyLD2LD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    Result CopyLH2LD(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    Result CopyLD2LH(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;
    Result CopyRDMA(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options) noexcept;

    Result BatchCopyLH2GD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    Result BatchCopyGD2LH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    Result BatchCopyLD2GD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    Result BatchCopyLD2GH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    Result BatchCopyGH2LD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    Result BatchCopyGD2LD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    Result BatchCopyLH2GH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    Result BatchCopyGH2GH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    Result BatchCopyGH2GD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    Result BatchCopyGH2LH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    Result BatchCopyGD2GH(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;
    Result BatchCopyGD2GD(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;

    Result BatchDataCopyDefault(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                const ExtOptions &options) noexcept;
    Result BatchDataCopyLocal(hybm_batch_copy_params &params, int32_t direction, const ExtOptions &options) noexcept;
    Result BatchDataCopyLocalSync(hybm_batch_copy_params &params, int32_t direction,
                                  const ExtOptions &options) noexcept;
    Result BatchDataCopyLocalAsync(hybm_batch_copy_params &params, int32_t direction,
                                   const ExtOptions &options) noexcept;
    Result BatchDataCopyLocalBatch(hybm_batch_copy_params &params, int32_t direction,
                                   const ExtOptions &options) noexcept;

    Result AllocSwapMemory();
    void FreeSwapMemory();

    void ClassifyDataAddr(void **globalAddrs, void **localAddrs, const uint64_t *counts, uint32_t batchSize,
                          std::unordered_map<uint32_t, CopyDescriptor> &registered,
                          std::unordered_map<uint32_t, CopyDescriptor> &localed,
                          std::unordered_map<uint32_t, CopyDescriptor> &notRegistered, uint32_t globalRankId) noexcept;
    Result BatchCopyWrite(hybm_batch_copy_params &params, const ExtOptions &options,
                          hybm_data_copy_direction direction) noexcept;
    Result BatchCopyRead(hybm_batch_copy_params &params, const ExtOptions &options,
                         hybm_data_copy_direction direction) noexcept;
    Result BatchCopyG2G(hybm_batch_copy_params &params, const ExtOptions &options,
                        hybm_data_copy_direction direction) noexcept;
    Result SafePut(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options, bool isLocalHost);
    Result SafeGet(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options, bool isLocalHost);

private:
    bool inited_{false};
    uint32_t rankId_{0};
    std::shared_ptr<transport::TransportManager> transportManager_;
    void *rdmaSwapBaseAddr_{nullptr};
    std::shared_ptr<RbtreeRangePool> rdmaSwapMemoryAllocator_;
};
} // namespace mf
} // namespace ock

#endif // MF_HYBRID_HYBM_DATA_OP_DEVICE_RDMA_H
