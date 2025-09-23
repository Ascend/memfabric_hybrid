/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MEMFABRIC_HYBRID_LOGGER_H
#define MEMFABRIC_HYBRID_LOGGER_H

#include <ctime>
#include <cstring>
#include <iostream>
#include <mutex>
#include <ostream>
#include <unistd.h>
#include <sstream>
#include <sys/time.h>
#include <sys/syscall.h>

#include "hybm_types.h"
#include "hybm_define.h"

#define BM_LOG_DEBUG(ARGS) MF_OUT_LOG("[BM ", ock::mf::DEBUG_LEVEL, ARGS)
#define BM_LOG_INFO(ARGS) MF_OUT_LOG("[BM ", ock::mf::INFO_LEVEL, ARGS)
#define BM_LOG_WARN(ARGS) MF_OUT_LOG("[BM ", ock::mf::WARN_LEVEL, ARGS)
#define BM_LOG_ERROR(ARGS) MF_OUT_LOG("[BM ", ock::mf::ERROR_LEVEL, ARGS)

#define BM_ASSERT_RETURN(ARGS, RET)              \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            BM_LOG_ERROR("Assert " << #ARGS);    \
            return RET;                          \
        }                                        \
    } while (0)
    
#define BM_VALIDATE_RETURN(ARGS, msg, RET)       \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            BM_LOG_ERROR(msg);                   \
            return RET;                          \
        }                                        \
    } while (0)

#define BM_ASSERT_RET_VOID(ARGS)                 \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            BM_LOG_ERROR("Assert " << #ARGS);    \
            return;                              \
        }                                        \
    } while (0)

#define BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(result, msg) \
    do {                                              \
        auto innerResult = (result);                  \
        if (UNLIKELY(innerResult != 0)) {             \
            BM_LOG_ERROR(msg);                        \
            return innerResult;                       \
        }                                             \
    } while (0)

#define BM_ASSERT(ARGS)                          \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            BM_LOG_ERROR("Assert " << #ARGS);    \
        }                                        \
    } while (0)

#endif // MEMFABRIC_HYBRID_LOGGER_H
