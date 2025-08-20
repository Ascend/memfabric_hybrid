/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_SMEM_LOGGER_H
#define MEMFABRIC_HYBRID_SMEM_LOGGER_H

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

#define SM_LOG_DEBUG(ARGS) MF_OUT_LOG("[SMEM ", ock::mf::DEBUG_LEVEL, ARGS)
#define SM_LOG_INFO(ARGS) MF_OUT_LOG("[SMEM ", ock::mf::INFO_LEVEL, ARGS)
#define SM_LOG_WARN(ARGS) MF_OUT_LOG("[SMEM ", ock::mf::WARN_LEVEL, ARGS)
#define SM_LOG_ERROR(ARGS) MF_OUT_LOG("[SMEM ", ock::mf::ERROR_LEVEL, ARGS)

#define SM_CHECK_CONDITION_RET(condition, RET)   \
    do {                                         \
        if (condition) {                         \
            return RET;                          \
        }                                        \
    } while (0)
// if ARGS is false, print error
#define SM_ASSERT_RETURN(ARGS, RET)              \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            SM_LOG_ERROR("Assert " << #ARGS);    \
            return RET;                          \
        }                                        \
    } while (0)

#define SM_VALIDATE_RETURN(ARGS, msg, RET)       \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            SM_SET_LAST_ERROR(msg);              \
            SM_LOG_ERROR(msg);                   \
            return RET;                          \
        }                                        \
    } while (0)

#define SM_ASSERT_RET_VOID(ARGS)                 \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            SM_LOG_ERROR("Assert " << #ARGS);    \
            return;                              \
        }                                        \
    } while (0)

#define SM_ASSERT_RETURN_NOLOG(ARGS, RET)        \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            return RET;                          \
        }                                        \
    } while (0)

#define SM_ASSERT(ARGS)                          \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            SM_LOG_ERROR("Assert " << #ARGS);    \
        }                                        \
    } while (0)

#define SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(result, msg) \
    do {                                              \
        auto innerResult = (result);                  \
        if (UNLIKELY(innerResult != 0)) {             \
            SM_LOG_ERROR(msg);                        \
            return innerResult;                       \
        }                                             \
    } while (0)

#define SM_RETURN_IT_IF_NOT_OK(result)    \
    do {                                  \
        auto innerResult = (result);      \
        if (UNLIKELY(innerResult != 0)) { \
            return innerResult;           \
        }                                 \
    } while (0)
#endif  // MEMFABRIC_HYBRID_SMEM_LOGGER_H
