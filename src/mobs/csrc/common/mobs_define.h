/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_MOBS_DEFINE_H
#define MEM_FABRIC_HYBRID_MOBS_DEFINE_H

namespace ock {
namespace mobs {
// macro for gcc optimization for prediction of if/else
#ifndef LIKELY
#define LIKELY(x) (__builtin_expect(!!(x), 1) != 0)
#endif

#ifndef UNLIKELY
#define UNLIKELY(x) (__builtin_expect(!!(x), 0) != 0)
#endif

#define MO_LOG_AND_SET_LAST_ERROR(msg)  \
    do {                                \
        std::stringstream tmpStr;       \
        tmpStr << msg;                  \
        MOLastError::Set(tmpStr.str()); \
        MO_LOG_ERROR(tmpStr.str());     \
    } while (0)

#define MO_SET_LAST_ERROR(msg)          \
    do {                                \
        std::stringstream tmpStr;       \
        tmpStr << msg;                  \
        MOLastError::Set(tmpStr.str()); \
    } while (0)

#define MO_COUT_AND_SET_LAST_ERROR(msg) \
    do {                                \
        std::stringstream tmpStr;       \
        tmpStr << msg;                  \
        MOLastError::Set(tmpStr.str()); \
        std::cout << msg << std::endl;  \
    } while (0)

// if expression is false, print error
#define MO_PARAM_VALIDATE(expression, msg, returnValue) \
    do {                                                \
        if (!(expression)) {                             \
            MO_SET_LAST_ERROR(msg);                     \
            MO_LOG_ERROR(msg);                          \
            return returnValue;                         \
        }                                               \
    } while (0)

#define MOBS_API __attribute__((visibility("default")))

}  // namespace smem
}  // namespace ock

#endif  //MEM_FABRIC_HYBRID_MOBS_DEFINE_H
