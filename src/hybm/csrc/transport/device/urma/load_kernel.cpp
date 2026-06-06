/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/

#include "load_kernel.h"

#include <climits>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "hybm_logger.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
namespace {
constexpr int32_t kCpuKernelMode = 0;
constexpr const char *kKernelJsonSuffix = "/opp/vendors/cust/op_impl/aicpu/config/libcann_hybm_kernel.json";
constexpr const char *kDefaultAscendPath = "/usr/local/Ascend/cann";

Result GetKernelFilePath(std::string &jsonPath)
{
    const char *ascendPath = std::getenv("ASCEND_HOME_PATH");
    if (ascendPath == nullptr || ascendPath[0] == '\0') {
        ascendPath = kDefaultAscendPath;
    }
    jsonPath = std::string(ascendPath) + kKernelJsonSuffix;
    BM_LOG_INFO("device_urma kernel json path: " << jsonPath);
    return BM_OK;
}

Result LoadBinaryFromJson(const char *jsonPath, aclrtBinHandle &binHandle)
{
    if (jsonPath == nullptr) {
        BM_LOG_ERROR("device_urma jsonPath is null in LoadBinaryFromJson");
        return BM_INVALID_PARAM;
    }

    char resolvedPath[PATH_MAX] = {};
    if (realpath(jsonPath, resolvedPath) == nullptr) {
        BM_LOG_ERROR("realpath failed for kernel json: " << jsonPath);
        return BM_FILE_NOT_ACCESS;
    }
    if (access(resolvedPath, R_OK) != 0) {
        BM_LOG_ERROR("kernel json is not readable: " << resolvedPath);
        return BM_FILE_NOT_ACCESS;
    }

    aclrtBinaryLoadOption loadOption{};
    loadOption.type = aclrtBinaryLoadOptionType::ACL_RT_BINARY_LOAD_OPT_CPU_KERNEL_MODE;
    loadOption.value.cpuKernelMode = kCpuKernelMode;
    aclrtBinaryLoadOptions loadOptions{};
    loadOptions.options = &loadOption;
    loadOptions.numOpt = 1;

    const auto ret = DlAclApi::AclrtBinaryLoadFromFile(resolvedPath, &loadOptions, &binHandle);
    if (ret != BM_OK) {
        BM_LOG_ERROR("aclrtBinaryLoadFromFile failed, path: " << resolvedPath << " ret: " << ret);
        return ret;
    }
    return BM_OK;
}

Result GetFuncHandle(aclrtBinHandle binHandle, const char *funcName, aclrtFuncHandle &funcHandle)
{
    if (binHandle == nullptr || funcName == nullptr) {
        BM_LOG_ERROR("device_urma binHandle or funcName is null in GetFuncHandle");
        return BM_INVALID_PARAM;
    }
    const auto ret = DlAclApi::AclrtBinaryGetFunction(binHandle, funcName, &funcHandle);
    if (ret != BM_OK) {
        BM_LOG_ERROR("aclrtBinaryGetFunction failed, func: " << funcName << " ret: " << ret);
        return ret;
    }
    if (funcHandle == nullptr) {
        BM_LOG_ERROR("device_urma funcHandle is null after aclrtBinaryGetFunction");
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}
} // namespace

Result LoadDeviceKernelAndGetHandles(const char *funcRead, const char *funcWrite, aclrtBinHandle &binHandle,
                                     DeviceFuncHandles &funcHandles)
{
    funcHandles = DeviceFuncHandles{};
    std::string jsonPath;
    auto ret = GetKernelFilePath(jsonPath);
    if (ret != BM_OK) {
        return ret;
    }
    if (binHandle == nullptr) {
        ret = LoadBinaryFromJson(jsonPath.c_str(), binHandle);
        if (ret != BM_OK) {
            return ret;
        }
    }
    ret = GetFuncHandle(binHandle, funcRead, funcHandles.batchRead);
    if (ret != BM_OK) {
        return ret;
    }
    return GetFuncHandle(binHandle, funcWrite, funcHandles.batchWrite);
}

} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock
