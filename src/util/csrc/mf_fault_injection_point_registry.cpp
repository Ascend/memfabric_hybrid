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

#include "mf_fault_injection_point_registry.h"

#include <array>
#include <cstdint>
#include <string>
#include <unistd.h>

namespace ock {
namespace mf {

namespace {

constexpr char FAULT_INJECTION_POINT_CONFIG_FILE_PREFIX[] = "/tmp/mf_failpoints_";
constexpr char FAULT_INJECTION_POINT_CONFIG_FILE_SUFFIX[] = ".conf";

constexpr int32_t FAULT_INJECTION_POINT_ERROR_RESULT = -1;

using DefaultPointRegisterHandler = FaultInjectionPointStatus (*)(const char *pointName, const char *pointDesc);

struct FaultInjectionPointDefinition {
    const char *name;
    const char *desc;
    DefaultPointRegisterHandler registerHandler;
};

void InjectErrorResult(FaultInjectionPointParam *userParam, int32_t *result)
{
    (void)userParam;
    if (result != nullptr) {
        *result = FAULT_INJECTION_POINT_ERROR_RESULT;
    }
}

template<auto Callback>
FaultInjectionPointStatus RegisterPointWithCallback(const char *pointName, const char *pointDesc)
{
    FaultInjectionPointStatus initStatus = FaultInjectionPointManager::Init();
    if (initStatus != FaultInjectionPointStatus::OK) {
        return initStatus;
    }

    FaultInjectionPointStatus registerStatus = FaultInjectionPointManager::Register(pointName, pointDesc, Callback);
    if (registerStatus != FaultInjectionPointStatus::OK) {
        (void)FaultInjectionPointManager::Exit();
        return registerStatus;
    }
    return FaultInjectionPointStatus::OK;
}

[[maybe_unused]] FaultInjectionPointStatus RegisterPointWithoutCallback(const char *pointName, const char *pointDesc)
{
    FaultInjectionPointStatus initStatus = FaultInjectionPointManager::Init();
    if (initStatus != FaultInjectionPointStatus::OK) {
        return initStatus;
    }

    FaultInjectionPointStatus registerStatus = FaultInjectionPointManager::Register(pointName, pointDesc);
    if (registerStatus != FaultInjectionPointStatus::OK) {
        (void)FaultInjectionPointManager::Exit();
        return registerStatus;
    }
    return FaultInjectionPointStatus::OK;
}

FaultInjectionPointStatus UnregisterPoint(const char *pointName)
{
    FaultInjectionPointStatus unregisterStatus = FaultInjectionPointManager::Unregister(pointName);
    FaultInjectionPointStatus exitStatus = FaultInjectionPointManager::Exit();
    if (unregisterStatus != FaultInjectionPointStatus::OK && unregisterStatus != FaultInjectionPointStatus::NOT_FOUND) {
        return unregisterStatus;
    }
    return exitStatus;
}

bool HasFaultInjectionPointConfigFile()
{
    const std::string configPath = std::string(FAULT_INJECTION_POINT_CONFIG_FILE_PREFIX) + std::to_string(getpid()) +
                                   FAULT_INJECTION_POINT_CONFIG_FILE_SUFFIX;
    return access(configPath.c_str(), F_OK) == 0;
}

constexpr std::array<FaultInjectionPointDefinition, 2> DEFAULT_FAULT_INJECTION_POINTS = {{
    {"ALLOC_LOCAL_MEMORY", "inject alloc local memory failure", &RegisterPointWithCallback<&InjectErrorResult>},
    {"MMAP", "inject mmap failure", &RegisterPointWithCallback<&InjectErrorResult>},
}};

} // namespace

FaultInjectionPointStatus FaultInjectionPointRegistry::Register()
{
    std::size_t registeredCount = 0;
    for (; registeredCount < DEFAULT_FAULT_INJECTION_POINTS.size(); ++registeredCount) {
        const auto &point = DEFAULT_FAULT_INJECTION_POINTS[registeredCount];
        FaultInjectionPointStatus status = point.registerHandler(point.name, point.desc);
        if (status != FaultInjectionPointStatus::OK) {
            for (std::size_t rollbackIndex = registeredCount; rollbackIndex > 0; --rollbackIndex) {
                (void)UnregisterPoint(DEFAULT_FAULT_INJECTION_POINTS[rollbackIndex - 1].name);
            }
            return status;
        }
    }

    if (HasFaultInjectionPointConfigFile()) {
        return FaultInjectionPointManager::Reload();
    }

    return FaultInjectionPointStatus::OK;
}

FaultInjectionPointStatus FaultInjectionPointRegistry::Unregister()
{
    FaultInjectionPointStatus finalStatus = FaultInjectionPointStatus::OK;

    for (std::size_t index = DEFAULT_FAULT_INJECTION_POINTS.size(); index > 0; --index) {
        FaultInjectionPointStatus status = UnregisterPoint(DEFAULT_FAULT_INJECTION_POINTS[index - 1].name);
        if (status != FaultInjectionPointStatus::OK) {
            finalStatus = status;
        }
    }

    return finalStatus;
}

} // namespace mf
} // namespace ock
