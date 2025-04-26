/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
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

#define SM_LOG_AND_SET_LAST_ERROR(msg)  \
    do {                                \
        std::stringstream tmpStr;       \
        tmpStr << msg;                  \
        SmLastError::Set(tmpStr.str()); \
        SM_LOG_ERROR(tmpStr.str());     \
    } while (0)

#define SM_SET_LAST_ERROR(msg)          \
    do {                                \
        std::stringstream tmpStr;       \
        tmpStr << msg;                  \
        SmLastError::Set(tmpStr.str()); \
    } while (0)

#define SM_COUT_AND_SET_LAST_ERROR(msg) \
    do {                                \
        std::stringstream tmpStr;       \
        tmpStr << msg;                  \
        SmLastError::Set(tmpStr.str()); \
        std::cout << msg << std::endl;  \
    } while (0)

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

#endif  //MEM_FABRIC_HYBRID_SMEM_DEFINE_H
