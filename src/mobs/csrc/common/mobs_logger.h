/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_MOBS_LOGGER_H
#define MEMFABRIC_HYBRID_MOBS_LOGGER_H

#include <ctime>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <unistd.h>
#include <sstream>
#include <sys/time.h>
#include <sys/syscall.h>

#include "mobs_define.h"

namespace ock {
namespace mobs {
using ExternalLog = void (*)(int, const char *);

enum LogLevel : int {
    DEBUG_LEVEL = 0,
    INFO_LEVEL,
    WARN_LEVEL,
    ERROR_LEVEL,
    BUTT_LEVEL  // no use
};

class MOOutLogger {
public:
    static MOOutLogger &Instance()
    {
        static MOOutLogger gLogger;
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

    inline void Log(int level, const std::ostringstream &oss)
    {
        if (logFunc_ != nullptr) {
            logFunc_(level, oss.str().c_str());
            return;
        }

        if (level < logLevel_) {
            return;
        }

        struct timeval tv {};
        char strTime[24];

        gettimeofday(&tv, nullptr);
        time_t timeStamp = tv.tv_sec;
        struct tm localTime {};
        if (strftime(strTime, sizeof strTime, "%Y-%m-%d %H:%M:%S.", localtime_r(&timeStamp, &localTime)) != 0) {
            std::cout << strTime << std::setw(6) << std::setfill('0') << tv.tv_usec << " " << LogLevelDesc(level) << " "
                      << syscall(SYS_gettid) << " " << oss.str() << std::endl;
        } else {
            std::cout << " Invalid time " << LogLevelDesc(level) << " " << syscall(SYS_gettid) << " " << oss.str()
                      << std::endl;
        }
    }

    MOOutLogger(const MOOutLogger &) = delete;
    MOOutLogger(MOOutLogger &&) = delete;

    ~MOOutLogger()
    {
        logFunc_ = nullptr;
    }

private:
    MOOutLogger() = default;

    const char *LogLevelDesc(const int level) const
    {
        const static std::string invalid = "invalid";
        if (UNLIKELY(level < DEBUG_LEVEL || level >= BUTT_LEVEL)) {
            return invalid.c_str();
        }
        return logLevelDesc_[level];
    }

private:
    LogLevel logLevel_ = INFO_LEVEL;
    ExternalLog logFunc_ = nullptr;

    const char *logLevelDesc_[BUTT_LEVEL] = {"debug", "info", "warn", "error"};
};
}  // namespace smem
}  // namespace ock

// macro for log
#ifndef MOBS_LOG_FILENAME_SHORT
#define MOBS_LOG_FILENAME_SHORT (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif
#define MO_OUT_LOG(LEVEL, ARGS)                                                        \
    do {                                                                               \
        std::ostringstream oss;                                                        \
        oss << "[SMEM " << MOBS_LOG_FILENAME_SHORT << ":" << __LINE__ << "] " << ARGS; \
        ock::mobs::MOOutLogger::Instance().Log(LEVEL, oss);                            \
    } while (0)

#define MO_LOG_DEBUG(ARGS) MO_OUT_LOG(ock::mobs::DEBUG_LEVEL, ARGS)
#define MO_LOG_INFO(ARGS) MO_OUT_LOG(ock::mobs::INFO_LEVEL, ARGS)
#define MO_LOG_WARN(ARGS) MO_OUT_LOG(ock::mobs::WARN_LEVEL, ARGS)
#define MO_LOG_ERROR(ARGS) MO_OUT_LOG(ock::mobs::ERROR_LEVEL, ARGS)

// if ARGS is false, print error
#define MO_ASSERT_RETURN(ARGS, RET)              \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            MO_LOG_ERROR("Assert " << #ARGS);    \
            return RET;                          \
        }                                        \
    } while (0)

#define MO_ASSERT_RET_VOID(ARGS)                 \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            MO_LOG_ERROR("Assert " << #ARGS);    \
            return;                              \
        }                                        \
    } while (0)

#define MO_ASSERT_RETURN_NOLOG(ARGS, RET)        \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            return RET;                          \
        }                                        \
    } while (0)

#define MO_ASSERT(ARGS)                          \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            MO_LOG_ERROR("Assert " << #ARGS);    \
        }                                        \
    } while (0)

#define MO_LOG_ERROR_RETURN_IT_IF_NOT_OK(result, msg) \
    do {                                              \
        auto innerResult = (result);                  \
        if (UNLIKELY(innerResult != 0)) {             \
            MO_LOG_ERROR(msg);                        \
            return innerResult;                       \
        }                                             \
    } while (0)

#define MO_RETURN_IT_IF_NOT_OK(result)    \
    do {                                  \
        auto innerResult = (result);      \
        if (UNLIKELY(innerResult != 0)) { \
            return innerResult;           \
        }                                 \
    } while (0)

#endif  // MEMFABRIC_HYBRID_MOBS_LOGGER_H
