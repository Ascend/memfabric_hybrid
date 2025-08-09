/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_LOGGER_H
#define MEMFABRIC_HYBRID_LOGGER_H

#include <ctime>
#include <cstring>
#include <iostream>
#include <mutex>
#include <ostream>
#include <unistd.h>
#include <sstream>
#include <sys/time.h>
#include <sys/syscall.h>

#include "hybm_types.h"
#include "hybm_define.h"

namespace ock {
namespace mf {
using ExternalLog = void (*)(int level, const char *msg);

enum LogLevel : int {
    DEBUG_LEVEL = 0,
    INFO_LEVEL,
    WARN_LEVEL,
    ERROR_LEVEL,
    BUTT_LEVEL
};

class HyBMOutLogger {
public:
    static HyBMOutLogger *Instance()
    {
        static HyBMOutLogger *gLogger = nullptr;
        static std::mutex gMutex;

        if (__builtin_expect(gLogger == nullptr, 0) != 0) {
            std::lock_guard<std::mutex> lg(gMutex);
            if (gLogger == nullptr) {
                gLogger = new (std::nothrow) HyBMOutLogger();

                if (gLogger == nullptr) {
                    printf("Failed to new HyBMOutLogger, probably out of memory");
                }
            }
        }

        return gLogger;
    }

    inline void SetLogLevel(LogLevel level)
    {
        mLogLevel = level;
    }

    inline void SetExternalLogFunction(ExternalLog func, bool forceUpdate = false)
    {
        if (mLogFunc == nullptr || forceUpdate) {
            mLogFunc = func;
        }
    }

    inline void Log(int level, const std::string &oss)
    {
        if (mLogFunc != nullptr) {
            mLogFunc(level, oss.c_str());
            return;
        }

        if (level < mLogLevel) {
            return;
        }

        struct timeval tv {};
        char strTime[24];

        gettimeofday(&tv, nullptr);
        time_t timeStamp = tv.tv_sec;
        struct tm localTime {};
        if (strftime(strTime, sizeof strTime, "%Y-%m-%d %H:%M:%S.", localtime_r(&timeStamp, &localTime)) != 0) {
            std::cout << strTime << tv.tv_usec << " " << LogLevelDesc(level) << " " << syscall(SYS_gettid) << " " <<
                oss << std::endl;
        } else {
            std::cout << " Invalid time " << LogLevelDesc(level) << " " << syscall(SYS_gettid) << " " << oss <<
                std::endl;
        }
    }

    HyBMOutLogger(const HyBMOutLogger &) = delete;
    HyBMOutLogger(HyBMOutLogger &&) = delete;
    HyBMOutLogger& operator=(HyBMOutLogger&&) = delete;
    HyBMOutLogger& operator=(const HyBMOutLogger&) = delete;

    ~HyBMOutLogger()
    {
        mLogFunc = nullptr;
    }

private:
    HyBMOutLogger() = default;

    inline const std::string &LogLevelDesc(int level)
    {
        static std::string invalid = "invalid";
        if (UNLIKELY(level < DEBUG_LEVEL || level >= BUTT_LEVEL)) {
            return invalid;
        }
        return mLogLevelDesc[level];
    }

private:
    const std::string mLogLevelDesc[BUTT_LEVEL] = {"debug", "info", "warn", "error"};

    LogLevel mLogLevel = INFO_LEVEL;
    ExternalLog mLogFunc = nullptr;
};

// macro for log
#ifndef HYBM_LOG_FILENAME_SHORT
#define HYBM_LOG_FILENAME_SHORT (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif
#define BM_OUT_LOG(LEVEL, ARGS)                                                        \
    do {                                                                               \
        std::ostringstream oss;                                                        \
        oss << "[HyBM " << HYBM_LOG_FILENAME_SHORT << ":" << __LINE__ << "] " << ARGS; \
        HyBMOutLogger::Instance()->Log(LEVEL, oss.str());                                    \
    } while (0)

#define BM_LOG_DEBUG(ARGS) BM_OUT_LOG(DEBUG_LEVEL, ARGS)
#define BM_LOG_INFO(ARGS) BM_OUT_LOG(INFO_LEVEL, ARGS)
#define BM_LOG_WARN(ARGS) BM_OUT_LOG(WARN_LEVEL, ARGS)
#define BM_LOG_ERROR(ARGS) BM_OUT_LOG(ERROR_LEVEL, ARGS)

#define BM_ASSERT_RETURN(ARGS, RET)              \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            BM_LOG_ERROR("Assert " << #ARGS);    \
            return RET;                          \
        }                                        \
    } while (0)
    
#define BM_VALIDATE_RETURN(ARGS, msg, RET)       \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            BM_LOG_ERROR(msg);                   \
            return RET;                          \
        }                                        \
    } while (0)

#define BM_ASSERT_RET_VOID(ARGS)                 \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            BM_LOG_ERROR("Assert " << #ARGS);    \
            return;                              \
        }                                        \
    } while (0)

#define BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(result, msg) \
    do {                                              \
        auto innerResult = (result);                  \
        if (UNLIKELY(innerResult != 0)) {             \
            BM_LOG_ERROR(msg);                        \
            return innerResult;                       \
        }                                             \
    } while (0)

#define BM_ASSERT(ARGS)                          \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            BM_LOG_ERROR("Assert " << #ARGS);    \
        }                                        \
    } while (0)
}
}

#endif // MEMFABRIC_HYBRID_LOGGER_H
