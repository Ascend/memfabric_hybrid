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
#ifndef ACC_LINKS_ACC_OUT_LOGGER_H
#define ACC_LINKS_ACC_OUT_LOGGER_H

#include <ctime>
#include <iomanip>
#include <cstring>
#include <iostream>
#include <mutex>
#include <unistd.h>
#include <sstream>
#include <sys/time.h>
#include <sys/syscall.h>

#include "mf_out_logger.h"

#define LOG_DEBUG(ARGS) MF_OUT_LOG("[AccLink ", ock::mf::DEBUG_LEVEL, ARGS)
#define LOG_INFO(ARGS) MF_OUT_LOG("[AccLink ", ock::mf::INFO_LEVEL, ARGS)
#define LOG_WARN(ARGS) MF_OUT_LOG("[AccLink ", ock::mf::WARN_LEVEL, ARGS)
#define LOG_ERROR(ARGS) MF_OUT_LOG("[AccLink ", ock::mf::ERROR_LEVEL, ARGS)

#ifndef ENABLE_TRACE_LOG
#define LOG_TRACE(x)
#elif
#define LOG_TRACE(x) ACC_LINKS_OUT_LOG(DEBUG_LEVEL, AccLinksTrace, ARGS)
#endif

#define ASSERT_RETURN(ARGS, RET)           \
    do {                                   \
        if (UNLIKELY(!(ARGS))) {           \
            LOG_ERROR("Assert " << #ARGS); \
            return RET;                    \
        }                                  \
    } while (0)

#define ASSERT_RET_VOID(ARGS)              \
    do {                                   \
        if (UNLIKELY(!(ARGS))) {           \
            LOG_ERROR("Assert " << #ARGS); \
            return;                        \
        }                                  \
    } while (0)

#define ASSERT(ARGS)                       \
    do {                                   \
        if (UNLIKELY(!(ARGS))) {           \
            LOG_ERROR("Assert " << #ARGS); \
        }                                  \
    } while (0)

#define VALIDATE_RETURN(ARGS, msg, RET)          \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            LOG_ERROR(msg);                      \
            return RET;                          \
        }                                        \
    } while (0)

#define VALIDATE_RETURN_VOID(ARGS, msg)          \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            LOG_ERROR(msg);                      \
            return;                              \
        }                                        \
    } while (0)

#define LOG_ERROR_RETURN_IT_IF_NOT_OK(result, msg) \
    do {                                           \
        auto innerResult = (result);               \
        if (UNLIKELY(innerResult != ACC_OK)) {     \
            LOG_ERROR(msg);                        \
            return innerResult;                    \
        }                                          \
    } while (0)

#endif  // ACC_LINKS_ACC_OUT_LOGGER_H