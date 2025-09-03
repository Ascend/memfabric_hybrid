/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
*/

#ifndef MEMFABRIC_HYBRID_BASE_LOGGER_H
#define MEMFABRIC_HYBRID_BASE_LOGGER_H

#include <ctime>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <unistd.h>
#include <sstream>
#include <algorithm>
#include <sys/time.h>
#include <sys/syscall.h>

// macro for gcc optimization for prediction of if/else
#ifndef LIKELY
#define LIKELY(x) (__builtin_expect(!!(x), 1) != 0)
#endif

#ifndef UNLIKELY
#define UNLIKELY(x) (__builtin_expect(!!(x), 0) != 0)
#endif

namespace ock {
namespace mf {
using ExternalLog = void (*)(int, const char *);

enum LogLevel : int {
    DEBUG_LEVEL = 0,
    INFO_LEVEL,
    WARN_LEVEL,
    ERROR_LEVEL,
    FATAL_LEVEL,
    BUTT_LEVEL  // no use
};

class OutLogger {
public:
    static OutLogger &Instance()
    {
        static OutLogger gLogger;
        return gLogger;
    }

    inline LogLevel GetLogLevel() const
    {
        return logLevel_;
    }

    inline ExternalLog GetLogExtraFunc() const
    {
        return logFunc_;
    }

    inline void SetLogLevel(LogLevel level)
    {
        logLevel_ = level;
    }

    inline void SetExternalLogFunction(ExternalLog func, bool forceUpdate = false)
    {
        if (logFunc_ == nullptr || forceUpdate) {
            logFunc_ = func;
        }
    }

    static bool ValidateLevel(int level)
    {
        return level >= DEBUG_LEVEL && level < BUTT_LEVEL;
    }

    inline void Log(int level, const std::ostringstream &oss)
    {
        // LCOV_EXCL_START
        std::string logMsg = oss.str();
        logMsg.erase(std::remove_if(logMsg.begin(), logMsg.end(), [](char c) { return c == '\r' || c == '\n'; }),
                     logMsg.end());
        if (logFunc_ != nullptr) {
            logFunc_(level, logMsg.c_str());
            return;
        }

        struct timeval tv {};
        char strTime[24];

        gettimeofday(&tv, nullptr);
        time_t timeStamp = tv.tv_sec;
        struct tm localTime {};
        auto result = localtime_r(&timeStamp, &localTime);
        if (result == nullptr) {
            return;
        }
        if (strftime(strTime, sizeof strTime, "%Y-%m-%d %H:%M:%S.", result) != 0) {
            std::cout << strTime << std::setw(6U) << std::setfill('0') << tv.tv_usec << " "
                      <<LogLevelDesc(level) << " " << syscall(SYS_gettid) << " " << logMsg << std::endl;
        } else {
            std::cout << " Invalid time " << LogLevelDesc(level) << " " << syscall(SYS_gettid) << " " << logMsg
                      << std::endl;
        }
        // LCOV_EXCL_STOP
    }

    OutLogger(const OutLogger &)            = delete;
    OutLogger(OutLogger &&)                 = delete;
    OutLogger &operator=(const OutLogger &) = delete;
    OutLogger &operator=(OutLogger &&)      = delete;

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
    LogLevel logLevel_   = ERROR_LEVEL;
    ExternalLog logFunc_ = nullptr;

    const char *logLevelDesc_[BUTT_LEVEL] = {"debug", "info", "warn", "error", "fatal"};
};
}  // namespace mf
}  // namespace ock

// macro for log
#ifndef MF_LOG_FILENAME_SHORT
#ifndef UT_ENABLED
#define MF_LOG_FILENAME_SHORT (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#else
#define MF_LOG_FILENAME_SHORT (__FILE__)
#endif
#endif
#define MF_OUT_LOG(TAG, LEVEL, ARGS)                                                  \
    do {                                                                              \
        if (static_cast<int>(LEVEL) < ock::mf::OutLogger::Instance().GetLogLevel()) { \
            break;                                                                    \
        }                                                                             \
        std::ostringstream oss;                                                       \
        oss << (TAG) << MF_LOG_FILENAME_SHORT << ":" << __LINE__ << "] " << ARGS;     \
        ock::mf::OutLogger::Instance().Log(static_cast<int>(LEVEL), oss);             \
    } while (0)

#endif  // MEMFABRIC_HYBRID_LOGGER_H
