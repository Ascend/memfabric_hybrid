/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Create Date : 2025
 */
#ifndef MEMFABRIC_HYBRID_ADAPTER_LOGGER_H
#define MEMFABRIC_HYBRID_ADAPTER_LOGGER_H

#include <ctime>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <unistd.h>
#include <sstream>
#include <sys/time.h>
#include <sys/syscall.h>

namespace ock {
namespace adapter {
using ExternalLog = void (*)(int, const char *);

#ifndef UNLIKELY
#define UNLIKELY(x) (__builtin_expect(!!(x), 0) != 0)
#endif

enum LogLevel : int {
    DEBUG_LEVEL = 0,
    INFO_LEVEL,
    WARN_LEVEL,
    ERROR_LEVEL,
    BUTT_LEVEL  // no use
};

class AdapterOutLogger {
public:
    static AdapterOutLogger &Instance()
    {
        static AdapterOutLogger gLogger;
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

    AdapterOutLogger(const AdapterOutLogger &) = delete;
    AdapterOutLogger(AdapterOutLogger &&) = delete;

    ~AdapterOutLogger()
    {
        logFunc_ = nullptr;
    }

private:
    AdapterOutLogger() = default;

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
}  // namespace adapter
}  // namespace ock

// macro for log
#ifndef ADAPTER_LOG_FILENAME_SHORT
#define ADAPTER_LOG_FILENAME_SHORT (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif
#define ADAPTER_OUT_LOG(LEVEL, ARGS)                                                        \
    do {                                                                               \
        std::ostringstream oss;                                                        \
        oss << "[ADAPTER " << ADAPTER_LOG_FILENAME_SHORT << ":" << __LINE__ << "] " << ARGS; \
        ock::adapter::AdapterOutLogger::Instance().Log(LEVEL, oss);                            \
    } while (0)

#define ADAPTER_LOG_DEBUG(ARGS) ADAPTER_OUT_LOG(ock::adapter::DEBUG_LEVEL, ARGS)
#define ADAPTER_LOG_INFO(ARGS) ADAPTER_OUT_LOG(ock::adapter::INFO_LEVEL, ARGS)
#define ADAPTER_LOG_WARN(ARGS) ADAPTER_OUT_LOG(ock::adapter::WARN_LEVEL, ARGS)
#define ADAPTER_LOG_ERROR(ARGS) ADAPTER_OUT_LOG(ock::adapter::ERROR_LEVEL, ARGS)

// if ARGS is false, print error
#define ADAPTER_ASSERT_RETURN(ARGS, RET)         \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            ADAPTER_LOG_ERROR("Assert " << #ARGS);    \
            return RET;                          \
        }                                        \
    } while (0)

#define ADAPTER_ASSERT_RET_VOID(ARGS)            \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            ADAPTER_LOG_ERROR("Assert " << #ARGS);    \
            return;                              \
        }                                        \
    } while (0)

#define ADAPTER_ASSERT_RETURN_NOLOG(ARGS, RET)        \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            return RET;                          \
        }                                        \
    } while (0)

#define ADAPTER_ASSERT(ARGS)                          \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            ADAPTER_LOG_ERROR("Assert " << #ARGS);    \
        }                                        \
    } while (0)

#define ADAPTER_LOG_ERROR_RETURN_IT_IF_NOT_OK(result, msg) \
    do {                                              \
        auto innerResult = (result);                  \
        if (UNLIKELY(innerResult != 0)) {             \
            ADAPTER_LOG_ERROR(msg);                        \
            return innerResult;                       \
        }                                             \
    } while (0)

#define ADAPTER_RETURN_IT_IF_NOT_OK(result)    \
    do {                                  \
        auto innerResult = (result);      \
        if (UNLIKELY(innerResult != 0)) { \
            return innerResult;           \
        }                                 \
    } while (0)
#endif  // MEMFABRIC_HYBRID_ADAPTER_LOGGER_H