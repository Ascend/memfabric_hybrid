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

#ifndef ZBAL_LOGGER_H
#define ZBAL_LOGGER_H

#include <ctime>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <atomic>
#include <mutex>
#include <unistd.h>
#include <sstream>
#include <algorithm>
#include <sys/time.h>
#include <sys/syscall.h>

#include "zbal_defines.h"

namespace zbal {
using ExternalLog = void (*)(int, const char *);

template<typename T>
void LogRecursive(std::ostream &os, T &&arg)
{
    os << std::forward<T>(arg);
}

template<typename... Args>
void LogAll(std::ostream &os, Args &&...args)
{
    (os << ... << std::forward<Args>(args));
}

enum LogLevel : int {
    DEBUG_LEVEL = 0,
    INFO_LEVEL,
    WARN_LEVEL,
    ERROR_LEVEL,
    FATAL_LEVEL,
    BUTT_LEVEL // no use
};

class OutLogger {
public:
    static OutLogger &Instance()
    {
        static OutLogger gLogger;
        return gLogger;
    }

    static bool ValidateLevel(int level)
    {
        return level >= DEBUG_LEVEL && level < BUTT_LEVEL;
    }

    static void DefaultLog(int level, const char *logMsg)
    {
        zbal::OutLogger::Instance().Log(level, logMsg);
    }

    void SetLogLevel(LogLevel level)
    {
        logLevel_ = level;
    }

    ALWAYS_INLINE const LogLevel &GetLogLevel() const
    {
        return logLevel_;
    }

    void SetExternalLogFunction(ExternalLog func, bool forceUpdate = false)
    {
        if (logFunc_ == nullptr || forceUpdate) {
            logFunc_ = func;
        }
    }

    ExternalLog GetExternalLogFunction() const
    {
        return logFunc_;
    }

    ALWAYS_INLINE void Log(int level, const std::string &logMsg)
    {
        if (logFunc_ != nullptr) {
            logFunc_(level, logMsg.c_str());
            return;
        }

        struct timeval tv{};
        char strTime[24];

        gettimeofday(&tv, nullptr);
        time_t timeStamp = tv.tv_sec;
        struct tm localTime{};
        auto result = localtime_r(&timeStamp, &localTime);
        if (result == nullptr) {
            return;
        }
        if (strftime(strTime, sizeof strTime, "%Y-%m-%d %H:%M:%S.", result) != 0) {
            const uint8_t timeWidth = 6U;
            std::cout << strTime << std::setw(timeWidth) << std::setfill('0') << tv.tv_usec << " "
                      << LogLevelDesc(level) << " " << getpid() << ":" << syscall(SYS_gettid) << " " << logMsg
                      << std::endl;
        } else {
            std::cout << " Invalid time " << LogLevelDesc(level) << " " << getpid() << ":" << syscall(SYS_gettid) << " "
                      << logMsg << std::endl;
        }
        // LCOV_EXCL_STOP
    }

    OutLogger(const OutLogger &) = delete;
    OutLogger(OutLogger &&) = delete;
    OutLogger &operator=(const OutLogger &) = delete;
    OutLogger &operator=(OutLogger &&) = delete;

    ~OutLogger()
    {
        logFunc_ = nullptr;
    }

private:
    OutLogger() = default;

    const char *LogLevelDesc(const int level) const
    {
        const static std::string invalid = "invalid";
        if (UNLIKELY(level < DEBUG_LEVEL || level >= BUTT_LEVEL)) {
            return invalid.c_str();
        }
        return logLevelDesc_[level];
    }

private:
    LogLevel logLevel_ = ERROR_LEVEL;
    ExternalLog logFunc_ = nullptr;

    const char *logLevelDesc_[BUTT_LEVEL] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
};
} // namespace zbal

// macro for log
#ifndef UT_ENABLED
#define ZBAL_LOG_FILENAME_SHORT (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#else
#define ZBAL_LOG_FILENAME_SHORT (__FILE__)
#endif

#define ZBAL_LOG_FORMAT ZBAL_LOG_FILENAME_SHORT << ":" << __LINE__ << " " << __FUNCTION__ << "] "
#define ZBAL_OUT_LOG(TAG, LEVEL, ARGS)                                             \
    do {                                                                           \
        if (static_cast<int>(LEVEL) < zbal::OutLogger::Instance().GetLogLevel()) { \
            break;                                                                 \
        }                                                                          \
        std::ostringstream oss;                                                    \
        oss << (TAG) << ZBAL_LOG_FORMAT << ARGS;                                   \
        zbal::OutLogger::Instance().Log(static_cast<int>(LEVEL), oss.str());       \
    } while (0)

#define ZBAL_LOG_DEBUG(ARGS)      ZBAL_OUT_LOG("[ZBAL ", zbal::DEBUG_LEVEL, ARGS)
#define ZBAL_LOG_INFO(ARGS)       ZBAL_OUT_LOG("[ZBAL ", zbal::INFO_LEVEL, ARGS)
#define ZBAL_LOG_WARN(ARGS)       ZBAL_OUT_LOG("[ZBAL ", zbal::WARN_LEVEL, ARGS)
#define ZBAL_LOG_WARN_LIMIT(ARGS) ZBAL_OUT_LOG_LIMIT("[ZBAL ", zbal::WARN_LEVEL, ARGS)
#define ZBAL_LOG_ERROR(ARGS)      ZBAL_OUT_LOG("[ZBAL ", zbal::ERROR_LEVEL, ARGS)

#define ZBAL_ASSERT_RETURN(ARGS, RET)            \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            ZBAL_LOG_ERROR("Assert " << #ARGS);  \
            return RET;                          \
        }                                        \
    } while (0)

#define ZBAL_LOG_AND_SET_LAST_ERROR(msg)      \
    do {                                      \
        std::stringstream tmpStr;             \
        tmpStr << msg;                        \
        zbal::ZBLastError::Set(tmpStr.str()); \
        ZBAL_LOG_ERROR(tmpStr.str());         \
    } while (0)

#define ZBAL_LOG_INFO_AND_SET_LAST_ERROR(msg) \
    do {                                      \
        std::stringstream tmpStr;             \
        tmpStr << msg;                        \
        zbal::ZBLastError::Set(tmpStr.str()); \
        ZBAL_LOG_INFO(tmpStr.str());          \
    } while (0)

#define ZBAL_VALIDATE_RETURN(ARGS, msg, RET)     \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            ZBAL_LOG_AND_SET_LAST_ERROR(msg);    \
            return RET;                          \
        }                                        \
    } while (0)

#define ZBAL_ASSERT_RET_VOID(ARGS)               \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            ZBAL_LOG_ERROR("Assert " << #ARGS);  \
            return;                              \
        }                                        \
    } while (0)

#define ZBAL_ASSERT_RETURN_NOLOG(ARGS, RET)      \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            return RET;                          \
        }                                        \
    } while (0)

#define ZBAL_ASSERT(ARGS)                        \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            ZBAL_LOG_ERROR("Assert " << #ARGS);  \
        }                                        \
    } while (0)

#define ZBAL_CHECK_S(condition, ...)                               \
    do {                                                           \
        if (!(condition)) {                                        \
            std::ostringstream oss;                                \
            oss << "[ZBAL_" << __FILE__ << ":" << __LINE__ << "] " \
                << "Check failed: " #condition ". ";               \
            zbal::LogAll(oss, __VA_ARGS__);                        \
            oss << std::endl;                                      \
            throw std::runtime_error(oss.str());                   \
        }                                                          \
    } while (0)

#define ZBAL_ASSERT_S(condition, ...)                              \
    do {                                                           \
        if (!(condition)) {                                        \
            std::ostringstream oss;                                \
            oss << "[ZBAL_" << __FILE__ << ":" << __LINE__ << "] " \
                << "Assertion failed: (" #condition ") ";          \
            zbal::LogAll(oss, __VA_ARGS__);                        \
            oss << std::endl;                                      \
            throw std::runtime_error(oss.str());                   \
        }                                                          \
    } while (0)

#endif // ZBAL_LOGGER_H
