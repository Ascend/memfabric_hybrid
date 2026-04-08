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
#ifndef MF_HYBRID_HYBM_DATA_OP_HOST_SHM_H
#define MF_HYBRID_HYBM_DATA_OP_HOST_SHM_H

#include "hybm_data_operator.h"

namespace ock {
namespace mf {

class HostDataOpHostShm : public DataOperator {
public:
    explicit HostDataOpHostShm(uint32_t rankId) noexcept;
    ~HostDataOpHostShm() override;

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
    Result CopyHostToHost(void *destVA, const void *srcVA, uint64_t length) noexcept;
    Result CopyDeviceToHost(void *destVA, const void *srcVA, uint64_t length) noexcept;
    Result CopyHostToDevice(void *destVA, const void *srcVA, uint64_t length) noexcept;
    Result BatchCopyHostToHost(void **destAddrs, void **srcAddrs, const uint64_t *counts, uint32_t batchSize) noexcept;
    Result BatchCopyDeviceToHost(void **destAddrs, void **srcAddrs, const uint64_t *counts,
                                 uint32_t batchSize) noexcept;
    Result BatchCopyHostToDevice(void **destAddrs, void **srcAddrs, const uint64_t *counts,
                                 uint32_t batchSize) noexcept;
    bool IsSupportedDirection(hybm_data_copy_direction direction) const noexcept;

private:
    bool inited_{false};
    uint32_t rankId_{0};
};

} // namespace mf
} // namespace ock

#endif // MF_HYBRID_HYBM_DATA_OP_HOST_SHM_H
