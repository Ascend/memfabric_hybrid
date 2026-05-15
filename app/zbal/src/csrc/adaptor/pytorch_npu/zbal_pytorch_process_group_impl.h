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
#ifndef ZBAL_PROCESS_GROUP_IMPL_H
#define ZBAL_PROCESS_GROUP_IMPL_H

#include <torch_npu/csrc/distributed/ProcessGroupHCCL.hpp>
#include "zbal_pytorch_process_group.h"

namespace zbal {
namespace adaptor {
namespace pytorch_npu {

class ProcessGroupZBALImpl : public c10d::Backend {
public:
    ProcessGroupZBALImpl(const c10::intrusive_ptr<c10d::Store> &store, int rank, int size,
                         c10::intrusive_ptr<ProcessGroupZBAL::Options> options = ProcessGroupZBAL::Options::create());

    ~ProcessGroupZBALImpl() = default;

public:
    c10::intrusive_ptr<c10d::Work> allreduce(std::vector<at::Tensor> &tensors,
                                             const c10d::AllreduceOptions &opts = c10d::AllreduceOptions()) override;

    c10::intrusive_ptr<c10d::Work>
    _allgather_base(at::Tensor &output, at::Tensor &input,
                    const c10d::AllgatherOptions &opt = c10d::AllgatherOptions()) override;

    c10::intrusive_ptr<c10d::Work> allgather(std::vector<std::vector<at::Tensor>> &outputTensors,
                                             std::vector<at::Tensor> &inputTensors,
                                             const c10d::AllgatherOptions &opts = c10d::AllgatherOptions()) override;

    c10::intrusive_ptr<c10d::Work> broadcast(std::vector<at::Tensor> &tensors,
                                             const c10d::BroadcastOptions &opts = c10d::BroadcastOptions()) override;

    c10::intrusive_ptr<c10d::Work> scatter(std::vector<at::Tensor> &outputTensors,
                                           std::vector<std::vector<at::Tensor>> &inputTensors,
                                           const c10d::ScatterOptions &opts = c10d::ScatterOptions()) override;

    c10::intrusive_ptr<c10d::Work> reduce_scatter(std::vector<at::Tensor> &outputTensors,
                                                  std::vector<std::vector<at::Tensor>> &inputTensors,
                                                  const RSOptions &opts = RSOptions()) override;

    c10::intrusive_ptr<c10d::Work> _reduce_scatter_base(at::Tensor &output, at::Tensor &input,
                                                        const RSOptions &opts = RSOptions()) override;

    c10::intrusive_ptr<c10d::Work> barrier(const c10d::BarrierOptions &opts = c10d::BarrierOptions()) override;

    c10::intrusive_ptr<c10d::Work> alltoall_base(at::Tensor &outputTensor, at::Tensor &inputTensor,
                                                 std::vector<int64_t> &outputSplits, std::vector<int64_t> &inputSplits,
                                                 const c10d::AllToAllOptions &opts = c10d::AllToAllOptions()) override;

    c10::intrusive_ptr<c10d::Work> send(std::vector<at::Tensor> &tensors, int dstRank, int tag) override;

    c10::intrusive_ptr<c10d::Work> recv(std::vector<at::Tensor> &tensors, int srcRank, int tag) override;

public:
    std::string getZBALCommName() noexcept;

    int32_t initCommunicator() noexcept;

private:
    c10::intrusive_ptr<ProcessGroupZBAL> zbalGroup_;
    c10::intrusive_ptr<c10d_npu::ProcessGroupHCCL> hcclGroup_;
    static std::set<std::string> hcclOp_;
};

} // namespace pytorch_npu
} // namespace adaptor
} // namespace zbal

#endif