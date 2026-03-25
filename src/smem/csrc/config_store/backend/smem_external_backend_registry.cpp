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

#include "smem_external_backend_registry.h"

#include <cstring>
#include <mutex>

namespace ock {
namespace smem {

struct SmemExternalBackendRegistry::RegistryState {
    std::mutex backendOpMutex;
    smem_conf_store_backend_op_t backendOp{};
    bool backendOpRegistered = false;
};

SmemExternalBackendRegistry::RegistryState &SmemExternalBackendRegistry::GetState() noexcept
{
    static RegistryState state;
    return state;
}

void SmemExternalBackendRegistry::ClearBackendOp(smem_conf_store_backend_op_t &backendOp) noexcept
{
    std::memset(&backendOp, 0, sizeof(backendOp));
}

void SmemExternalBackendRegistry::SetExternalBackendOp(const smem_conf_store_backend_op_t &backendOp) noexcept
{
    RegistryState &state = GetState();
    std::lock_guard<std::mutex> lock(state.backendOpMutex);
    state.backendOp = backendOp;
    state.backendOpRegistered = true;
}

bool SmemExternalBackendRegistry::GetExternalBackendOp(smem_conf_store_backend_op_t &backendOp) noexcept
{
    RegistryState &state = GetState();
    std::lock_guard<std::mutex> lock(state.backendOpMutex);
    if (!state.backendOpRegistered) {
        ClearBackendOp(backendOp);
        return false;
    }
    backendOp = state.backendOp;
    return true;
}

void SmemExternalBackendRegistry::ResetExternalBackendOp() noexcept
{
    RegistryState &state = GetState();
    std::lock_guard<std::mutex> lock(state.backendOpMutex);
    ClearBackendOp(state.backendOp);
    state.backendOpRegistered = false;
}

} // namespace smem
} // namespace ock
