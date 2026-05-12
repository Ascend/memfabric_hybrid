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
#ifndef ZBAL_PROCESS_GROUP_H
#define ZBAL_PROCESS_GROUP_H

#include <c10/util/intrusive_ptr.h>
#include <c10/util/irange.h>
#include <c10d/Backend.hpp>
#include <c10d/Store.hpp>
#include <c10d/Utils.hpp>
#include <c10d/comm.hpp>
#include <c10d/Work.hpp>

#include <torch/csrc/Exceptions.h>
#include <ATen/core/functional.h>
#include <torch/csrc/jit/python/pybind_utils.h>
#include <torch/csrc/utils/object_ptr.h>
#include <torch/csrc/utils/pybind.h>
#include <torch/csrc/utils/tensor_flatten.h>

#include <torch_npu/csrc/core/npu/NPUStream.h>
#include <torch_npu/csrc/core/npu/NPUEvent.h>

#include "zbal_def.h"

namespace zbal {
namespace adaptor {
namespace pytorch_npu {

const std::string ZBAL_BACKEND_NAME = "zbal";
constexpr std::chrono::milliseconds WORKER_MAX_TIMEOUT{1800000};
using RSOptions = c10d::ReduceScatterOptions;
using MilliSeconds = std::chrono::milliseconds;

class ProcessGroupZBAL : public c10d::Backend {
public:
    class WorkZBAL : public c10d::Work, public std::enable_shared_from_this<WorkZBAL> {
    public:
        // Constructor takes a list of NPU devices to adapt framework, But zbal support one device only!!!
        explicit WorkZBAL(const std::vector<at::Device> &devices, int rank, c10d::OpType opType);

        ~WorkZBAL() override;

        bool isCompleted() override;

        bool isSuccess() const override;

        bool wait(std::chrono::milliseconds timeout) override;

        c10::intrusive_ptr<c10::ivalue::Future> getFuture() override;

        void synchronize() override;

        bool finishedNPUExecution();

        std::vector<at::Tensor> result() override;

    protected:
        std::vector<at::Device> devices_;

        std::vector<zbal_comm_t> zbalComms_;

        std::shared_ptr<std::vector<c10_npu::NPUEvent>> zbalEndEvents_;

        std::vector<at::Tensor> barrierTensors_;

        bool blockingWait_ = false;

        std::chrono::milliseconds opTimeout_;

        std::chrono::time_point<std::chrono::steady_clock> workStartTime_;

    private:
        void synchronizeInternal(std::chrono::milliseconds timeout);

        void checkAndSetException() const;

        void checkAndThrowException() const;

        bool finishedNPUExecutionInternal() const;

        std::shared_ptr<std::vector<at::Tensor>> outputs_;

        c10::intrusive_ptr<c10d::Store> store_;

        c10::intrusive_ptr<at::ivalue::Future> future_;

        std::vector<at::Tensor> lazy_destroy_tensors_;

        friend class ProcessGroupZBAL;
    };

    struct Options : c10d::Backend::Options {
        explicit Options(bool isHighPriorityStream = false);

        static c10::intrusive_ptr<Options> create(bool isHigh = false, MilliSeconds tm = WORKER_MAX_TIMEOUT)
        {
            (void)tm;
            return c10::make_intrusive<Options>(isHigh);
        }

        MilliSeconds opTimeout;

        bool isHighPriorityStream;

        std::vector<uint32_t> globalRanksInGroup;

        std::string groupId;
    };

    ProcessGroupZBAL(const c10::intrusive_ptr<c10d::Store> &store, int rank, int size,
                     c10::intrusive_ptr<Options> options = Options::create());

    ~ProcessGroupZBAL() override;

    const std::string getBackendName() const override
    {
        return ZBAL_BACKEND_NAME;
    }

    c10::intrusive_ptr<c10d::Work> allreduce(std::vector<at::Tensor> &tensors,
                                             const c10d::AllreduceOptions &opts = c10d::AllreduceOptions()) override;

    c10::intrusive_ptr<c10d::Work> _allgather_base(at::Tensor &output, at::Tensor &input,
                                                   const c10d::AllgatherOptions &opt = c10d::AllgatherOptions());

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

    std::string getZBALCommName() noexcept;

    int32_t initCommunicator() noexcept;

protected:
    bool blockingWait_ = false;
    std::chrono::milliseconds opTimeout_;
    c10::intrusive_ptr<c10d::Store> store_;
    std::unordered_map<std::string, std::vector<c10_npu::NPUStream>> zbalStreams_;
    std::unordered_map<std::string, std::vector<c10_npu::NPUEvent>> zbalEvents_;
    std::mutex mutext_;
    c10::intrusive_ptr<Options> options_;

private:
    zbal_comm_t groupComm_{nullptr};
    std::string groupName_;
    int myWorldRank_;
    static std::atomic<uint64_t> groupCounter_;

private:
    template<typename Fn, typename PreProcess, typename PostProcess>
    c10::intrusive_ptr<c10d::Work> collective(std::vector<at::Tensor> &input, std::vector<at::Tensor> &output, Fn fn,
                                              PreProcess pre, PostProcess post, c10d::OpType opType);

    template<typename Fn, typename PreProcess, typename PostProcess>
    c10::intrusive_ptr<c10d::Work> pointToPoint(std::vector<at::Tensor> &tensor, Fn fn, int peer, c10d::OpType opType,
                                                PreProcess pre, PostProcess post);

    c10::intrusive_ptr<c10d::Work> alltoall_normal(at::Tensor &outputTensor, at::Tensor &inputTensor,
                                                   std::vector<int64_t> &outputSplits, std::vector<int64_t> &inSplits,
                                                   const c10d::AllToAllOptions &opts = c10d::AllToAllOptions());

    c10::intrusive_ptr<c10d::Work> alltoall_v(at::Tensor &outputTensor, at::Tensor &inputTensor,
                                              std::vector<int64_t> &outputSplits, std::vector<int64_t> &inputSplits,
                                              const c10d::AllToAllOptions &opts = c10d::AllToAllOptions());

    uint64_t GetNextGroupCounter() noexcept;

    int32_t PrepareCommunicator(int rank, int size) noexcept;

    std::string ConstructCommName() noexcept;

    int32_t PrepareResources(const std::vector<at::Device> &devs) noexcept;
};

} // namespace pytorch_npu
} // namespace adaptor
} // namespace zbal

#endif // ZBAL_PROCESS_GROUP_H