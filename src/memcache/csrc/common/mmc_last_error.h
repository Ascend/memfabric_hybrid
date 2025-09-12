/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_MMC_LAST_ERROR_H
#define MEMFABRIC_HYBRID_MMC_LAST_ERROR_H

#include "mmc_common_includes.h"

namespace ock {
namespace mmc {
class MmcLastError {
public:
    /**
     * @brief Set last error string
     *
     * @param msg          [in] last error message
     */
    static void Set(const std::string &msg);

    /**
     * @brief Set last error string
     *
     * @param msg          [in] last error message
     */
    static void Set(const char *msg);

    /**
     * @brief Get and clear last error messaged
     *
     * @return err string if there is, and clear it
     */
    static const char *GetAndClear(bool clear);

private:
    static thread_local bool have_;       /* thread local flag that indicates if there is last error */
    static thread_local std::string msg_; /* last error message */
};

inline void MmcLastError::Set(const std::string &msg)
{
    msg_ = msg;
    have_ = true;
}

inline void MmcLastError::Set(const char *msg)
{
    msg_ = msg;
    have_ = true;
}

inline const char *MmcLastError::GetAndClear(bool clear)
{
    /* have last error, just set the flag to false */
    if (have_) {
        have_ = !clear;
        return msg_.c_str();
    }

    /* empty string */
    static std::string emptyString;

    return emptyString.c_str();
}
}  // namespace mmc
}  // namespace ock

#endif  //MEMFABRIC_HYBRID_MMC_LAST_ERROR_H
