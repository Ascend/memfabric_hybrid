/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef MEMFABRIC_HYBRID_SMEM_LAST_ERROR_H
#define MEMFABRIC_HYBRID_SMEM_LAST_ERROR_H

#include <string>

namespace ock {
namespace smem {
class SmLastError {
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
    static thread_local bool have_;
    static thread_local std::string msg_;
};

inline void SmLastError::Set(const std::string &msg)
{
    msg_ = msg;
    have_ = true;
}

inline void SmLastError::Set(const char *msg)
{
    msg_ = msg;
    have_ = true;
}

inline const char *SmLastError::GetAndClear(bool clear)
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
}  // namespace smem
}  // namespace ock

#endif  // MEMFABRIC_HYBRID_SMEM_LAST_ERROR_H
