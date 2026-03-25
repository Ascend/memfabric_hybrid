/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Embricks is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MEMFABRIC_HYBRID_EMB_DEFINES_H
#define MEMFABRIC_HYBRID_EMB_DEFINES_H

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <sstream>

namespace ock {
namespace emb {
using Result = int32_t;

enum EmResultErrorCode : Result {
    EM_OK = 0,
    EM_ERROR = -1,
    EM_INVALID_PARAM = -2,
    EM_NEW_OBJ_FAILED = -3,
    EM_DL_OPEN_LIB_FAILED = -4,
    EM_DL_LOAD_SYM_FAILED = -5,
    EM_FILE_NOT_FOUND = -6,
    EM_INVALID_VALUE = -7,
    EM_INVALID_PTR = -8,
    EM_ERROR_ALLOC = -9,
    EM_NOT_ENOUGH_MEM = -10,
};

#ifndef LIKELY
#define LIKELY(x) (__builtin_expect(!!(x), 1) != 0)
#endif

#ifndef UNLIKELY
#define UNLIKELY(x) (__builtin_expect(!!(x), 0) != 0)
#endif

#define EM_LIKELY(expr)   LIKELY(expr)
#define EM_UNLIKELY(expr) UNLIKELY(expr)

#define EM_API __attribute__((visibility("default")))

#define DL_LOAD_SYM(TARGET_FUNC_VAR, TARGET_FUNC_TYPE, FILE_HANDLE, SYMBOL_NAME)                      \
    do {                                                                                              \
        TARGET_FUNC_VAR = (TARGET_FUNC_TYPE)dlsym(FILE_HANDLE, SYMBOL_NAME);                          \
        if ((TARGET_FUNC_VAR) == nullptr) {                                                           \
            EM_LOG_ERROR("Failed to call dlsym to load " << (SYMBOL_NAME) << ", error" << dlerror()); \
            dlclose(FILE_HANDLE);                                                                     \
            FILE_HANDLE = nullptr;                                                                    \
            return EM_DL_LOAD_SYM_FAILED;                                                             \
        }                                                                                             \
    } while (0)

} // namespace zbccl
}
}

#endif // MEMFABRIC_HYBRID_EMB_DEFINES_H
