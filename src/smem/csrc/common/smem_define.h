/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef MEM_FABRIC_HYBRID_SMEM_DEFINE_H
#define MEM_FABRIC_HYBRID_SMEM_DEFINE_H

namespace ock {
namespace smem {

#define SM_LOG_AND_SET_LAST_ERROR(msg)  \
    do {                                \
        std::stringstream tmpStr;       \
        tmpStr << msg;                  \
        ock::smem::SmLastError::Set(tmpStr.str()); \
        SM_LOG_ERROR(tmpStr.str());     \
    } while (0)

#define SM_SET_LAST_ERROR(msg)          \
    do {                                \
        std::stringstream tmpStr;       \
        tmpStr << msg;                  \
        ock::smem::SmLastError::Set(tmpStr.str()); \
    } while (0)

#define SM_COUT_AND_SET_LAST_ERROR(msg) \
    do {                                \
        std::stringstream tmpStr;       \
        tmpStr << msg;                  \
        ock::smem::SmLastError::Set(tmpStr.str()); \
        std::cout << msg << std::endl;  \
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

}  // namespace smem
}  // namespace ock

#endif  // MEM_FABRIC_HYBRID_SMEM_DEFINE_H
