/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef STORE_HYBRID_CONFIG_STORE_LOG_H
#define STORE_HYBRID_CONFIG_STORE_LOG_H

#include "mf_out_logger.h"

#define STORE_LOG_DEBUG(ARGS) MF_OUT_LOG("[SMEM ", ock::mf::DEBUG_LEVEL, ARGS)
#define STORE_LOG_INFO(ARGS) MF_OUT_LOG("[SMEM ", ock::mf::INFO_LEVEL, ARGS)
#define STORE_LOG_WARN(ARGS) MF_OUT_LOG("[SMEM ", ock::mf::WARN_LEVEL, ARGS)
#define STORE_LOG_ERROR(ARGS) MF_OUT_LOG("[SMEM ", ock::mf::ERROR_LEVEL, ARGS)
#define STORE_LOG_ERROR_LIMIT(ARGS) MF_OUT_LOG_LIMIT("[SMEM ", ock::mf::ERROR_LEVEL, ARGS)

// if ARGS is false, print error
#define STORE_ASSERT_RETURN(ARGS, RET)              \
    do {                                            \
        if (__builtin_expect(!(ARGS), 0) != 0) {    \
            STORE_LOG_ERROR("Assert " << #ARGS);    \
            return RET;                             \
        }                                           \
    } while (0)

#define STORE_VALIDATE_RETURN(ARGS, msg, RET)       \
    do {                                            \
        if (__builtin_expect(!(ARGS), 0) != 0) {    \
            STORE_LOG_ERROR(msg);                   \
            return RET;                             \
        }                                           \
    } while (0)

#endif // STORE_HYBRID_CONFIG_STORE_LOG_H