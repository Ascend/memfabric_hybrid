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

#include "hybm_data_op_host_shm.h"

#include "dl_hybrid_api.h"

namespace ock {
namespace mf {

HostDataOpHostShm::HostDataOpHostShm(uint32_t rankId) noexcept : rankId_(rankId) {}

HostDataOpHostShm::~HostDataOpHostShm()
{
    UnInitialize();
}

Result HostDataOpHostShm::Initialize() noexcept
{
    if (inited_) {
        return BM_OK;
    }
    inited_ = true;
    return BM_OK;
}

void HostDataOpHostShm::UnInitialize() noexcept
{
    inited_ = false;
}

bool HostDataOpHostShm::IsSupportedDirection(hybm_data_copy_direction direction) const noexcept
{
    switch (direction) {
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST:
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST:
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST:
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE:
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST:
            return true;
        default:
            return false;
    }
}

Result HostDataOpHostShm::CopyHostToHost(void *destVA, const void *srcVA, uint64_t length) noexcept
{
    return DlHybridApi::Memcpy(destVA, length, srcVA, length, ACL_MEMCPY_HOST_TO_HOST);
}

Result HostDataOpHostShm::CopyDeviceToHost(void *destVA, const void *srcVA, uint64_t length) noexcept
{
    return DlHybridApi::Memcpy(destVA, length, srcVA, length, ACL_MEMCPY_DEVICE_TO_HOST);
}

Result HostDataOpHostShm::CopyHostToDevice(void *destVA, const void *srcVA, uint64_t length) noexcept
{
    return DlHybridApi::Memcpy(destVA, length, srcVA, length, ACL_MEMCPY_HOST_TO_DEVICE);
}

Result HostDataOpHostShm::DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                                   const ExtOptions &options) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(inited_, "inited_ = " << inited_, BM_NOT_INITIALIZED);
    (void)options;
    if (!IsSupportedDirection(direction)) {
        BM_LOG_ERROR("data copy invalid direction for host shm: " << direction);
        return BM_INVALID_PARAM;
    }

    Result ret;
    switch (direction) {
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST:
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST:
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST:
            ret = CopyHostToHost(params.dest, params.src, params.dataSize);
            break;
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST:
            ret = CopyDeviceToHost(params.dest, params.src, params.dataSize);
            break;
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE:
            ret = CopyHostToDevice(params.dest, params.src, params.dataSize);
            break;
        default:
            BM_LOG_ERROR("data copy invalid direction for host shm: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

Result HostDataOpHostShm::DataCopyAsync(hybm_copy_params &params, hybm_data_copy_direction direction,
                                        const ExtOptions &options) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(inited_, "inited_ = " << inited_, BM_NOT_INITIALIZED);
    (void)params;
    (void)direction;
    (void)options;
    BM_LOG_ERROR("host shm does not support async copy");
    return BM_ERROR;
}

Result HostDataOpHostShm::BatchCopyHostToHost(void **destAddrs, void **srcAddrs, const uint64_t *counts,
                                              uint32_t batchSize) noexcept
{
    for (uint32_t i = 0; i < batchSize; ++i) {
        auto ret = CopyHostToHost(destAddrs[i], srcAddrs[i], counts[i]);
        if (ret != BM_OK) {
            return ret;
        }
    }
    return BM_OK;
}

Result HostDataOpHostShm::BatchCopyDeviceToHost(void **destAddrs, void **srcAddrs, const uint64_t *counts,
                                                uint32_t batchSize) noexcept
{
    for (uint32_t i = 0; i < batchSize; ++i) {
        auto ret = CopyDeviceToHost(destAddrs[i], srcAddrs[i], counts[i]);
        if (ret != BM_OK) {
            return ret;
        }
    }
    return BM_OK;
}

Result HostDataOpHostShm::BatchCopyHostToDevice(void **destAddrs, void **srcAddrs, const uint64_t *counts,
                                                uint32_t batchSize) noexcept
{
    for (uint32_t i = 0; i < batchSize; ++i) {
        auto ret = CopyHostToDevice(destAddrs[i], srcAddrs[i], counts[i]);
        if (ret != BM_OK) {
            return ret;
        }
    }
    return BM_OK;
}

Result HostDataOpHostShm::BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                        const ExtOptions &options) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(inited_, "inited_ = " << inited_, BM_NOT_INITIALIZED);
    (void)options;
    if (!IsSupportedDirection(direction)) {
        BM_LOG_ERROR("batch data copy invalid direction for host shm: " << direction);
        return BM_INVALID_PARAM;
    }
    Result ret;
    switch (direction) {
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST:
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST:
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST:
            ret = BatchCopyHostToHost(params.destinations, params.sources, params.dataSizes, params.batchSize);
            break;
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST:
            ret = BatchCopyDeviceToHost(params.destinations, params.sources, params.dataSizes, params.batchSize);
            break;
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE:
            ret = BatchCopyHostToDevice(params.destinations, params.sources, params.dataSizes, params.batchSize);
            break;
        default:
            BM_LOG_ERROR("batch data copy invalid direction for host shm: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

Result HostDataOpHostShm::Wait(int32_t waitId) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(inited_, "inited_ = " << inited_, BM_NOT_INITIALIZED);
    (void)waitId;
    return BM_OK;
}

} // namespace mf
} // namespace ock
