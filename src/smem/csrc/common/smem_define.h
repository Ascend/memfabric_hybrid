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
#ifndef MEM_FABRIC_HYBRID_SMEM_DEFINE_H
#define MEM_FABRIC_HYBRID_SMEM_DEFINE_H

namespace ock {
namespace smem {
// macro for gcc optimization for prediction of if/else
#ifndef LIKELY
#define LIKELY(x) (__builtin_expect(!!(x), 1) != 0)
#endif

#ifndef UNLIKELY
#define UNLIKELY(x) (__builtin_expect(!!(x), 0) != 0)
#endif

#define SM_SET_LAST_ERROR(msg)                     \
    do {                                           \
        std::stringstream tmpStr;                  \
        tmpStr << (msg);                           \
        ock::smem::SmLastError::Set(tmpStr.str()); \
    } while (0)

#define SM_COUT_AND_SET_LAST_ERROR(msg)            \
    do {                                           \
        std::stringstream tmpStr;                  \
        tmpStr << (msg);                           \
        ock::smem::SmLastError::Set(tmpStr.str()); \
        std::cout << (msg) << std::endl;           \
    } while (0)

// if expression is true, print error
#define SM_PARAM_VALIDATE(expression, msg, returnValue) \
    do {                                                \
        if ((expression)) {                             \
            SM_SET_LAST_ERROR(msg);                     \
            SM_LOG_ERROR(msg);                          \
            return returnValue;                         \
        }                                               \
    } while (0)

#define SMEM_API __attribute__((visibility("default")))

} // namespace smem
} // namespace ock

#endif // MEM_FABRIC_HYBRID_SMEM_DEFINE_H
