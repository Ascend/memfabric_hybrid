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
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include "zbal_common_includes.h"
#include "dl_cann_api.h"
#include "zbal_pytorch_util.h"

namespace zbal {
namespace adaptor {
namespace pytorch_npu {

const std::map<at::ScalarType, zbal_datatype_t> kScalarTypeToZbalDataType = {
    {at::kByte, ZBAL_DATA_TYPE_UINT8},
    {at::kChar, ZBAL_DATA_TYPE_INT8},
    {at::kShort, ZBAL_DATA_TYPE_INT16},
    {at::kInt, ZBAL_DATA_TYPE_INT32},
    {at::kLong, ZBAL_DATA_TYPE_INT64},
    {at::kHalf, ZBAL_DATA_TYPE_FP16},
    {at::kFloat, ZBAL_DATA_TYPE_FP32},
    {at::kDouble, ZBAL_DATA_TYPE_FP64},
    {at::kBool, ZBAL_DATA_TYPE_UINT8},
    {at::kBFloat16, ZBAL_DATA_TYPE_BFP16},
};

const std::map<c10d::ReduceOp, zbal_reduce_op_t> ReduceOpToZbalReduceOp = {
    {c10d::ReduceOp::MIN, ZBAL_REDUCE_MIN},
    {c10d::ReduceOp::MAX, ZBAL_REDUCE_MAX},
    {c10d::ReduceOp::SUM, ZBAL_REDUCE_SUM},
    {c10d::ReduceOp::PRODUCT, ZBAL_REDUCE_PROD},
};


std::vector<at::Device> GetDeviceList(const std::vector<at::Tensor> &tensors)
{
    std::vector<at::Device> devices;
    devices.reserve(tensors.size());
    for (auto &tensor : tensors) {
        devices.push_back(tensor.device());
    }
    return devices;
}

std::string GetKeyFromDevices(const std::vector<at::Device> &devices)
{
    std::string deviceList;
    for (auto &device : devices) {
        if (deviceList.empty()) {
            deviceList = std::to_string(device.index());
        } else {
            deviceList += "," + std::to_string(device.index());
        }
    }
    return deviceList;
}

void SyncStreams(const std::vector<at::Device> &devices,
                 std::vector<c10_npu::NPUEvent> &events,
                 std::vector<c10_npu::NPUStream> &streams)
{
    for (size_t i = 0; i < devices.size(); ++i) {
        c10_npu::NPUStream &zbalSteam = streams[i];
        c10_npu::NPUEvent &event = events[i];
        event.record(c10_npu::getCurrentNPUStream(devices[i].index()));
        ZBAL_LOG_DEBUG("Event: record zbal group is successfully executed, event=" << event.event());
        event.block(zbalSteam);
        ZBAL_LOG_DEBUG("Event: block zbal group is successfully executed, event=" << event.event());
    }
}

void CheckTensors(const std::vector<at::Tensor> &tensors)
{
    (void)tensors;
}

void CheckSingleTensor(const at::Tensor &tensor)
{
    if (!torch_npu::utils::is_npu(tensor) || tensor.is_sparse()) {
        ZBAL_CHECK_S(false, "tensor must be on npu and dense.");
    }

    if (!tensor.is_contiguous(tensor.suggest_memory_format())) {
        ZBAL_CHECK_S(false, "tensor must be contiguous.");
    }
}

int32_t CheckNpuTensorsDifferentDevices(const std::vector<at::Tensor> &tensors)
{
    if (tensors.size() != 1) {
        ZBAL_LOG_ERROR("Tensor list mustn't be larger than the number of available NPUs");
        return Z_INVALID_PARAM;
    }

    const auto &first = tensors.front();
    std::unordered_set<decltype(first.get_device())> usedDevices;
    usedDevices.reserve(tensors.size());

    for (auto &t : tensors) {
        if (!torch_npu::utils::is_npu(t) || t.is_sparse()) {
            ZBAL_LOG_ERROR("tensors must be NPU and dense");
            return Z_INVALID_PARAM;
        }
        if (t.scalar_type() != first.scalar_type()) {
            ZBAL_LOG_ERROR("tensors must have same scaler type type");
            return Z_INVALID_PARAM;
        }
        if (t.sizes() != first.sizes()) {
            ZBAL_LOG_ERROR("tensors must have same size");
            return Z_INVALID_PARAM;
        }
        if (t.strides() != first.strides()) {
            ZBAL_LOG_ERROR("tensors must have same strides");
            return Z_INVALID_PARAM;
        }
        if (!t.is_contiguous(t.suggest_memory_format())) {
            ZBAL_LOG_ERROR("tensor must be contiguous");
            return Z_INVALID_PARAM;
        }
        const auto inserted = usedDevices.insert(t.get_device()).second;
        if (!inserted) {
            ZBAL_LOG_ERROR("tensors must be on distinct NPU devices");
            return Z_INVALID_PARAM;
        }
    }
    return Z_OK;
}

void CheckNpuTensorsSameDevice(const std::vector<at::Tensor> &tensors)
{
    if (tensors.size() == 0) {
        ZBAL_CHECK_S(false, "Tensor list must be nonempty");
    }

    const auto &first = tensors.front();
    for (const auto &t : tensors) {
        ZBAL_CHECK_S(torch_npu::utils::is_npu(t) && !t.is_sparse(), "Tensor must be NPU and dense");

        ZBAL_CHECK_S(t.scalar_type() == first.scalar_type(), "Tensors must have identical type");

        ZBAL_CHECK_S(t.is_non_overlapping_and_dense(), "Tensors must be non-overlapping and dense");

        ZBAL_CHECK_S(t.get_device() == first.get_device(), "Tensors must be on same NPU device");
    }
}

at::Tensor ZbalNewLikeFlat(std::vector<std::vector<at::Tensor>> &tensors, size_t deviceIdx)
{
    if (tensors.empty() || tensors[0].empty()) {
        ZBAL_CHECK_S(false, "Received an empty list");
    }

    if (deviceIdx >= tensors.size()) {
        ZBAL_CHECK_S(false, "Invalid device index");
    }

    auto &t = tensors[deviceIdx][0];
    auto device = t.device();
    for (const auto i : c10::irange(1, tensors[deviceIdx].size())) {
        if (tensors[deviceIdx][i].device() != device) {
            ZBAL_CHECK_S(false, "Expecting all tensors on the same device");
        }
    }

    at::DeviceGuard gpuGuard(device);
    std::vector<int64_t> sizes{static_cast<int64_t>(tensors[deviceIdx].size())};
    std::vector<int64_t> strides{static_cast<int64_t>(t.numel())};
    sizes.insert(sizes.end(), t.sizes().begin(), t.sizes().end());
    strides.insert(strides.end(), t.strides().begin(), t.strides().end());
    return at::empty_strided(sizes, strides, t.options().memory_format(c10::nullopt));
}

std::vector<at::Tensor> FlattenForScatterGather(std::vector<std::vector<at::Tensor>>& tensorLists,
                                                std::vector<at::Tensor>& other, size_t worldSize)
{
    ZBAL_CHECK_S(tensorLists.size() == other.size(), "input tensors has different size");

    const auto numDevices = tensorLists.size();

    std::vector<at::Tensor> flattened;
    flattened.resize(numDevices);
    for (auto i = size_t{}; i < numDevices; i++) {
        if (tensorLists[i].size() != worldSize * numDevices) {
            ZBAL_CHECK_S(false, "Tensor list input to scatter/gather must match number of collective");
        }

        if (tensorLists[i].front().get_device() != other[i].get_device()) {
            ZBAL_CHECK_S(false, "Corresponding input/output tensors to scatter/gather must all on the same device");
        }

        for (const auto &t : tensorLists[i]) {
            if (t.numel() != other[i].numel()) {
                ZBAL_CHECK_S(false, "All tensor operands to scatter/gather must have the same size");
            }
        }
        // Flatten the tensors (from all ranks) into a single big tensor.
        flattened[i] = ZbalNewLikeFlat(tensorLists, i);
    }
    return flattened;
}

uint64_t GetNumelForZBAL(const at::Tensor &t)
{
    return t.numel();
}

zbal_datatype_t GetZbalDataType(at::ScalarType type)
{
    auto it = kScalarTypeToZbalDataType.find(type);
    if (it == kScalarTypeToZbalDataType.end()) {
        ZBAL_CHECK_S(false, "Unsupported data type for ZBAL process group");
    }
    return kScalarTypeToZbalDataType.at(type);
}

zbal_reduce_op_t GetZbalReduceOp(const c10d::ReduceOp op)
{
    auto it = ReduceOpToZbalReduceOp.find(op);
    if (it == ReduceOpToZbalReduceOp.end()) {
        ZBAL_CHECK_S(false, "Unsupported reduce op for ZBAL process group");
    }
    return ReduceOpToZbalReduceOp.at(op);
}

void CheckSplitSize(const std::vector<int64_t> &splits, const at::Tensor &tensor, int groupSize)
{
    if (splits.size() != static_cast<size_t>(groupSize)) {
        ZBAL_CHECK_S(false, "splits size is not equal to group size");
    }

    const auto sum = c10::sum_integers(splits);
    if (sum != tensor.size(0)) {
        ZBAL_LOG_ERROR("check split size failed, sum=" << sum << ", size0=" << tensor.size(0));
        ZBAL_CHECK_S(false, "splits size and dim 0 of tensor not match");
    }

    for (auto split : splits) {
        ZBAL_CHECK_S(split >= 0, "split element is negative integer");
    }
}

bool CheckSameSize(const std::vector<at::Tensor> &inputTensors)
{
    for (const auto &tensor : inputTensors) {
        if (!inputTensors[0].is_same_size(tensor)) {
            return false;
        }
    }
    return true;
}

}
}
}

bool OptionsManager::IsHcclZeroCopyEnable = false;
bool OptionsManager::CheckForceUncached = false;

std::string ZBALFormatErrorCode(int32_t errorCode)
{
    std::ostringstream oss;
    oss << "\n[ERROR] CODE" << static_cast<int>(errorCode);

    return oss.str();
}
