/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_MMC_DEFINE_H
#define MEM_FABRIC_HYBRID_MMC_DEFINE_H

using HRESULT = uint32_t;
#define OK (HRESULT(0x00000000L))
#define MMC_FAIL (HRESULT(0x00000001L))
#define MMC_DATA_TTL_MS 2000
#define MMC_THRESHOLD_PRINT_SECONDS 30

namespace ock {
namespace mmc {
// macro for gcc optimization for prediction of if/else
#ifndef LIKELY
#define LIKELY(x) (__builtin_expect(!!(x), 1) != 0)
#endif

#ifndef UNLIKELY
#define UNLIKELY(x) (__builtin_expect(!!(x), 0) != 0)
#endif

/**
 * @brief Set last error message and log it
 *
 * @param msg              [in] message to be set and log
 */
#define MMC_LOG_AND_SET_LAST_ERROR(msg)  \
    do {                                 \
        std::stringstream tmpStr;        \
        tmpStr << msg;                   \
        MmcLastError::Set(tmpStr.str()); \
        MMC_LOG_ERROR(tmpStr.str());     \
    } while (0)

/**
 * @brief Set last error message
 *
 * @param msg              [in] message to be set
 */
#define MMC_SET_LAST_ERROR(msg)          \
    do {                                 \
        std::stringstream tmpStr;        \
        tmpStr << msg;                   \
        MmcLastError::Set(tmpStr.str()); \
    } while (0)

/**
 * @brief Set last error message and print to stdout
 *
 * @param msg              [in] message to be set and printed
 */
#define MMC_COUT_AND_SET_LAST_ERROR(msg) \
    do {                                 \
        std::stringstream tmpStr;        \
        tmpStr << msg;                   \
        MmcLastError::Set(tmpStr.str()); \
        std::cout << msg << std::endl;   \
    } while (0)

/**
* @brief Validate expression, if expression is false then
 * 1 set last error message
 * 2 log an error message
 * 3 return the specified value
 *
 * @param expression       [in] expression to be validated
 * @param msg              [in] message to set and log
 * @param returnValue      [in] return value
 */
#define MMC_VALIDATE_RETURN(expression, msg, returnValue) \
    do {                                                  \
        if (UNLIKELY(!(expression))) {                    \
            MMC_SET_LAST_ERROR(msg);                      \
            MMC_LOG_ERROR(msg);                           \
            return returnValue;                           \
        }                                                 \
    } while (0)

/**
 * @brief Validate expression, if expression is false then
 * 1 set last error message
 * 2 log an error message
 * 3 return
 *
 * @param expression       [in] expression to be validated
 * @param msg              [in] message to set and log
 */
#define MMC_VALIDATE_RETURN_VOID(expression, msg) \
    do {                                          \
        if (UNLIKELY(!(expression))) {            \
            MMC_SET_LAST_ERROR(msg);              \
            MMC_LOG_ERROR(msg);                   \
            return;                               \
        }                                         \
    } while (0)

#define MMC_API __attribute__((visibility("default")))

inline bool RESULT_OK(const HRESULT hr)
{
    return ((hr) | OK) == OK;
}

inline bool RESULT_FAIL(const HRESULT hr)
{
    return (static_cast<HRESULT>(hr) & MMC_FAIL) == MMC_FAIL;
}

/**
 * @brief Delete a ptr safely, i.e. delete and set to nullptr
 *
 * @param p                [in] ptr to be deleted
 */
#define SAFE_DELETE(p) \
    do {               \
        delete (p);    \
        p = nullptr;   \
    } while (0)

}  // namespace mmc
}  // namespace ock

#endif  //MEM_FABRIC_HYBRID_MMC_DEFINE_H
