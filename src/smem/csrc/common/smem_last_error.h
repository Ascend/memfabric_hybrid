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
#ifndef MEMFABRIC_HYBRID_SMEM_LAST_ERROR_H
#define MEMFABRIC_HYBRID_SMEM_LAST_ERROR_H

#include <cstdint>
#include <string>

namespace ock {
namespace smem {
class SmLastError {
public:
    static constexpr int32_t SM_DEFAULT_ERROR = -1;

    /**
     * @brief Set last error string (code defaults to SM_DEFAULT_ERROR)
     *
     * @param msg          [in] last error message
     */
    static void Set(const std::string &msg);

    /**
     * @brief Set last error string (code defaults to SM_DEFAULT_ERROR)
     *
     * @param msg          [in] last error message
     */
    static void Set(const char *msg);

    /**
     * @brief Set last error code and message
     *
     * @param code         [in] last error code
     * @param msg          [in] last error message
     */
    static void Set(int32_t code, const std::string &msg);

    /**
     * @brief Get and clear last error message
     *
     * @return err string if there is, and clear it
     */
    static const char *GetAndClear(bool clear);

    /**
     * @brief Get and clear last error code
     *
     * @param clear        [in] whether to clear the stored code
     * @return last error code, or 0 if none was set
     */
    static int32_t GetAndClearCode(bool clear);

private:
    static thread_local bool have_;
    static thread_local std::string msg_;
    static thread_local int32_t code_;
};

inline void SmLastError::Set(const std::string &msg)
{
    msg_ = msg;
    code_ = SM_DEFAULT_ERROR;
    have_ = true;
}

inline void SmLastError::Set(const char *msg)
{
    msg_ = msg;
    code_ = SM_DEFAULT_ERROR;
    have_ = true;
}

inline void SmLastError::Set(int32_t code, const std::string &msg)
{
    msg_ = msg;
    code_ = code;
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

inline int32_t SmLastError::GetAndClearCode(bool clear)
{
    auto code = code_;
    if (clear) {
        code_ = 0;
    }
    return code;
}
} // namespace smem
} // namespace ock

#endif // MEMFABRIC_HYBRID_SMEM_LAST_ERROR_H