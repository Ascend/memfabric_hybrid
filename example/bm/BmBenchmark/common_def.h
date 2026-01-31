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
#ifndef COMMON_DEF_H
#define COMMON_DEF_H

#include <iostream>
#include <string>
#include <unistd.h>
#include <chrono>
#include <sstream>
#include <iomanip>

constexpr const char* BaseFileName(const char* path)
{
    const char* file = path;
    while (*path) {
        if (*path == '/' || *path == '\\') {
            file = path + 1;
        }
        ++path;
    }
    return file;
}

inline std::string NowTime()
{
    auto now = std::chrono::system_clock::now();
    auto sec = std::chrono::time_point_cast<std::chrono::seconds>(now);
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(now - sec).count();

    std::time_t tt = std::chrono::system_clock::to_time_t(sec);
    std::tm tm{};
    localtime_r(&tt, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setw(6) << std::setfill('0') << us;
    return oss.str();
}

#define LOG_PREINT(level, msg)          \
    do {                                \
        std::ostringstream oss;         \
        oss << NowTime() << " "         \
                 << getpid() << " "     \
                 << BaseFileName(__FILE__) << ":" << __LINE__ \
                 << " [" level "] "                           \
                 << msg;                                      \
        std::cout << oss.str() << std::endl; \
    } while (0)

#define LOG_INFO(msg)  LOG_PREINT("INFO", msg)
#define LOG_WARN(msg)  LOG_PREINT("WARN", msg)
#define LOG_ERROR(msg) LOG_PREINT("ERROR", msg)

#define CHECK_RET_ERR(x, msg) \
    do {                      \
        if ((x) != 0) {       \
            LOG_ERROR(msg);   \
            return -1;        \
        }                     \
    } while (0)

#define CHECK_RET_VOID(x, msg) \
    do {                       \
        if ((x) != 0) {        \
            LOG_ERROR(msg);    \
            return;            \
        }                      \
    } while (0)

const int32_t RANK_SIZE_MAX = 32;

#endif