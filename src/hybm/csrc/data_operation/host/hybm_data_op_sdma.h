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
#ifndef MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H
#define MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H

#include <unordered_map>
#include "mf_rwlock.h"
#include "hybm_data_operator.h"
#include "hybm_stream.h"
#include "hybm_rbtree_range_pool.h"

namespace ock {
namespace mf {
class HostDataOpSDMA : public DataOperator {
public:
    explicit HostDataOpSDMA() noexcept;
    ~HostDataOpSDMA() override;

    Result Initialize() noexcept override;
    void UnInitialize() noexcept override;

    Result DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                     const ExtOptions &options) noexcept override;
    Result DataCopyAsync(hybm_copy_params &params, hybm_data_copy_direction direction,
                          const ExtOptions &options) noexcept override;
    Result BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                          const ExtOptions &options) noexcept override;
    Result Wait(int32_t waitId) noexcept override;

    void CleanUp() noexcept override;

private:
    Result CopyLH2GD(void* gvaAddr, const void* hostAddr, size_t count, void* stream) noexcept;
    Result CopyGD2LH(void* hostAddr, const void* gvaAddr, size_t count, void* stream) noexcept;
    Result CopyLH2GH(void* destVA, const void* srcVA, uint64_t length, void* stream) noexcept;
    Result CopyGH2LH(void* destVA, const void* srcVA, uint64_t length, void* stream) noexcept;

    void InitG2GStreamTask(StreamTask &task) noexcept;
    Result CopyG2G(void *destVA, const void *srcVA, size_t count, void *stream) noexcept;
    Result BatchCopyG2G(hybm_batch_copy_params &params, const ExtOptions &options) noexcept;

    Result CopyG2GAsync(void *destVA, const void *srcVA, size_t count, void *stream) noexcept;

    Result BatchCopyLH2GD(void *gvaAddrs[], void *hostAddrs[], const uint64_t counts[],
                       uint32_t batchSize, void *stream) noexcept;
    Result BatchCopyGD2LH(void *hostAddrs[], void *gvaAddrs[], const uint64_t counts[],
                       uint32_t batchSize, void *stream) noexcept;
    Result BatchCopyLH2GH(void *gvaAddrs[], void *hostAddrs[], const uint64_t counts[],
                       uint32_t batchSize, void *stream) noexcept;
    Result BatchCopyGH2LH(void *hostAddrs[], void *gvaAddrs[], const uint64_t counts[],
                       uint32_t batchSize, void *stream) noexcept;

private:
    bool inited_ = false;
};
}
}

#endif  // MEM_FABRIC_HYBRID_HYBM_DATA_OPERATOR_SDMA_H