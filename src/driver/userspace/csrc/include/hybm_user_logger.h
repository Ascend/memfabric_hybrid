/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_USER_LOGGER_H
#define MEMFABRIC_HYBRID_USER_LOGGER_H

#include "mf_out_logger.h"

#ifndef UNLIKELY
#define UNLIKELY(x) (__builtin_expect(!!(x), 0) != 0)
#endif

namespace ock {
namespace mf {

#define BM_USER_LOG_DEBUG(ARGS) MF_OUT_LOG("[USER ", ock::mf::DEBUG_LEVEL, ARGS)
#define BM_USER_LOG_INFO(ARGS) MF_OUT_LOG("[USER ", ock::mf::INFO_LEVEL, ARGS)
#define BM_USER_LOG_WARN(ARGS) MF_OUT_LOG("[USER ", ock::mf::WARN_LEVEL, ARGS)
#define BM_USER_LOG_ERROR(ARGS) MF_OUT_LOG("[USER ", ock::mf::ERROR_LEVEL, ARGS)

#define BM_USER_ASSERT_RETURN(ARGS, RET)                \
    do {                                           \
        if (__builtin_expect(!(ARGS), 0) != 0) {   \
            BM_USER_LOG_ERROR("Assert " << #ARGS); \
            return RET;                            \
        }                                          \
    } while (0)

} // namespace mf
} // namespace ock

#endif // MEMFABRIC_HYBRID_USER_LOGGER_H
