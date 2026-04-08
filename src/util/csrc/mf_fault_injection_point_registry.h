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

#ifndef MEMFABRIC_HYBRID_SRC_UTIL_CSRC_MF_FAULT_INJECTION_POINT_REGISTRY_H_
#define MEMFABRIC_HYBRID_SRC_UTIL_CSRC_MF_FAULT_INJECTION_POINT_REGISTRY_H_

#include "mf_fault_injection_point.h"

namespace ock {
namespace mf {

class FaultInjectionPointRegistry {
public:
    static FaultInjectionPointStatus Register();
    static FaultInjectionPointStatus Unregister();
};

} // namespace mf
} // namespace ock

#endif // MEMFABRIC_HYBRID_SRC_UTIL_CSRC_MF_FAULT_INJECTION_POINT_REGISTRY_H_
