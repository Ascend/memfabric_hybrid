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
#include <torch/custom_class.h>
#include <torch/csrc/python_headers.h>
#include <torch/csrc/THP.h>
#include <torch/python.h>

#include "zbal_trace_viewer_dumper.h"
#include "zbal_pytorch_util.h"
#include "zbal_operations.h"
#include "zbal_common_includes.h"
#include "zbal_comm_host_device_struct.h"
#include "dl_cann_api.h"
#include "zbal_pytorch_process_group.h"

namespace zbal {
namespace adaptor {
namespace pytorch_npu {

using namespace underapi;

constexpr int64_t kSynchronizeBusyWaitMillis = 10;
std::atomic<uint64_t> ProcessGroupZBAL::groupCounter_{0ULL};

ProcessGroupZBAL::WorkZBAL::WorkZBAL(const std::vector<at::Device> &devices, int rank, c10d::OpType opType)
    : Work(rank, opType), devices_(devices), workStartTime_(std::chrono::steady_clock::now())
{
    zbalEndEvents_ = std::make_shared<std::vector<c10_npu::NPUEvent>>(devices.size());
    zbalComms_.resize(devices.size());
}

ProcessGroupZBAL::WorkZBAL::~WorkZBAL() {}

bool ProcessGroupZBAL::WorkZBAL::isCompleted()
{
    checkAndSetException();
    return exception() || finishedNPUExecutionInternal();
}

bool ProcessGroupZBAL::WorkZBAL::isSuccess() const
{
    if (exception()) {
        return false;
    }
    return finishedNPUExecutionInternal();
}

void ProcessGroupZBAL::WorkZBAL::synchronizeInternal(std::chrono::milliseconds timeout)
{
    (void)timeout;
    for (const auto i : c10::irange(devices_.size())) {
        auto currentStream = c10_npu::getCurrentNPUStream(devices_[i].index());
        (*zbalEndEvents_)[i].block(currentStream);
        ZBAL_LOG_INFO("Event: block zbal work is successfully executed, event=" << (*zbalEndEvents_)[i].event());
        if (!barrierTensors_.empty()) {
            c10_npu::NPUGuard npuGuard(devices_[i]);
            c10_npu::npuSynchronizeDevice();
        }
    }

    if (blockingWait_) {
        while (!isCompleted()) {
            auto currentTimepoint = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(currentTimepoint - workStartTime_) > opTimeout_) {
                throw std::runtime_error("Operation has exceeded timeout limit!");
            }
            checkAndThrowException();
            std::this_thread::sleep_for(std::chrono::milliseconds(kSynchronizeBusyWaitMillis));
        }
        checkAndThrowException();
    }
}

bool ProcessGroupZBAL::WorkZBAL::wait(std::chrono::milliseconds timeout)
{
    synchronizeInternal(timeout);
    return true;
}

void ProcessGroupZBAL::WorkZBAL::synchronize()
{
    synchronizeInternal(kNoTimeout);
}

bool ProcessGroupZBAL::WorkZBAL::finishedNPUExecution()
{
    checkAndSetException();
    return finishedNPUExecutionInternal();
}

std::vector<at::Tensor> ProcessGroupZBAL::WorkZBAL::result()
{
    return *outputs_;
}

void ProcessGroupZBAL::WorkZBAL::checkAndThrowException() const
{
    checkAndSetException();
    if (exception()) {
        std::rethrow_exception(exception());
    }
}

void ProcessGroupZBAL::WorkZBAL::checkAndSetException() const
{
    if (exception()) {
        return;
    }
}

bool ProcessGroupZBAL::WorkZBAL::finishedNPUExecutionInternal() const
{
    if (!c10_npu::NpuSysCtrl::GetInstance().GetInitFlag()) {
        return false;
    }
    try {
        for (const auto i : c10::irange(devices_.size())) {
            if (!(*zbalEndEvents_)[i].query()) {
                return false;
            }
        }
    } catch (const std::exception &e) {
        if (std::string(e.what()).find("driver shutting down") == std::string::npos) {
            throw std::runtime_error("finish execution internal failed.");
        }
        ZBAL_LOG_INFO("[Rank " << rank_ << "] Event query failed with exeption: " << e.what());
    }

    return true;
}

c10::intrusive_ptr<c10::ivalue::Future> ProcessGroupZBAL::WorkZBAL::getFuture()
{
    return future_;
}

ProcessGroupZBAL::ProcessGroupZBAL(const c10::intrusive_ptr<c10d::Store> &store, int rank, int size,
                                   c10::intrusive_ptr<Options> options)
    : c10d::Backend(rank, size), store_(store)
{
    if (rank < 0 || size < 1 || rank >= size) {
        ZBAL_LOG_ERROR("invalid input rank=" << rank << ", size=" << size);
        throw std::runtime_error("invalid arguments for process group creation");
    }

    auto timeoutMill = options->opTimeout * 1000;
    if (timeoutMill > WORKER_MAX_TIMEOUT) {
        timeoutMill = WORKER_MAX_TIMEOUT;
        ZBAL_LOG_WARN(static_cast<int>(options->opTimeout.count()) << " exceed, set timeout to default value");
    }
    opTimeout_ = timeoutMill;

    options_ = c10::make_intrusive<Options>(options->isHighPriorityStream);
    options_->opTimeout = options->opTimeout;
    options_->isHighPriorityStream = options->isHighPriorityStream;
    options_->globalRanksInGroup = options->globalRanksInGroup;
    options_->groupId = options->groupId;

    auto ret = PrepareCommunicator(rank, size);
    if (ret != Z_OK) {
        throw std::runtime_error("create zbal process group failed.");
    }
    ZBAL_LOG_DEBUG("create process group success, groupId=" << options_->groupId << ", rank=" << rank_
                                                            << ", size=" << size << ", name=" << groupName_
                                                            << ", isHigh=" << options_->isHighPriorityStream
                                                            << ", timeout=" << static_cast<int>(opTimeout_.count()));
}

uint64_t ProcessGroupZBAL::GetNextGroupCounter() noexcept
{
    ++groupCounter_;
    return groupCounter_.load();
}

int32_t ProcessGroupZBAL::PrepareResources(std::string &groupName, const std::vector<at::Device> &devices) noexcept
{
    if (devices.size() != 1) {
        ZBAL_LOG_ERROR("input devices is invalid, only 1 device is supported.");
        return Z_INVALID_PARAM;
    }

    c10_npu::OptionalNPUGuard npuGuard;
    std::vector<c10_npu::NPUStream> streamVal;
    streamVal.reserve(devices.size());

    for (size_t i = 0; i < devices.size(); ++i) {
        npuGuard.set_index(devices[i].index());
        if (EnvHelper::OP_DEFAULT_STREAM) {
            streamVal.push_back(c10_npu::getCurrentNPUStream());
        } else {
            streamVal.push_back(c10_npu::getNPUStreamFromPool(devices[i].index()));
        }
    }

    std::lock_guard<std::mutex> lock(mutext_);
    zbalStreams_.emplace(groupName, std::move(streamVal));
    zbalEvents_.emplace(std::piecewise_construct, std::make_tuple(groupName), std::make_tuple(devices.size()));
    return Z_OK;
}

std::string ProcessGroupZBAL::ConstructCommName() noexcept
{
    std::vector<uint32_t> &ranks = options_->globalRanksInGroup;
    std::ostringstream oss;

    bool isGlobalGroup = ranks.empty();
    uint32_t start = isGlobalGroup ? 0 : ranks.front();
    if (size_ == 1) {
        oss << ZBAL_BACKEND_NAME << "_" << start << ":" << start << ":" << 1 << "_group_" << GetNextGroupCounter();
        return oss.str();
    }

    uint32_t end = isGlobalGroup ? size_ - 1 : ranks.back();
    uint32_t stride = (end - start) / (size_ - 1);

    for (int i = 0; !isGlobalGroup && i < size_ - 1; ++i) {
        if (ranks[i] + stride != ranks[i + 1]) {
            ZBAL_LOG_WARN("group ranks are not equal difference [" << ranks[i] << ", " << ranks[i + 1] << "]");
            break;
        }
    }

    oss << ZBAL_BACKEND_NAME << "_" << start << ":" << end << ":" << stride << "_group_" + GetNextGroupCounter();
    return oss.str();
}

std::string ProcessGroupZBAL::ConstructP2pCommName(int peer) noexcept
{
    int peerWorldRank = options_->globalRanksInGroup.empty() ? peer : options_->globalRanksInGroup.at(peer);
    int lowRank = myWorldRank_ < peerWorldRank ? myWorldRank_ : peerWorldRank;
    int highRank = myWorldRank_ < peerWorldRank ? peerWorldRank : myWorldRank_;
    std::string groupName = std::to_string(lowRank) + ":" + std::to_string(highRank);
    return groupName;
}

int32_t ProcessGroupZBAL::PrepareCommunicator(int rank, int size) noexcept
{
    groupName_ = ConstructCommName();
    myWorldRank_ = options_->globalRanksInGroup.empty() ? rank : options_->globalRanksInGroup.at(rank);

    if (options_->globalRanksInGroup.empty() && ZBALInitState::Instance().Bootstrapped()) {
        ZBAL_VALIDATE_RETURN(initCommunicator() == Z_OK, "init global zbal comm failed.", Z_ERROR);
        ZBAL_LOG_DEBUG("prepare comm success, rank=" << rank << ", size=" << size << ", key=" << groupName_);
        return Z_OK;
    } else {
        ZBAL_LOG_DEBUG("zbal bootstrap not ready, prepare global comm meta skip.");
        return Z_OK;
    }
}

int32_t ProcessGroupZBAL::initCommunicator() noexcept
{
    std::lock_guard<std::mutex> lock(mutext_);
    if (groupComm_ != nullptr) {
        return Z_OK;
    }

    zbal_comm_options_t opt;
    opt.backendType = ZBAL_ASCEND_NPU;
    opt.isWorldGroup = options_->globalRanksInGroup.empty() ? 1 : 0;
    opt.groupSize = size_;
    opt.name = const_cast<char *>(groupName_.c_str());
    opt.groupRankId = rank_;

    auto ret = zbal_comm_create(&opt, &groupComm_);
    if (ret != Z_OK || groupComm_ == nullptr) {
        ZBAL_LOG_ERROR("create comm failed, ret=" << ret << ", rank=" << myWorldRank_ << ", size=" << size_
                                                  << ", key=" << groupName_);
        return Z_CREATE_COMM_FAILED;
    }
    ZBAL_LOG_DEBUG("init comm success, rank=" << myWorldRank_ << ", size=" << size_ << ", key=" << groupName_);
    return Z_OK;
}

int32_t ProcessGroupZBAL::initP2pCommunicator(int peer, std::string &groupName) noexcept
{
    std::lock_guard<std::mutex> lock(mutext_);
    if (groupP2pComms_.find(groupName) != groupP2pComms_.end()) {
        return Z_OK;
    }

    int peerWorldRank = options_->globalRanksInGroup.empty() ? peer : options_->globalRanksInGroup.at(peer);
    zbal_comm_options_t opt;
    opt.backendType = ZBAL_ASCEND_NPU;
    opt.isWorldGroup = 0;
    opt.groupSize = 2;
    opt.name = const_cast<char *>(groupName.c_str());
    opt.groupRankId = myWorldRank_ < peerWorldRank ? 0 : 1;

    zbal_comm_t groupComm {};
    auto ret = zbal_comm_create(&opt, &groupComm);
    if (ret != Z_OK || groupComm == nullptr) {
        ZBAL_LOG_ERROR("create comm failed, ret=" << ret << ", rank=" << myWorldRank_ << ", size=" << opt.groupSize
                                                  << ", key=" << groupName);
        return Z_CREATE_COMM_FAILED;
    }

    groupP2pComms_.emplace(groupName, groupComm);
    ZBAL_LOG_DEBUG("init comm success, rank=" << myWorldRank_ << ", size=" << opt.groupSize << ", key=" << groupName);
    return Z_OK;
}

template<typename Fn, typename PreProcess, typename PostProcess>
c10::intrusive_ptr<c10d::Work> ProcessGroupZBAL::collective(std::vector<at::Tensor> &inputs,
                                                            std::vector<at::Tensor> &outputs, Fn fn, PreProcess pre,
                                                            PostProcess post, c10d::OpType opType)
{
    const auto devices = GetDeviceList(inputs);
    ZBAL_CHECK_S(initCommunicator() == Z_OK, "init zbal comm failed.");

    std::vector<zbal_comm_t> zbalComms = {groupComm_};
    ZBAL_CHECK_S(PrepareResources(groupName_, devices) == Z_OK, "prepare zbal resource failed.");

    auto &zbalStreams = zbalStreams_[groupName_];
    SyncStreams(devices, zbalEvents_[groupName_], zbalStreams);

    auto work = c10::make_intrusive<ProcessGroupZBAL::WorkZBAL>(devices, rank_, opType);
    work->outputs_ = std::make_shared<std::vector<at::Tensor>>(outputs);

    c10_npu::OptionalNPUGuard npuGuard;
    pre(zbalStreams, work);

    for (const auto i : c10::irange(inputs.size())) {
        npuGuard.set_index(devices[i].index());
        c10_npu::NPUStream &zbalStream = zbalStreams[i];

        c10_npu::NPUCachingAllocator::recordStream(inputs[i].storage().data_ptr(), zbalStream);
    }

    {
        for (const auto i : c10::irange(inputs.size())) {
            npuGuard.set_index(devices[i].index());
            // to avoid to much task pushed to the stream, leading to stream overflow
            // insert sync point fluxLimit(key, i)

            int32_t ret = fn(inputs[i], outputs[i], zbalStreams[i], zbalComms[i]);
            ZBAL_CHECK_S(ret == 0, "zbal process group fn exec failed");
        }
    }

    post(zbalStreams, work);
    {
        c10_npu::NPUMultiStreamGuard guard(zbalStreams);
        work->future_ = c10::make_intrusive<at::ivalue::Future>(c10::ListType::create(c10::TensorType::get()), devices);
        work->future_->markCompleted(at::IValue(*work->outputs_));
    }

    for (size_t i = 0; i < inputs.size(); ++i) {
        c10_npu::NPUStream &zbalStream = zbalStreams[i];
        (*(work->zbalEndEvents_))[i].record(zbalStream);
        ZBAL_LOG_DEBUG(
            "Event: record zbal work is successfully executed, event=" << (*(work->zbalEndEvents_))[i].event());
        work->zbalComms_[i] = zbalComms[i];
    }
    work->blockingWait_ = blockingWait_;
    work->opTimeout_ = opTimeout_;
    return work;
}

template<typename Fn, typename PreProcess, typename PostProcess>
c10::intrusive_ptr<c10d::Work> ProcessGroupZBAL::pointToPoint(std::vector<at::Tensor> &tensors, Fn fn, int peer,
                                                              c10d::OpType opType, PreProcess pre, PostProcess post)
{
    const auto devices = GetDeviceList(tensors);
    auto groupName = ConstructP2pCommName(peer);
    ZBAL_CHECK_S(initP2pCommunicator(peer, groupName) == Z_OK, "init zbal comm failed.");

    std::vector<zbal_comm_t> zbalComms = {groupP2pComms_[groupName]};
    ZBAL_CHECK_S(PrepareResources(groupName, devices) == Z_OK, "prepare zbal resource failed.");

    auto &zbalStreams = zbalStreams_[groupName];
    SyncStreams(devices, zbalEvents_[groupName], zbalStreams);

    auto work = c10::make_intrusive<ProcessGroupZBAL::WorkZBAL>(devices, rank_, opType);
    work->outputs_ = std::make_shared<std::vector<at::Tensor>>(tensors);

    c10_npu::OptionalNPUGuard npuGuard;
    pre(zbalStreams, work);

    for (const auto i : c10::irange(tensors.size())) {
        npuGuard.set_index(devices[i].index());
        c10_npu::NPUStream &zbalStream = zbalStreams[i];
        c10_npu::NPUCachingAllocator::recordStream(tensors[i].storage().data_ptr(), zbalStream);
    }

    {
        int peerWorldRank = options_->globalRanksInGroup.empty() ? peer : options_->globalRanksInGroup.at(peer);
        int peerP2pIdx = myWorldRank_ < peerWorldRank ? 1 : 0;
        for (const auto i : c10::irange(tensors.size())) {
            npuGuard.set_index(devices[i].index());
            int32_t ret = fn(tensors[i], zbalStreams[i], zbalComms[i], peerP2pIdx);
            ZBAL_CHECK_S(ret == 0, "zbal process group fn exec failed");
        }
    }

    post(zbalStreams, work);
    {
        c10_npu::NPUMultiStreamGuard guard(zbalStreams);
        work->future_ = c10::make_intrusive<at::ivalue::Future>(c10::ListType::create(c10::TensorType::get()), devices);
        work->future_->markCompleted(at::IValue(*work->outputs_));
    }

    for (size_t i = 0; i < tensors.size(); ++i) {
        c10_npu::NPUStream &zbalStream = zbalStreams[i];
        (*(work->zbalEndEvents_))[i].record(zbalStream);
        ZBAL_LOG_DEBUG(
            "Event: record zbal work is successfully executed, event=" << (*(work->zbalEndEvents_))[i].event());
        work->zbalComms_[i] = zbalComms[i];
    }
    work->blockingWait_ = blockingWait_;
    work->opTimeout_ = opTimeout_;
    return work;
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBAL::allreduce(std::vector<at::Tensor> &tensors,
                                                           const c10d::AllreduceOptions &opts)
{
    std::vector<at::Tensor> inputTensors = {tensors[0]};
    std::vector<at::Tensor> outputTensors = {tensors[0]};
    ZBAL_CHECK_S(CheckNpuTensorsDifferentDevices(inputTensors) == 0, "check input tensor failed.");
    ZBAL_CHECK_S(CheckNpuTensorsDifferentDevices(outputTensors) == 0, "check output tensor failed.");

    return collective(
        inputTensors, outputTensors,
        [&](at::Tensor &input, at::Tensor &output, c10_npu::NPUStream &stream, zbal_comm_t comm) {
            RECORD_FUNCTION("ZbalAllReduce", std::vector<c10::IValue>({}));
            c10_npu::NPUCachingAllocator::recordStream(output.storage().data_ptr(), stream);

            auto scalarType = input.scalar_type();
            void *inputDataPtr = input.data_ptr();
            void *outputDataPtr = output.data_ptr();
            auto numel = GetNumelForZBAL(input);
            auto zbalType = GetZbalDataType(scalarType);
            auto zbalReduceOp = GetZbalReduceOp(opts.reduceOp);

            ZBAL_CHECK_S(ZbalReduceSupportDataType(scalarType), "Unsupported data type for ZBAL allreduce");

            auto slice = numel / size_;
            auto elements = (rank_ != size_ - 1) ? slice : numel - (size_ - 1) * slice;
            int64_t bufferElems = static_cast<int64_t>(numel < static_cast<size_t>(size_) ? numel : elements);
            at::Tensor bufferTensor =
                at::empty({bufferElems}, at::TensorOptions().device(input.device()).dtype(scalarType));
            void *bufferDataPtr = bufferTensor.data_ptr();

            std::function<int()> call_all_reduce = [inputDataPtr, outputDataPtr, bufferDataPtr, numel, bufferElems,
                                                    zbalType, zbalReduceOp, comm, stream]() -> int {
                auto result = zbal_all_reduce(inputDataPtr, outputDataPtr, bufferDataPtr, numel, bufferElems, zbalType,
                                              zbalReduceOp, comm, stream.stream(false));
                return result;
            };
            at_npu::native::OpCommand::RunOpApiV2("zbal_all_reduce", call_all_reduce);
            return Z_OK;
        },
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {},
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {},
        c10d::OpType::ALLREDUCE);
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBAL::_allgather_base(at::Tensor &outputTensor, at::Tensor &inputTensor,
                                                                 const c10d::AllgatherOptions &opts)
{
    (void)opts;
    if (inputTensor.dtype() != outputTensor.dtype()) {
        ZBAL_CHECK_S(false, "output tensor must have the same dtype as input tensor");
    }

    if (inputTensor.numel() * size_ != outputTensor.numel()) {
        ZBAL_CHECK_S(false, "output tensor size(", outputTensor.numel(), ") must be equal to world_size(", size_,
                     ") times input tensor size(", inputTensor.numel(), ")");
    }

    std::vector<at::Tensor> inputTensors = {inputTensor};
    std::vector<at::Tensor> outputTensors = {outputTensor};
    ZBAL_CHECK_S(CheckNpuTensorsDifferentDevices(inputTensors) == 0, "check input tensor failed.");
    ZBAL_CHECK_S(CheckNpuTensorsDifferentDevices(outputTensors) == 0, "check output tensor failed.");

    return collective(
        inputTensors, outputTensors,
        [&](at::Tensor &input, at::Tensor &output, c10_npu::NPUStream &stream, zbal_comm_t comm) {
            RECORD_FUNCTION("ZBALAllGatherBase", std::vector<c10::IValue>({input}));
            c10_npu::NPUCachingAllocator::recordStream(output.storage().data_ptr(), stream);

            void *inputDataPtr = input.data_ptr();
            void *outputDataPtr = output.data_ptr();
            auto numel = GetNumelForZBAL(input);
            auto zbalType = GetZbalDataType(input.scalar_type());

            std::function<int()> call_all_gather = [inputDataPtr, outputDataPtr, numel, zbalType, comm,
                                                    stream]() -> int {
                auto result = zbal_all_gather(inputDataPtr, outputDataPtr, numel, zbalType, comm, stream.stream(false));
                return result;
            };
            at_npu::native::OpCommand::RunOpApiV2("zbal_all_gather_base", call_all_gather);
            return Z_OK;
        },
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {},
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {},
        c10d::OpType::ALLGATHER);
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBAL::allgather(std::vector<std::vector<at::Tensor>> &outputTensors,
                                                           std::vector<at::Tensor> &inputTensors,
                                                           const c10d::AllgatherOptions &opts)
{
    (void)opts;
    CheckNpuTensorsSameDevice(outputTensors.back());
    ZBAL_CHECK_S(CheckNpuTensorsDifferentDevices(inputTensors) == 0, "check input tensor failed.");

    bool sameSize = CheckSameSize(outputTensors.back());
    if (!sameSize) {
        ZBAL_CHECK_S(false, "Un-support tensors with different size");
    }

    int outSize = static_cast<int>(outputTensors[0].size());
    uint64_t outputNums[outSize];
    for (const auto i : c10::irange(outputTensors.size())) {
        for (const auto j : c10::irange(outSize)) {
            outputNums[j] = static_cast<uint64_t>(outputTensors[i][j].numel());
        }
    }

    auto outputFlattened = FlattenForScatterGather(outputTensors, inputTensors, size_);
    ZBAL_CHECK_S(CheckNpuTensorsDifferentDevices(outputFlattened) == 0, "check input tensor failed.");

    return collective(
        inputTensors, outputFlattened,
        [&](at::Tensor &input, at::Tensor &output, c10_npu::NPUStream &stream, zbal_comm_t comm) {
            RECORD_FUNCTION("ZBALAllGather", std::vector<c10::IValue>({input}));
            c10_npu::NPUCachingAllocator::recordStream(output.storage().data_ptr(), stream);

            auto inputDataPtr = input.data_ptr();
            auto outputDataPtr = output.data_ptr();
            auto numel = GetNumelForZBAL(input);
            auto zbalType = GetZbalDataType(input.scalar_type());

            std::function<int()> call_all_gather = [inputDataPtr, outputDataPtr, numel, zbalType, comm,
                                                    stream]() -> int {
                return zbal_all_gather(inputDataPtr, outputDataPtr, numel, zbalType, comm, stream.stream(false));
            };
            at_npu::native::OpCommand::RunOpApiV2("zbal_all_gather", call_all_gather);
            return Z_OK;
        },
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {},
        [&](std::vector<c10_npu::NPUStream> &streams, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &work) {
            // Copy the flattened output tensors to the outputs.
            (void)work;
            for (const auto i : c10::irange(outputTensors.size())) {
                c10_npu::NPUStreamGuard guard(streams[i]);
                for (const auto j : c10::irange(outputTensors[0].size())) {
                    c10_npu::NPUCachingAllocator::recordStream(outputTensors[i][j].storage().data_ptr(), streams[i]);
                    at::Tensor outputTensor = outputFlattened[i][j].slice(0, 0, outputNums[j]);
                    at::Tensor outputTensorShape = at::reshape(outputTensor, outputTensors[i][j].sizes());
                    outputTensors[i][j].copy_(outputTensorShape, true);
                }
            }
        },
        c10d::OpType::ALLGATHER);
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBAL::_reduce_scatter_base(at::Tensor &outputTensor, at::Tensor &inputTensor,
                                                                      const c10d::ReduceScatterOptions &opts)
{
    if (inputTensor.dtype() != outputTensor.dtype()) {
        ZBAL_CHECK_S(false, "output tensor must have the same dtype as input tensor");
    }

    if (inputTensor.numel() != outputTensor.numel() * size_) {
        ZBAL_CHECK_S(false, "input tensor size(", inputTensor.numel(), ") must be equal to world_size(", size_,
                     ") times output tensor size(", outputTensor.numel(), ")");
    }

    std::vector<at::Tensor> inputTensors = {inputTensor};
    std::vector<at::Tensor> outputTensors = {outputTensor};
    ZBAL_CHECK_S(CheckNpuTensorsDifferentDevices(inputTensors) == 0, "check input tensor failed.");
    ZBAL_CHECK_S(CheckNpuTensorsDifferentDevices(outputTensors) == 0, "check output tensor failed.");

    return collective(
        inputTensors, outputTensors,
        [&](at::Tensor &input, at::Tensor &output, c10_npu::NPUStream &stream, zbal_comm_t comm) {
            RECORD_FUNCTION("ZbalReduceScatterBase", std::vector<c10::IValue>({}));
            c10_npu::NPUCachingAllocator::recordStream(output.storage().data_ptr(), stream);

            auto scalarType = input.scalar_type();
            void *inputDataPtr = input.data_ptr();
            void *outputDataPtr = output.data_ptr();
            auto numel = GetNumelForZBAL(output);
            auto zbalType = GetZbalDataType(scalarType);
            auto zbalReduceOp = GetZbalReduceOp(opts.reduceOp);

            ZBAL_CHECK_S(ZbalReduceSupportDataType(scalarType), "Unsupported data type for ZBAL reduce scatter");

            std::function<int()> call_reduce_scatter = [inputDataPtr, outputDataPtr, numel, zbalType, zbalReduceOp,
                                                        comm, stream]() -> int {
                auto result = zbal_reduce_scatter(inputDataPtr, outputDataPtr, numel, zbalType, zbalReduceOp, comm,
                                                  stream.stream(false));
                return result;
            };
            at_npu::native::OpCommand::RunOpApiV2("zbal_reduce_scatter_base", call_reduce_scatter);
            return Z_OK;
        },
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {},
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {},
        c10d::OpType::REDUCE_SCATTER);
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBAL::broadcast(std::vector<at::Tensor> &tensors,
                                                           const c10d::BroadcastOptions &opts)
{
    ZBAL_CHECK_S(CheckNpuTensorsDifferentDevices(tensors) == 0, "check input tensor failed.");
    return collective(
        tensors, tensors,
        [&](at::Tensor &input, at::Tensor &output, c10_npu::NPUStream &stream, zbal_comm_t comm) {
            RECORD_FUNCTION("ZbalBroadcast", std::vector<c10::IValue>({input}));
            c10_npu::NPUCachingAllocator::recordStream(output.storage().data_ptr(), stream);
            const auto root = opts.rootRank * tensors.size() + opts.rootTensor;

            void *inputDataPtr = input.data_ptr();
            auto numel = GetNumelForZBAL(input);
            auto zbalType = GetZbalDataType(input.scalar_type());

            std::function<int()> call_broadcast = [inputDataPtr, numel, zbalType, root, comm, stream]() -> int {
                auto result = zbal_broadcast(inputDataPtr, numel, zbalType, root, comm, stream.stream(false));
                return result;
            };
            at_npu::native::OpCommand::RunOpApiV2("zbal_broadcast", call_broadcast);
            return Z_OK;
        },
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {},
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {},
        c10d::OpType::BROADCAST);
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBAL::scatter(std::vector<at::Tensor> &outputTensors,
                                                         std::vector<std::vector<at::Tensor>> &inputTensors,
                                                         const c10d::ScatterOptions &opts)
{
    const bool is_root = (myWorldRank_ == opts.rootRank);
    std::vector<uint64_t> rank_data_addrs;
    void *bufferDataPtr = nullptr;
    if (is_root) {
        if (inputTensors.size() != 1) {
            ZBAL_LOG_ERROR("requires a single-element input list containing a list with tensors.");
        }

        for (auto &tensor_list : inputTensors[0]) {
            rank_data_addrs.push_back(reinterpret_cast<uint64_t>(tensor_list.data_ptr()));
        }
        // Allocate device memory to hold the tensor address list
        int64_t addrListSize = static_cast<int64_t>(rank_data_addrs.size() * sizeof(uint64_t));
        at::Tensor bufferTensor =
            at::empty({addrListSize}, at::TensorOptions().device(inputTensors[0][0].device()).dtype(torch::kInt8));
        auto result = DlCannApi::AclrtMemcpy(reinterpret_cast<void *>(bufferTensor.data_ptr()), addrListSize,
                                             rank_data_addrs.data(), addrListSize, ACL_MEMCPY_HOST_TO_DEVICE);
        if (result != Z_OK) {
            ZBAL_LOG_ERROR("tensor addrs h2d copy failed, result: " << result);
        }
        bufferDataPtr = bufferTensor.data_ptr();
    }
    std::vector<at::Tensor> dummy_inputs = outputTensors;

    return collective(
        dummy_inputs, outputTensors,
        [&](at::Tensor &, at::Tensor &output, c10_npu::NPUStream &stream, zbal_comm_t comm) {
            RECORD_FUNCTION("ZbalScatter", std::vector<c10::IValue>({output}));
            c10_npu::NPUCachingAllocator::recordStream(output.storage().data_ptr(), stream);

            const uint16_t root_rank = opts.rootRank;
            auto zbalType = GetZbalDataType(output.scalar_type());
            uint64_t recv_numel = GetNumelForZBAL(output);
            std::function<int()> call_scatter = [=]() -> int {
                return zbal_scatter(bufferDataPtr, output.data_ptr(), recv_numel, zbalType, root_rank, comm,
                                    stream.stream(false));
            };
            at_npu::native::OpCommand::RunOpApiV2("zbal_scatter", call_scatter);
            return Z_OK;
        },
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {},
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {},
        c10d::OpType::SCATTER);
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBAL::reduce_scatter(std::vector<at::Tensor> &outputTensors,
                                                                std::vector<std::vector<at::Tensor>> &inputTensors,
                                                                const c10d::ReduceScatterOptions &opts)
{
    ZBAL_CHECK_S(CheckNpuTensorsDifferentDevices(outputTensors) == 0, "check output tensor failed.");
    CheckNpuTensorsSameDevice(inputTensors.back());

    bool sameSize = CheckSameSize(inputTensors.back());
    if (!sameSize) {
        ZBAL_CHECK_S(false, "Un-support tensors with different size");
    }

    auto inputFlattened = FlattenForScatterGather(inputTensors, outputTensors, size_);
    ZBAL_CHECK_S(CheckNpuTensorsDifferentDevices(inputFlattened) == 0, "check input tensor failed.");

    return collective(
        inputFlattened, outputTensors,
        [&](at::Tensor &input, at::Tensor &output, c10_npu::NPUStream &stream, zbal_comm_t comm) {
            RECORD_FUNCTION("ZbalReduceScatter", std::vector<c10::IValue>({}));
            c10_npu::NPUCachingAllocator::recordStream(output.storage().data_ptr(), stream);

            auto scalarType = input.scalar_type();
            void *inputDataPtr = input.data_ptr();
            void *outputDataPtr = output.data_ptr();
            auto numel = GetNumelForZBAL(output);
            auto zbalType = GetZbalDataType(scalarType);
            auto zbalReduceOp = GetZbalReduceOp(opts.reduceOp);

            ZBAL_CHECK_S(ZbalReduceSupportDataType(scalarType), "Unsupported data type for ZBAL reduce scatter");

            std::function<int()> call_reduce_scatter = [inputDataPtr, outputDataPtr, numel, zbalType, zbalReduceOp,
                                                        comm, stream]() -> int {
                auto result = zbal_reduce_scatter(inputDataPtr, outputDataPtr, numel, zbalType, zbalReduceOp, comm,
                                                  stream.stream(false));
                return result;
            };
            at_npu::native::OpCommand::RunOpApiV2("zbal_reduce_scatter", call_reduce_scatter);
            return Z_OK;
        },
        [&](std::vector<c10_npu::NPUStream> &streams, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &work) {
            (void)work;
            for (const auto i : c10::irange(inputTensors.size())) {
                c10_npu::NPUStreamGuard guard(streams[i]);
                for (const auto j : c10::irange(inputTensors[0].size())) {
                    c10_npu::NPUCachingAllocator::recordStream(inputTensors[i][j].storage().data_ptr(), streams[i]);
                    inputFlattened[i][j].copy_(inputTensors[i][j], true);
                }
            }
        },
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {},
        c10d::OpType::REDUCE_SCATTER);

    return nullptr;
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBAL::barrier(const c10d::BarrierOptions &opts)
{
    (void)opts;
    at::Tensor tensor1 = at::ones({1}, at::TensorOptions().device(c10::DeviceType::PrivateUse1).dtype(at::kFloat));
    at::Tensor tensor2 = at::ones({1}, at::TensorOptions().device(c10::DeviceType::PrivateUse1).dtype(at::kFloat));
    std::vector<at::Tensor> inputTensors = {tensor1};
    std::vector<at::Tensor> outputTensors = {tensor2};
    return collective(
        inputTensors, outputTensors,
        [&](at::Tensor &input, at::Tensor &output, c10_npu::NPUStream &stream, zbal_comm_t comm) {
            (void)input;
            (void)output;
            auto barrier_call = [comm, stream]() -> int { return zbal_barrier(comm, stream.stream(false)); };
            at_npu::native::OpCommand::RunOpApiV2("zbal_barrier", barrier_call);
            return Z_OK;
        },
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {},
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {},
        c10d::OpType::ALLTOALL_BASE);
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBAL::alltoall_base(at::Tensor &outputTensor, at::Tensor &inputTensor,
                                                               std::vector<int64_t> &outputSplits,
                                                               std::vector<int64_t> &inputSplits,
                                                               const c10d::AllToAllOptions &opts)
{
    (void)opts;
    ZBAL_CHECK_S(outputTensor.dtype() == inputTensor.dtype(), "input output tensor must be same type.");
    ZBAL_CHECK_S(outputTensor.scalar_type() == inputTensor.scalar_type(), "input output tensor must be same type.");
    ZBAL_CHECK_S(inputTensor.size(0) > 0, "input tensor shape is 0");

    CheckSingleTensor(outputTensor);
    CheckSingleTensor(inputTensor);

    std::vector<at::Tensor> inputTensors = {inputTensor};
    std::vector<at::Tensor> outputTensors = {outputTensor};
    CheckNpuTensorsDifferentDevices(inputTensors);
    CheckNpuTensorsDifferentDevices(outputTensors);

    if (outputSplits.empty()) {
        int64_t avgDim0 = static_cast<int64_t>(outputTensor.size(0) / size_);
        for (int64_t i = 0; i < size_; i++) {
            outputSplits.push_back(avgDim0);
        }
    }
    if (inputSplits.empty()) {
        int64_t avgDim0 = static_cast<int64_t>(inputTensor.size(0) / size_);
        for (int64_t i = 0; i < size_; i++) {
            inputSplits.push_back(avgDim0);
        }
    }

    CheckSplitSize(outputSplits, outputTensor, size_);
    CheckSplitSize(inputSplits, inputTensor, size_);

    std::vector<int64_t> inputCumSum(size_ * ZBAL_FLAG_SIZE, 0);
    std::vector<int64_t> outputCounts(size_ * ZBAL_FLAG_SIZE, 0);
    uint64_t outputSizePerRow = outputTensor.size(0) > 0 ? outputTensor.numel() / outputTensor.size(0) : 0;
    uint64_t inputSizePerRow = inputTensor.numel() / inputTensor.size(0);
    ZBAL_CHECK_S(outputSizePerRow == 0 || outputSizePerRow == inputSizePerRow, "unexpect row element in input/output");
    uint64_t prevCumSum = 0;
    for (int i = 0; i < size_; i++) {
        outputCounts[i * ZBAL_FLAG_SIZE] = outputSplits[i] * outputSizePerRow;
        inputCumSum[i * ZBAL_FLAG_SIZE] = prevCumSum;
        prevCumSum = inputSplits[i] * inputSizePerRow + prevCumSum;
    }

    const int inoutPtrSize = 2;
    std::vector<int64_t> elements(inoutPtrSize, 0);
    elements[0] = static_cast<uint64_t>(inputTensor.numel());
    elements[1] = static_cast<uint64_t>(outputTensor.numel());

    at::Tensor inCumSumTensor = torch::tensor(inputCumSum, torch::kInt64).to(inputTensor.device());
    at::Tensor outCountTensor = torch::tensor(outputCounts, torch::kInt64).to(inputTensor.device());
    at::Tensor elementTensor = torch::tensor(elements, torch::kInt64).to(inputTensor.device());

    return collective(
        inputTensors, outputTensors,
        [&](at::Tensor &input, at::Tensor &output, c10_npu::NPUStream &stream, zbal_comm_t comm) {
            RECORD_FUNCTION("ZbalAlltoAllV", std::vector<c10::IValue>({}));
            auto inputPtr = input.data_ptr();
            auto outputPtr = output.data_ptr();
            auto inCumSumPtr = inCumSumTensor.data_ptr();
            auto outCountPtr = outCountTensor.data_ptr();
            auto elementPtr = elementTensor.data_ptr();
            auto dataType = GetZbalDataType(input.scalar_type());
            auto alltoall_call = [inputPtr, outputPtr, inCumSumPtr, outCountPtr, elementPtr, dataType, comm,
                                  stream]() -> int {
                return zbal_all_to_all_v(inputPtr, outputPtr, inCumSumPtr, outCountPtr, elementPtr, dataType, comm,
                                         stream.stream(false));
            };
            at_npu::native::OpCommand::RunOpApiV2("zbal_all_to_all_v", alltoall_call);
            return Z_OK;
        },
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {},
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {},
        c10d::OpType::ALLTOALL_BASE);
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBAL::send(std::vector<at::Tensor> &tensors, int dstRank, int tag)
{
    (void)tag;
    ZBAL_CHECK_S(CheckNpuTensorsDifferentDevices(tensors) == 0, "check output tensor failed.");

    return pointToPoint(
        tensors,
        [&](at::Tensor &tensor, c10_npu::NPUStream &stream, zbal_comm_t comm, int peer) {
            RECORD_FUNCTION("ZbalSend", std::vector<c10::IValue>({tensor}));

            auto dataPtr = tensor.data_ptr();
            auto numel = GetNumelForZBAL(tensor);
            auto zbalType = GetZbalDataType(tensor.scalar_type());

            auto call_send = [dataPtr, numel, zbalType, peer, comm, stream]() -> int {
                return zbal_send(dataPtr, numel, zbalType, peer, comm, stream.stream(false));
            };

            at_npu::native::OpCommand::RunOpApiV2("zbal_send", call_send);
            return Z_OK;
        },
        dstRank, c10d::OpType::SEND,
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {},
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {});
}

c10::intrusive_ptr<c10d::Work> ProcessGroupZBAL::recv(std::vector<at::Tensor> &tensors, int srcRank, int tag)
{
    (void)tag;
    ZBAL_CHECK_S(CheckNpuTensorsDifferentDevices(tensors) == 0, "check output tensor failed.");

    return pointToPoint(
        tensors,
        [&](at::Tensor &tensor, c10_npu::NPUStream &stream, zbal_comm_t comm, int peer) {
            RECORD_FUNCTION("ZbalRecv", std::vector<c10::IValue>({tensor}));

            auto dataPtr = tensor.data_ptr();
            auto numel = GetNumelForZBAL(tensor);
            auto zbalType = GetZbalDataType(tensor.scalar_type());

            auto call_recv = [dataPtr, numel, zbalType, peer, comm, stream]() -> int {
                return zbal_recv(dataPtr, numel, zbalType, peer, comm, stream.stream(false));
            };

            at_npu::native::OpCommand::RunOpApiV2("zbal_recv", call_recv);
            return Z_OK;
        },
        srcRank, c10d::OpType::RECV,
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {},
        [&](std::vector<c10_npu::NPUStream> &, c10::intrusive_ptr<ProcessGroupZBAL::WorkZBAL> &) {});
}

std::string ProcessGroupZBAL::getZBALCommName() noexcept
{
    return groupName_;
}

ProcessGroupZBAL::Options::Options(bool isHighPriorityStream)
    : c10d::Backend::Options(ZBAL_BACKEND_NAME), opTimeout(WORKER_MAX_TIMEOUT),
      isHighPriorityStream(isHighPriorityStream)
{}

ProcessGroupZBAL::~ProcessGroupZBAL()
{
    if (groupComm_ != nullptr) {
        auto result = zbal_comm_destroy(groupComm_, 0);
        if (result != Z_OK) {
            ZBAL_LOG_WARN("~ process group: " << groupName_ << " on rank " << myWorldRank_ << " result " << result);
            return;
        }
        groupComm_ = nullptr;
        ZBAL_LOG_DEBUG("~ process group success: " << groupName_ << " on rank " << myWorldRank_);
    }

    for (auto &[groupName, groupComm] : groupP2pComms_) {
        auto result = zbal_comm_destroy(groupComm, 0);
        if (result != Z_OK) {
            ZBAL_LOG_WARN("~ process group: " << groupName << " on rank " << myWorldRank_ << " result " << result);
            return;
        }
        ZBAL_LOG_DEBUG("~ process group success: " << groupName << " on rank " << myWorldRank_);
    }
    groupP2pComms_.clear();
}

} // namespace pytorch_npu
} // namespace adaptor
} // namespace zbal
