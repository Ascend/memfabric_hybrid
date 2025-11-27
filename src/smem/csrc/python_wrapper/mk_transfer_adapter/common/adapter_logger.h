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
#ifndef MEMFABRIC_HYBRID_ADAPTER_LOGGER_H
#define MEMFABRIC_HYBRID_ADAPTER_LOGGER_H

#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>
#include "mf_out_logger.h"

#define ADAPTER_LOG_DEBUG(ARGS) MF_OUT_LOG("[ADAPTER ", ock::mf::DEBUG_LEVEL, ARGS)
#define ADAPTER_LOG_INFO(ARGS) MF_OUT_LOG("[ADAPTER ", ock::mf::INFO_LEVEL, ARGS)
#define ADAPTER_LOG_WARN(ARGS) MF_OUT_LOG("[ADAPTER ", ock::mf::WARN_LEVEL, ARGS)
#define ADAPTER_LOG_ERROR(ARGS) MF_OUT_LOG("[ADAPTER ", ock::mf::ERROR_LEVEL, ARGS)

// if ARGS is false, print error
#define ADAPTER_ASSERT_RETURN(ARGS, RET)         \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            ADAPTER_LOG_ERROR("Assert " << #ARGS);    \
            return RET;                          \
        }                                        \
    } while (0)

#define ADAPTER_ASSERT_RET_VOID(ARGS)            \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            ADAPTER_LOG_ERROR("Assert " << #ARGS);    \
            return;                              \
        }                                        \
    } while (0)

#define ADAPTER_ASSERT_RETURN_NOLOG(ARGS, RET)        \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            return RET;                          \
        }                                        \
    } while (0)

#define ADAPTER_ASSERT(ARGS)                          \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            ADAPTER_LOG_ERROR("Assert " << #ARGS);    \
        }                                        \
    } while (0)

#define ADAPTER_LOG_ERROR_RETURN_IT_IF_NOT_OK(result, msg) \
    do {                                              \
        auto innerResult = (result);                  \
        if (UNLIKELY(innerResult != 0)) {             \
            ADAPTER_LOG_ERROR(msg);                        \
            return innerResult;                       \
        }                                             \
    } while (0)

#define ADAPTER_RETURN_IT_IF_NOT_OK(result)    \
    do {                                  \
        auto innerResult = (result);      \
        if (UNLIKELY(innerResult != 0)) { \
            return innerResult;           \
        }                                 \
    } while (0)
#endif  // MEMFABRIC_HYBRID_ADAPTER_LOGGER_H