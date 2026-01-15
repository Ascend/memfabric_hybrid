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
        : rankId_(rankId), transportManager_(transportManager) {};

    ~HostDataOpRDMA() override;

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
    Result CopyHost2Gva(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options);
    Result CopyGva2Host(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options);
    Result CopyDevice2Gva(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options);
    Result CopyGva2Device(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options);
    Result CopyGva2Gva(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options);
    Result SafePut(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options, bool isLocalHost);
    Result SafeGet(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options, bool isLocalHost);

    Result BatchCopyLH2LH(void *gvaAddrs[], void *hostAddrs[], const uint64_t counts[], uint32_t batchSize) noexcept;
    Result BatchCopyLD2LH(void *hostAddrs[], void *deviceAddrs[], const uint64_t counts[], uint32_t batchSize,
                          const ExtOptions &options) noexcept;
    Result BatchCopyLH2LD(void *deviceAddrs[], void *hostAddrs[], const uint64_t counts[], uint32_t batchSize,
                          const ExtOptions &options) noexcept;
    Result BatchCopyLD2GH(void *gvaAddrs[], void *deviceAddrs[], const uint64_t counts[], uint32_t batchSize,
                          const ExtOptions &options) noexcept;
    Result BatchCopyGH2LD(void *deviceAddrs[], void *gvaAddrs[], const uint64_t counts[], uint32_t batchSize,
                          const ExtOptions &options) noexcept;
    Result BatchCopyLH2GH(void *gvaAddrs[], void *hostAddrs[], const uint64_t counts[], uint32_t batchSize,
                          const ExtOptions &options) noexcept;
    Result BatchCopyGH2LH(void *hostAddrs[], void *gvaAddrs[], const uint64_t counts[], uint32_t batchSize,
                          const ExtOptions &options) noexcept;
    Result BatchCopyGH2GH(void *destAddrs[], void *srcAddrs[], const uint64_t counts[], uint32_t batchSize,
                          const ExtOptions &options) noexcept;

    void ClassifyDataAddr(void **globalAddrs, void **localAddrs, const uint64_t *counts, uint32_t batchSize,
                          std::unordered_map<uint32_t, CopyDescriptor> &rmtRankMap,
                          std::unordered_map<uint32_t, CopyDescriptor> &localRankMap) noexcept;
    Result BatchWriteLD2RH(uint32_t rmtRankId, CopyDescriptor &rmtCopyDescriptor, const ExtOptions &options) noexcept;
    Result BatchReadRH2LD(uint32_t rmtRankId, CopyDescriptor &rmtCopyDescriptor, const ExtOptions &options) noexcept;

    Result BatchReadRH2LH(CopyDescriptor &rmtCopyDescriptor, const ExtOptions &options) noexcept;

private:
    Result InnerBatchReadRH2LH(const CopyDescriptor &rmtCopyDescriptor, const ExtOptions &options, uint64_t batchOffset,
                               size_t batchEnd, void *tmpRdmaAddrs[]) const;
    bool inited_{false};
    uint32_t rankId_{0};
    void *rdmaSwapBaseAddr_{nullptr};
    std::shared_ptr<transport::TransportManager> transportManager_;
    std::shared_ptr<RbtreeRangePool> rdmaSwapMemoryAllocator_;
};
} // namespace mf
} // namespace ock
#endif // MF_HYBRID_HYBM_DATA_OP_HOST_RDMA_H