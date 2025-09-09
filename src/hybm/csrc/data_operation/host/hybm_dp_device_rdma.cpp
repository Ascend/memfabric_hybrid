/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include "hybm_dp_device_rdma.h"

namespace ock {
namespace mf {
DataOpDeviceRDMA::DataOpDeviceRDMA(uint32_t rankId, std::shared_ptr<transport::TransportManager> tm) noexcept
    : rankId_{rankId},
      transportManager_{std::move(tm)}
{
}

int32_t DataOpDeviceRDMA::Initialize() noexcept
{
    return BM_OK;
}

void DataOpDeviceRDMA::UnInitialize() noexcept {}

int32_t DataOpDeviceRDMA::DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                                   const ock::mf::ExtOptions &options) noexcept
{
    BM_LOG_INFO("DataCopy local rank:" << rankId_ << ", srcId: " << options.srcRankId << ", dstId: " << options.destRankId);
    auto src = (uint64_t)(ptrdiff_t)params.src;
    auto dest = (uint64_t)(ptrdiff_t)params.dest;
    int ret;
    if (options.srcRankId == rankId_) {
        ret = transportManager_->WriteRemote(options.destRankId, src, dest, params.dataSize);
    } else if (options.destRankId == rankId_) {
        ret = transportManager_->ReadRemote(options.srcRankId, src, dest, params.dataSize);
    } else {
        BM_LOG_ERROR("local rank:" << rankId_ << ", srcId: " << options.srcRankId << ", dstId: " << options.destRankId);
        return BM_INVALID_PARAM;
    }

    if (ret != BM_OK) {
        BM_LOG_ERROR("transport copy data failed: " << ret);
    }

    return ret;
}

int32_t DataOpDeviceRDMA::DataCopy2d(hybm_copy_2d_params &params, hybm_data_copy_direction direction,
                                     const ock::mf::ExtOptions &options) noexcept
{
    BM_LOG_ERROR("DataOpDeviceRDMA::DataCopy2d Not Supported!");
    return BM_ERROR;
}

int32_t DataOpDeviceRDMA::DataCopyAsync(hybm_copy_params &params, hybm_data_copy_direction direction,
                                        const ExtOptions &options) noexcept
{
    BM_LOG_ERROR("DataOpDeviceRDMA::DataCopyAsync Not Supported!");
    return BM_ERROR;
}

int32_t DataOpDeviceRDMA::Wait(int32_t waitId) noexcept
{
    BM_LOG_ERROR("DataOpDeviceRDMA::Wait Not Supported!");
    return BM_ERROR;
}
}
}