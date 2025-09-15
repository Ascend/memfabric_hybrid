/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef STORE_HYBRID_CONFIG_STORE_LOG_H
#define STORE_HYBRID_CONFIG_STORE_LOG_H


#include <ctime>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <unistd.h>
#include <sstream>
#include <sys/time.h>
#include <sys/syscall.h>

#ifndef LIKELY
#define LIKELY(x) (__builtin_expect(!!(x), 1) != 0)
#endif

#ifndef UNLIKELY
#define UNLIKELY(x) (__builtin_expect(!!(x), 0) != 0)
#endif

namespace ock {
namespace smem {
using ExternalLog = void (*)(int, const char *);

constexpr int MICROSECOND_WIDTH = 6;

enum LogLevel : int {
    DEBUG_LEVEL = 0,
    INFO_LEVEL,
    WARN_LEVEL,
    ERROR_LEVEL,
    BUTT_LEVEL  // no use
};

class StoreOutLogger {
public:
    static StoreOutLogger &Instance()
    {
        static StoreOutLogger gLogger;
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

        struct timeval tv{};
        char strTime[24];

        gettimeofday(&tv, nullptr);
        time_t timeStamp = tv.tv_sec;
        struct tm localTime{};
        if (strftime(strTime, sizeof strTime, "%Y-%m-%d %H:%M:%S.", localtime_r(&timeStamp, &localTime)) != 0) {
            std::cout << strTime << std::setw(MICROSECOND_WIDTH) << std::setfill('0') << tv.tv_usec << " "
                      << LogLevelDesc(level) << " " << syscall(SYS_gettid) << " " << oss.str() << std::endl;
        } else {
            std::cout << " Invalid time " << LogLevelDesc(level) << " " << syscall(SYS_gettid) << " " << oss.str()
                      << std::endl;
        }
    }

    StoreOutLogger(const StoreOutLogger &) = delete;
    StoreOutLogger& operator=(const StoreOutLogger&) = delete;
    StoreOutLogger(StoreOutLogger &&) = delete;

    ~StoreOutLogger()
    {
        logFunc_ = nullptr;
    }

private:
    StoreOutLogger() = default;

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

#ifndef STORE_LOG_FILENAME_SHORT
#define STORE_LOG_FILENAME_SHORT (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif
#define STORE_OUT_LOG(LEVEL, ARGS)                                                        \
    do {                                                                               \
        std::ostringstream oss;                                                        \
        oss << "[STORE " << STORE_LOG_FILENAME_SHORT << ":" << __LINE__ << "] " << ARGS;     \
        ock::smem::StoreOutLogger::Instance().Log(LEVEL, oss);                              \
    } while (0)

#define STORE_LOG_DEBUG(ARGS) STORE_OUT_LOG(ock::smem::DEBUG_LEVEL, ARGS)
#define STORE_LOG_INFO(ARGS) STORE_OUT_LOG(ock::smem::INFO_LEVEL, ARGS)
#define STORE_LOG_WARN(ARGS) STORE_OUT_LOG(ock::smem::WARN_LEVEL, ARGS)
#define STORE_LOG_ERROR(ARGS) STORE_OUT_LOG(ock::smem::ERROR_LEVEL, ARGS)

// if ARGS is false, print error
#define STORE_ASSERT_RETURN(ARGS, RET)              \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            STORE_LOG_ERROR("Assert " << #ARGS);    \
            return RET;                          \
        }                                        \
    } while (0)

#define STORE_PARAM_VALIDATE(expression, msg, returnValue) \
    do {                                                \
        if ((expression)) {                             \
            STORE_LOG_ERROR(msg);                          \
            return returnValue;                         \
        }                                               \
    } while (0)

#endif // STORE_HYBRID_CONFIG_STORE_LOG_H