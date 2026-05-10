/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef ZBAL_LAST_ERROR_H
#define ZBAL_LAST_ERROR_H

#include <string>

#include "zbal_defines.h"

namespace zbal {
class ZBLastError {
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

ALWAYS_INLINE void ZBLastError::Set(const std::string &msg)
{
    msg_ = msg;
    have_ = true;
}

ALWAYS_INLINE void ZBLastError::Set(const char *msg)
{
    msg_ = msg;
    have_ = true;
}

ALWAYS_INLINE const char *ZBLastError::GetAndClear(bool clear)
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
} // namespace zbal

#endif // ZBAL_LAST_ERROR_H