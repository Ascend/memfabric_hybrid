/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_USER_LOGGER_H
#define MEMFABRIC_HYBRID_USER_LOGGER_H

#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <ostream>
#include <sstream>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef UNLIKELY
#define UNLIKELY(x) (__builtin_expect(!!(x), 0) != 0)
#endif

namespace ock {
namespace mf {
using ExternalLog = void (*)(int level, const char *msg);

enum LogLevel : int {
    DEBUG_LEVEL = 0,
    INFO_LEVEL,
    WARN_LEVEL,
    ERROR_LEVEL,
    BUTT_LEVEL,
};

class HybmUserOutLogger {
public:
    static HybmUserOutLogger *Instance()
    {
        static HybmUserOutLogger *gLogger = nullptr;
        static std::mutex gMutex;

        if (__builtin_expect(gLogger == nullptr, 0) != 0) {
            gMutex.lock();
            if (gLogger == nullptr) {
                gLogger = new (std::nothrow) HybmUserOutLogger();

                if (gLogger == nullptr) {
                    printf("Failed to new HybmUserOutLogger, probably out of memory");
                }
            }
            gMutex.unlock();
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

    inline void Log(int level, const std::ostringstream &oss)
    {
        if (mLogFunc != nullptr) {
            mLogFunc(level, oss.str().c_str());
            return;
        }

        if (level < mLogLevel) {
            return;
        }

        struct timeval tv{};
        char strTime[24];

        gettimeofday(&tv, nullptr);
        time_t timeStamp = tv.tv_sec;
        struct tm localTime{};
        if (strftime(strTime, sizeof strTime, "%Y-%m-%d %H:%M:%S.", localtime_r(&timeStamp, &localTime)) != 0) {
            std::cout << strTime << std::setw(6) << std::setfill('0') << tv.tv_usec << " " << LogLevelDesc(level) << " "
                      << syscall(SYS_gettid) << " " << oss.str() << std::endl;
        } else {
            std::cout << " Invalid time " << LogLevelDesc(level) << " " << syscall(SYS_gettid) << " " << oss.str()
                      << std::endl;
        }
    }

    HybmUserOutLogger(const HybmUserOutLogger &) = delete;
    HybmUserOutLogger(HybmUserOutLogger &&) = delete;

    ~HybmUserOutLogger()
    {
        mLogFunc = nullptr;
    }

private:
    HybmUserOutLogger() = default;

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

    LogLevel mLogLevel = DEBUG_LEVEL;
    ExternalLog mLogFunc = nullptr;
};

// macro for log
#ifndef HYBM_USER_LOG_FILENAME_SHORT
#define HYBM_USER_LOG_FILENAME_SHORT (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif
#define BM_OUT_LOG(LEVEL, ARGS)                                                             \
    do {                                                                                    \
        std::ostringstream oss;                                                             \
        oss << "[HyBM " << HYBM_USER_LOG_FILENAME_SHORT << ":" << __LINE__ << "] " << ARGS; \
        HybmUserOutLogger::Instance()->Log(LEVEL, oss);                                     \
    } while (0)

#define BM_USER_LOG_DEBUG(ARGS) BM_OUT_LOG(DEBUG_LEVEL, ARGS)
#define BM_USER_LOG_INFO(ARGS)  BM_OUT_LOG(INFO_LEVEL, ARGS)
#define BM_USER_LOG_WARN(ARGS)  BM_OUT_LOG(WARN_LEVEL, ARGS)
#define BM_USER_LOG_ERROR(ARGS) BM_OUT_LOG(ERROR_LEVEL, ARGS)

#define BM_ASSERT_RETURN(ARGS, RET)                \
    do {                                           \
        if (__builtin_expect(!(ARGS), 0) != 0) {   \
            BM_USER_LOG_ERROR("Assert " << #ARGS); \
            return RET;                            \
        }                                          \
    } while (0)

#define BM_ASSERT_RET_VOID(ARGS)                   \
    do {                                           \
        if (__builtin_expect(!(ARGS), 0) != 0) {   \
            BM_USER_LOG_ERROR("Assert " << #ARGS); \
            return;                                \
        }                                          \
    } while (0)

#define BM_ASSERT(ARGS)                            \
    do {                                           \
        if (__builtin_expect(!(ARGS), 0) != 0) {   \
            BM_USER_LOG_ERROR("Assert " << #ARGS); \
        }                                          \
    } while (0)
}
}

#endif // MEMFABRIC_HYBRID_USER_LOGGER_H
