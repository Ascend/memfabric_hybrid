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

#ifndef SRC_SMEM_CSRC_CONFIG_STORE_BACKEND_SMEM_EXTERNAL_BACKEND_REGISTRY_H_
#define SRC_SMEM_CSRC_CONFIG_STORE_BACKEND_SMEM_EXTERNAL_BACKEND_REGISTRY_H_

#include "smem_def.h"

namespace ock {
namespace smem {

class SmemExternalBackendRegistry final {
public:
    SmemExternalBackendRegistry() = delete;
    ~SmemExternalBackendRegistry() = delete;
    SmemExternalBackendRegistry(const SmemExternalBackendRegistry &) = delete;
    SmemExternalBackendRegistry &operator=(const SmemExternalBackendRegistry &) = delete;
    SmemExternalBackendRegistry(SmemExternalBackendRegistry &&) = delete;
    SmemExternalBackendRegistry &operator=(SmemExternalBackendRegistry &&) = delete;

    static void SetExternalBackendOp(const smem_conf_store_backend_op_t &backendOp) noexcept;
    static bool GetExternalBackendOp(smem_conf_store_backend_op_t &backendOp) noexcept;
    static void ResetExternalBackendOp() noexcept;

private:
    struct RegistryState;

    static RegistryState &GetState() noexcept;
    static void ClearBackendOp(smem_conf_store_backend_op_t &backendOp) noexcept;
};

} // namespace smem
} // namespace ock

#endif // SRC_SMEM_CSRC_CONFIG_STORE_BACKEND_SMEM_EXTERNAL_BACKEND_REGISTRY_H_
