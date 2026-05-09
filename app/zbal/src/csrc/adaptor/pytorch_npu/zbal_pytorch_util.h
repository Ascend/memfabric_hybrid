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
#ifndef ZBAL_ADAPTOR_PY_UTIL_H
#define ZBAL_ADAPTOR_PY_UTIL_H

#include <mutex>
#include <atomic>
#include <torch/csrc/distributed/c10d/Types.hpp>
#include "zbal_def.h"
#include "torch_npu/csrc/core/npu/sys_ctrl/npu_sys_ctrl.h"
#include "torch_npu/csrc/framework/FormatHelper.h"
#include "torch_npu/csrc/core/NPUBridge.h"
#include "torch_npu/csrc/core/npu/NPUGuard.h"
#include "torch_npu/csrc/core/npu/DeviceUtils.h"
#include "torch_npu/csrc/core/npu/NPUFormat.h"
#include "torch_npu/csrc/core/npu/NPUEvent.h"
#include "torch_npu/csrc/framework/OpCommand.h"

namespace zbal {
namespace adaptor {
namespace pytorch_npu {

std::vector<at::Device> GetDeviceList(const std::vector<at::Tensor> &tensors);

std::string GetKeyFromDevices(const std::vector<at::Device> &devices);

void SyncStreams(const std::vector<at::Device> &devices,
    std::vector<c10_npu::NPUEvent> &zbalEvents,
    std::vector<c10_npu::NPUStream> &zbalStreams);

void CheckTensors(const std::vector<at::Tensor> &tensors);

void CheckSingleTensor(const at::Tensor &tensor);

// std::vector<at::Tensor> CastOriginFormat(const std::vector<at::Tensor>& inputTensors);

int32_t CheckNpuTensorsDifferentDevices(const std::vector<at::Tensor> &tensors);

uint64_t GetNumelForZBAL(const at::Tensor &t);

zbal_datatype_t GetZbalDataType(at::ScalarType type);

zbal_reduce_op_t GetZbalReduceOp(const c10d::ReduceOp op);

void CheckSplitSize(const std::vector<int64_t> &splits, const at::Tensor &tensor, int groupSize);

bool CheckSameSize(const std::vector<at::Tensor>& input_tensors);

void CheckNpuTensorsSameDevice(const std::vector<at::Tensor> &tensors);

std::vector<at::Tensor> FlattenForScatterGather(std::vector<std::vector<at::Tensor>>& tensorLists,
                                                std::vector<at::Tensor>& other, size_t worldSize);

at::Tensor ZbalNewLikeFlat(std::vector<std::vector<at::Tensor>> &tensors, size_t deviceIdx);

}
}
}

struct OptionsManager {
    static bool IsHcclZeroCopyEnable;
    static bool CheckForceUncached;
};

std::string ZBALFormatErrorCode(int32_t errorCode);

#define PTA_ERROR_MOCK(err_code) ZBALFormatErrorCode((int32_t)(err_code))
#define OPS_ERROR_MOCK(err_code) ZBALFormatErrorCode((int32_t)(err_code))

#define NPU_CHECK_ERROR_MOCK(err_code, ...)                                  \
    do {                                                                     \
        int error_code = err_code;                                           \
        if ((error_code) != ACL_ERROR_NONE) {                                \
            std::ostringstream oss;                                          \
            oss << " NPU function error: [ShmemAllocator Currently do not support detail error log]" << std::endl; \
            std::string err_msg = oss.str();                                 \
            ASCEND_LOGE("%s", err_msg.c_str());                              \
        }                                                                    \
    } while (0)

#define NPU_CHECK_WARN_MOCK(err_code, ...)                                   \
    do {                                                                     \
        int error_code = err_code;                                           \
        if ((error_code) != ACL_ERROR_NONE) {                                \
            std::ostringstream oss;                                          \
            oss << " NPU function warning: [ShmemAllocator Currently do not support detail warning log]" << std::endl; \
            std::string err_msg = oss.str();                                 \
            ASCEND_LOGW("%s", err_msg.c_str());                              \
        }                                                                    \
    } while (0)

const int32_t ACL_SYNC_TIMEOUT = 3600 * 1000;  // ms

#endif