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
#ifndef MEM_FABRIC_HYBRID_HYBM_TYPES_H
#define MEM_FABRIC_HYBRID_HYBM_TYPES_H

#include <cstdint>
#include <memory>

namespace ock {
namespace mf {
using Result = int32_t;

enum BErrorCode : int32_t {
    BM_OK = 0,
    BM_ERROR = -1,
    BM_INVALID_PARAM = -2,
    BM_MALLOC_FAILED = -3,
    BM_NEW_OBJECT_FAILED = -4,
    BM_FILE_NOT_ACCESS = -5,
    BM_DL_FUNCTION_FAILED = -6,
    BM_TIMEOUT = -7,
    BM_UNDER_API_UNLOAD = -8,
    BM_NOT_INITIALIZED = -9,
    BM_NOT_SUPPORT_FUNC = -10,
    BM_NOT_SUPPORTED = -100,
};

constexpr uint32_t UN40 = 40;
} // namespace mf
} // namespace ock

#endif // MEM_FABRIC_HYBRID_HYBM_TYPES_H
