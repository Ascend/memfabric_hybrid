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

#include <cstdint>
#include <string>
#include <unistd.h>

namespace ock {
namespace mf {

namespace {

constexpr char ALLOC_LOCAL_MEMORY_POINT_NAME[] = "ALLOC_LOCAL_MEMORY";
constexpr char ALLOC_LOCAL_MEMORY_POINT_DESC[] = "inject alloc local memory failure";
constexpr char MMAP_POINT_NAME[] = "MMAP";
constexpr char MMAP_POINT_DESC[] = "inject mmap failure";
constexpr char FAULT_INJECTION_POINT_CONFIG_FILE_PREFIX[] = "/tmp/mf_failpoints_";
constexpr char FAULT_INJECTION_POINT_CONFIG_FILE_SUFFIX[] = ".conf";

constexpr int32_t FAULT_INJECTION_POINT_ERROR_RESULT = -1;

void InjectErrorResult(FaultInjectionPointParam *userParam, int32_t *result)
{
    (void)userParam;
    if (result != nullptr) {
        *result = FAULT_INJECTION_POINT_ERROR_RESULT;
    }
}

FaultInjectionPointStatus RegisterPoint(const char *pointName, const char *pointDesc)
{
    FaultInjectionPointStatus initStatus = FaultInjectionPointManager::Init();
    if (initStatus != FaultInjectionPointStatus::OK) {
        return initStatus;
    }

    FaultInjectionPointStatus registerStatus =
        FaultInjectionPointManager::Register(pointName, pointDesc, &InjectErrorResult);
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

} // namespace

FaultInjectionPointStatus FaultInjectionPointRegistry::Register()
{
    FaultInjectionPointStatus hybmStatus = RegisterPoint(ALLOC_LOCAL_MEMORY_POINT_NAME, ALLOC_LOCAL_MEMORY_POINT_DESC);
    if (hybmStatus != FaultInjectionPointStatus::OK) {
        return hybmStatus;
    }

    FaultInjectionPointStatus smemStatus = RegisterPoint(MMAP_POINT_NAME, MMAP_POINT_DESC);
    if (smemStatus != FaultInjectionPointStatus::OK) {
        (void)UnregisterPoint(ALLOC_LOCAL_MEMORY_POINT_NAME);
        return smemStatus;
    }

    if (HasFaultInjectionPointConfigFile()) {
        return FaultInjectionPointManager::Reload();
    }

    return FaultInjectionPointStatus::OK;
}

FaultInjectionPointStatus FaultInjectionPointRegistry::Unregister()
{
    FaultInjectionPointStatus finalStatus = FaultInjectionPointStatus::OK;

    FaultInjectionPointStatus smemStatus = UnregisterPoint(MMAP_POINT_NAME);
    if (smemStatus != FaultInjectionPointStatus::OK) {
        finalStatus = smemStatus;
    }

    FaultInjectionPointStatus hybmStatus = UnregisterPoint(ALLOC_LOCAL_MEMORY_POINT_NAME);
    if (hybmStatus != FaultInjectionPointStatus::OK) {
        finalStatus = hybmStatus;
    }

    return finalStatus;
}

} // namespace mf
} // namespace ock
