/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "zbal_pytorch_process_group_impl.h"
#include "zbal_functions.h"
#include "zbal_env_helper.h"

namespace zbal {
namespace adaptor {
namespace pytorch_npu {

std::set<std::string> ProcessGroupZBALImpl::hcclOp_ = Func::GetEnvSplitByComma(ENV_NAME_HCCL_OP);

ProcessGroupZBALImpl::ProcessGroupZBALImpl(const c10::intrusive_ptr<c10d::Store>& store,
                                           int rank, int size, c10::intrusive_ptr<ProcessGroupZBAL::Options> options)
    : c10d::Backend(rank, size)
{
    zbalGroup_ = c10::make_intrusive<ProcessGroupZBAL>(store, rank, size, options);

    if (!hcclOp_.empty()) {
        auto hcclOption = c10::make_intrusive<c10d_npu::ProcessGroupHCCL::Options>();
        hcclOption->is_high_priority_stream = options->isHighPriorityStream;
        hcclOption->timeout = options->opTimeout;
        hcclOption->global_ranks_in_group = options->globalRanksInGroup;
        hcclOption->group_id = options->groupId;
        hcclGroup_ = c10::make_intrusive<c10d_npu::ProcessGroupHCCL>(store, rank, size, hcclOption);
    }
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBALImpl::allreduce(std::vector<at::Tensor> &tensors,
                                                               const c10d::AllreduceOptions &opts)
{
    if (ZBAL_UNLIKELY(hcclOp_.find(__func__) != hcclOp_.end())) {
        return hcclGroup_->allreduce(tensors, opts);
    } else {
        return zbalGroup_->allreduce(tensors, opts);
    }
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBALImpl::_allgather_base(at::Tensor &outputTensor,
                                                                     at::Tensor &inputTensor,
                                                                     const c10d::AllgatherOptions &opts)
{
    if (ZBAL_UNLIKELY(hcclOp_.find("allgather") != hcclOp_.end())) {
        return hcclGroup_->_allgather_base(outputTensor, inputTensor, opts);
    } else {
        return zbalGroup_->_allgather_base(outputTensor, inputTensor, opts);
    }
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBALImpl::_reduce_scatter_base(at::Tensor &outputTensor,
                                                                          at::Tensor &inputTensor,
                                                                          const c10d::ReduceScatterOptions &opts)
{
    if (ZBAL_UNLIKELY(hcclOp_.find("reduce_scatter") != hcclOp_.end())) {
        return hcclGroup_->_reduce_scatter_base(outputTensor, inputTensor, opts);
    } else {
        return zbalGroup_->_reduce_scatter_base(outputTensor, inputTensor, opts);
    }
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBALImpl::allgather(std::vector<std::vector<at::Tensor>> &outputTensors,
                                                               std::vector<at::Tensor> &inputTensors,
                                                               const c10d::AllgatherOptions &opts)
{
    if (ZBAL_UNLIKELY(hcclOp_.find(__func__) != hcclOp_.end())) {
        return hcclGroup_->allgather(outputTensors, inputTensors, opts);
    } else {
        return zbalGroup_->allgather(outputTensors, inputTensors, opts);
    }
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBALImpl::broadcast(std::vector<at::Tensor> &tensors,
                                                               const c10d::BroadcastOptions &opts)
{
    if (ZBAL_UNLIKELY(hcclOp_.find(__func__) != hcclOp_.end())) {
        return hcclGroup_->broadcast(tensors, opts);
    } else {
        return zbalGroup_->broadcast(tensors, opts);
    }
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBALImpl::scatter(std::vector<at::Tensor>& outputTensors,
                                                             std::vector<std::vector<at::Tensor>>& inputTensors,
                                                             const c10d::ScatterOptions &opts)
{
    if (ZBAL_UNLIKELY(hcclOp_.find(__func__) != hcclOp_.end())) {
        return hcclGroup_->scatter(outputTensors, inputTensors, opts);
    } else {
        return zbalGroup_->scatter(outputTensors, inputTensors, opts);
    }
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBALImpl::reduce_scatter(std::vector<at::Tensor> &outputTensors,
                                                                    std::vector<std::vector<at::Tensor>> &inputTensors,
                                                                    const c10d::ReduceScatterOptions &opts)
{
    if (ZBAL_UNLIKELY(hcclOp_.find(__func__) != hcclOp_.end())) {
        return hcclGroup_->reduce_scatter(outputTensors, inputTensors, opts);
    } else {
        return zbalGroup_->reduce_scatter(outputTensors, inputTensors, opts);
    }
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBALImpl::barrier(const c10d::BarrierOptions& opts)
{
    if (ZBAL_UNLIKELY(hcclOp_.find(__func__) != hcclOp_.end())) {
        return hcclGroup_->barrier(opts);
    } else {
        return zbalGroup_->barrier(opts);
    }
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBALImpl::alltoall_base(at::Tensor &outputTensor,
                                                                   at::Tensor &inputTensor,
                                                                   std::vector<int64_t> &outputSplits,
                                                                   std::vector<int64_t> &inputSplits,
                                                                   const c10d::AllToAllOptions &opts)
{
    if (ZBAL_UNLIKELY(hcclOp_.find("alltoall") != hcclOp_.end())) {
        return hcclGroup_->alltoall_base(outputTensor, inputTensor, outputSplits, inputSplits, opts);
    } else {
        return zbalGroup_->alltoall_base(outputTensor, inputTensor, outputSplits, inputSplits, opts);
    }
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBALImpl::send(std::vector<at::Tensor>& tensors, int dstRank, int tag)
{
    if (ZBAL_UNLIKELY(hcclOp_.find(__func__) != hcclOp_.end())) {
        return hcclGroup_->send(tensors, dstRank, tag);
    } else {
        return zbalGroup_->send(tensors, dstRank, tag);
    }
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBALImpl::recv(std::vector<at::Tensor>& tensors, int srcRank, int tag)
{
    if (ZBAL_UNLIKELY(hcclOp_.find(__func__) != hcclOp_.end())) {
        return hcclGroup_->recv(tensors, srcRank, tag);
    } else {
        return zbalGroup_->recv(tensors, srcRank, tag);
    }
}

std::string ProcessGroupZBALImpl::getZBALCommName() noexcept
{
    return zbalGroup_->getZBALCommName();
}

int32_t ProcessGroupZBALImpl::initCommunicator() noexcept
{
    return zbalGroup_->initCommunicator();
}

}  // namespace pytorch_npu
}  // namespace adaptor
}  // namespace zbal