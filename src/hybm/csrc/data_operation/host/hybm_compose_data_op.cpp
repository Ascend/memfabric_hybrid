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
#include "hybm_logger.h"
#include "hybm_data_op_factory.h"
#include "hybm_compose_data_op.h"

namespace ock {
namespace mf {
HostComposeDataOp::HostComposeDataOp(hybm_options options, transport::TransManagerPtr tm,
                                     HybmEntityTagInfoPtr tag) noexcept
    : options_{std::move(options)}, transport_{std::move(tm)}, entityTagInfo_{std::move(tag)}
{}

HostComposeDataOp::~HostComposeDataOp() noexcept {}

Result HostComposeDataOp::Initialize() noexcept
{
    // AI_CORE驱动不走这里的dataOperator
    if (options_.bmType == HYBM_TYPE_AI_CORE_INITIATE) {
        return BM_OK;
    }

    if (options_.bmDataOpType & HYBM_DOP_TYPE_SDMA) {
        sdmaDataOperator_ = DataOperatorFactory::CreateSdmaDataOperator();
        auto ret = sdmaDataOperator_->Initialize();
        if (ret != BM_OK) {
            BM_LOG_ERROR("SDMA data operator init failed, ret:" << ret);
            sdmaDataOperator_ = nullptr;
            return ret;
        }
    }

    if (options_.bmDataOpType & HYBM_DOP_TYPE_DEVICE_RDMA) {
        devRdmaDataOperator_ = DataOperatorFactory::CreateDevRdmaDataOperator(options_.rankId, transport_);
        auto ret = devRdmaDataOperator_->Initialize();
        if (ret != BM_OK) {
            BM_LOG_ERROR("Device RDMA data operator init failed, ret:" << ret);
            sdmaDataOperator_ = nullptr;
            devRdmaDataOperator_ = nullptr;
            return ret;
        }
    }

    if (options_.bmDataOpType & (HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_HOST_URMA | HYBM_DOP_TYPE_HOST_TCP)) {
        hostRdmaDataOperator_ = DataOperatorFactory::CreateHostRdmaDataOperator(options_.rankId, transport_);
        auto ret = hostRdmaDataOperator_->Initialize();
        if (ret != BM_OK) {
            BM_LOG_ERROR("Host RDMA data operator init failed, ret:" << ret);
            sdmaDataOperator_ = nullptr;
            devRdmaDataOperator_ = nullptr;
            hostRdmaDataOperator_ = nullptr;
            return ret;
        }
    }

    return BM_OK;
}

void HostComposeDataOp::UnInitialize() noexcept
{
    if (hostRdmaDataOperator_ != nullptr) {
        hostRdmaDataOperator_->UnInitialize();
        hostRdmaDataOperator_ = nullptr;
    }
    if (devRdmaDataOperator_ != nullptr) {
        devRdmaDataOperator_->UnInitialize();
        devRdmaDataOperator_ = nullptr;
    }
    if (sdmaDataOperator_ != nullptr) {
        sdmaDataOperator_->UnInitialize();
        sdmaDataOperator_ = nullptr;
    }
}

Result HostComposeDataOp::DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                                   const ExtOptions &options) noexcept
{
    if (options_.scene == HYBM_SCENE_TRANS) {
        if (sdmaDataOperator_ != nullptr && (options_.bmDataOpType & HYBM_DOP_TYPE_SDMA)) {
            return sdmaDataOperator_->DataCopy(params, direction, options);
        }

        if (devRdmaDataOperator_ != nullptr && (options_.bmDataOpType & HYBM_DOP_TYPE_DEVICE_RDMA)) {
            return devRdmaDataOperator_->DataCopy(params, direction, options);
        }

        BM_LOG_ERROR("only support sdma or dev_rdma in trans currently");
        return BM_INVALID_PARAM;
    }

    auto availableOps = GetPrioritedDataOperators(options);
    if (availableOps.empty()) {
        BM_LOG_ERROR("data copy from rank " << options.srcRankId << " to rank " << options.destRankId
                                            << " no data operator available");
        return BM_INVALID_PARAM;
    }

    Result result = BM_ERROR;
    for (auto &ops : availableOps) {
        BM_LOG_DEBUG("try data copy from rank " << options.srcRankId << " to rank " << options.destRankId
                                                << " with data op " << ops.first);
        result = ops.second->DataCopy(params, direction, options);
        if (result == BM_OK) {
            break;
        }

        BM_LOG_ERROR("data copy from rank " << options.srcRankId << " to rank " << options.destRankId
                                            << " with data op " << ops.first << " failed " << result);
    }
    return result;
}

Result HostComposeDataOp::BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                        const ExtOptions &options) noexcept
{
    if (options_.scene == HYBM_SCENE_TRANS) {
        if (sdmaDataOperator_ != nullptr && (options_.bmDataOpType & HYBM_DOP_TYPE_SDMA)) {
            return sdmaDataOperator_->BatchDataCopy(params, direction, options);
        }

        if (devRdmaDataOperator_ != nullptr && (options_.bmDataOpType & HYBM_DOP_TYPE_DEVICE_RDMA)) {
            return devRdmaDataOperator_->BatchDataCopy(params, direction, options);
        }

        BM_LOG_ERROR("only support sdma or dev_rdma in trans currently");
        return BM_INVALID_PARAM;
    }

    auto availableOps = GetPrioritedDataOperators(options);
    if (availableOps.empty()) {
        BM_LOG_ERROR("batch data copy from rank " << options.srcRankId << " to rank " << options.destRankId
                                                  << " no data operator available");
        return BM_INVALID_PARAM;
    }

    Result result = BM_ERROR;
    for (auto &ops : availableOps) {
        BM_LOG_DEBUG("try batch data copy from rank " << options.srcRankId << " to rank " << options.destRankId
                                                      << " with data op " << ops.first);
        result = ops.second->BatchDataCopy(params, direction, options);
        if (result == BM_OK) {
            break;
        }

        BM_LOG_ERROR("data batch copy from rank " << options.srcRankId << " to rank " << options.destRankId
                                                  << " with data op " << ops.first << " failed " << result);
    }
    return result;
}

Result HostComposeDataOp::DataCopyAsync(hybm_copy_params &params, hybm_data_copy_direction direction,
                                        const ExtOptions &options) noexcept
{
    auto availableOps = GetPrioritedDataOperators(options);
    if (availableOps.empty()) {
        BM_LOG_ERROR("data copy async from rank " << options.srcRankId << " to rank " << options.destRankId
                                                  << " no data operator available");
        return BM_INVALID_PARAM;
    }

    Result result = BM_ERROR;
    for (auto &ops : availableOps) {
        BM_LOG_DEBUG("try data copy async from rank " << options.srcRankId << " to rank " << options.destRankId
                                                      << " with data op " << ops.first);
        result = ops.second->DataCopyAsync(params, direction, options);
        if (result == BM_OK) {
            break;
        }

        BM_LOG_ERROR("data copy async from rank " << options.srcRankId << " to rank " << options.destRankId
                                                  << " with data op " << ops.first << " failed " << result);
    }
    return result;
}

Result HostComposeDataOp::Wait(int32_t waitId) noexcept
{
    /*
     * Note: Currently, only SDMA supports asynchronous operations; we only perform the wait for the SDMA Data Operator.
     * Subsequent consideration involves using the 3 bits in the wait ID to indicate which data operator is being used.
     */
    if (sdmaDataOperator_ == nullptr) {
        BM_LOG_ERROR("SDMA data operator not exist.");
        return BM_ERROR;
    }

    return sdmaDataOperator_->Wait(waitId);
}

HostComposeDataOp::DataOperators HostComposeDataOp::GetPrioritedDataOperators(const ExtOptions &options) noexcept
{
    HostComposeDataOp::DataOperators dataOperators;
    auto opTypes = entityTagInfo_->GetRank2RankOpType(options.srcRankId, options.destRankId);
    if (sdmaDataOperator_ != nullptr && (opTypes & static_cast<uint32_t>(HYBM_DOP_TYPE_SDMA)) != 0U) {
        dataOperators.emplace_back("SDMA", sdmaDataOperator_);
    }

    if (devRdmaDataOperator_ != nullptr && (opTypes & static_cast<uint32_t>(HYBM_DOP_TYPE_DEVICE_RDMA)) != 0U) {
        dataOperators.emplace_back("DEV_RDMA", devRdmaDataOperator_);
    }

    if (hostRdmaDataOperator_ != nullptr && (opTypes & static_cast<uint32_t>(HYBM_DOP_TYPE_HOST_RDMA)) != 0U) {
        dataOperators.emplace_back("HOST_RDMA", hostRdmaDataOperator_);
    }

    if (hostRdmaDataOperator_ != nullptr && (opTypes & static_cast<uint32_t>(HYBM_DOP_TYPE_HOST_URMA)) != 0U) {
        dataOperators.emplace_back("HOST_URMA", hostRdmaDataOperator_);
    }

    return dataOperators;
}
} // namespace mf
} // namespace ock