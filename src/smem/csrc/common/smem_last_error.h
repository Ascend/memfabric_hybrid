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
} // namespace smem
} // namespace ock

#endif // MEMFABRIC_HYBRID_SMEM_LAST_ERROR_H