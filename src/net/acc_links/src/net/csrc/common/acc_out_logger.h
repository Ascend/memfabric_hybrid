/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef ACC_LINKS_ACC_OUT_LOGGER_H
#define ACC_LINKS_ACC_OUT_LOGGER_H

#include <ctime>
#include <cstring>
#include <iostream>
#include <mutex>
#include <unistd.h>
#include <sstream>
#include <sys/time.h>
#include <sys/syscall.h>

namespace ock {
namespace acc {
#ifndef ACC_LINKS_OUT_LOGGER
using AccExternalLog = void(*)(int level, const char* msg);
#endif

enum AccLogLevel : int { DEBUG_LEVEL = 0, INFO_LEVEL, WARN_LEVEL, ERROR_LEVEL, BUTT_LEVEL };

class AccOutLogger {
public:
    static AccOutLogger* Instance()
    {
        static AccOutLogger* gLogger = nullptr;
        static std::mutex gMutex;

        if (__builtin_expect(gLogger == nullptr, 0) != 0) {
            std::lock_guard<std::mutex> lg(gMutex);
            if (gLogger == nullptr) {
                gLogger = new (std::nothrow) AccOutLogger();

                if (gLogger == nullptr) {
                    printf("Failed to new AccOutLogger, probably out of memory");
                }
            }
        }

        return gLogger;
    }

    inline void SetLogLevel(AccLogLevel level)
    {
        mLogLevel = level;
    }

    inline void SetAuditLogLevel(AccLogLevel level)
    {
        mAuditLogLevel = level;
    }

    inline void SetExternalLogFunction(AccExternalLog func, bool forceUpdate = false)
    {
        if (mLogFunc == nullptr || forceUpdate) {
            mLogFunc = func;
        }
    }

    inline void SetExternalAuditLogFunction(AccExternalLog func, bool forceUpdate = false)
    {
        if (mAuditLogFunc == nullptr || forceUpdate) {
            mAuditLogFunc = func;
        }
    }

    void Log(int level, const std::ostringstream &oss)
    {
        if (mLogFunc != nullptr) {
            mLogFunc(level, oss.str().c_str());
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
            std::cout << strTime << tv.tv_usec << " " << LogLevelDesc(level) << " " << syscall(SYS_gettid) << " "
                      << oss.str() << std::endl;
        } else {
            std::cout << " Invalid time " << LogLevelDesc(level) << " " << syscall(SYS_gettid) << " " << oss.str()
                      << std::endl;
        }
    }

    void AuditLog(int level, const std::ostringstream &oss)
    {
        if (mAuditLogFunc != nullptr) {
            mAuditLogFunc(level, oss.str().c_str());
            return;
        }

        if (level < mAuditLogLevel) {
            return;
        }

        struct timeval tv {};
        char strTime[24];
        gettimeofday(&tv, nullptr);
        time_t timeStamp = tv.tv_sec;
        struct tm localTime {};
        if (strftime(strTime, sizeof strTime, "%Y-%m-%d %H:%M:%S.", localtime_r(&timeStamp, &localTime)) != 0) {
            std::cout << strTime << tv.tv_usec << " " << LogLevelDesc(level) << " " << syscall(SYS_gettid) << " "
                      << oss.str() << std::endl;
        } else {
            std::cout << " Invalid time " << LogLevelDesc(level) << " " << syscall(SYS_gettid) << " " << oss.str()
                      << std::endl;
        }
    }

    AccOutLogger(const AccOutLogger&) = delete;
    AccOutLogger(AccOutLogger&&) = delete;
    AccOutLogger& operator=(const AccOutLogger&) = delete;
    AccOutLogger& operator=(AccOutLogger&&) = delete;

    ~AccOutLogger()
    {
        mLogFunc = nullptr;
        mAuditLogFunc = nullptr;
    }

private:
    AccOutLogger() = default;

    inline const std::string &LogLevelDesc(int level)
    {
        static std::string invalid = "invalid";
        if (level < DEBUG_LEVEL || level >= BUTT_LEVEL) {
            return invalid;
        }
        return mLogLevelDesc[level];
    }

private:
    const std::string mLogLevelDesc[BUTT_LEVEL] = {"debug", "info", "warn", "error"};

    AccLogLevel mLogLevel = INFO_LEVEL;
    AccLogLevel mAuditLogLevel = INFO_LEVEL;
    AccExternalLog mLogFunc = nullptr;
    AccExternalLog mAuditLogFunc = nullptr;
};

/* macro for log */
#ifndef ACC_LINKS_FILENAME_SHORT
#define ACC_LINKS_FILENAME_SHORT (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif
#define ACC_LINKS_OUT_LOG(LEVEL, MODULE, ARGS)                                                       \
    do {                                                                                             \
        std::ostringstream oss;                                                                      \
        oss << "[" << #MODULE << " " << ACC_LINKS_FILENAME_SHORT << ":" << __LINE__ << "] " << ARGS; \
        AccOutLogger::Instance()->Log(LEVEL, oss);                                                   \
    } while (0)

#define AUDIT_OUT_LOG(LEVEL, MODULE, ARGS)              \
    do {                                                \
        std::ostringstream oss;                         \
        oss << "[AUDIT " << #MODULE << "] " << ARGS;    \
        AccOutLogger::Instance()->AuditLog(LEVEL, oss); \
    } while (0)

#define LOG_DEBUG(ARGS) ACC_LINKS_OUT_LOG(DEBUG_LEVEL, AccLinks, ARGS)
#define LOG_INFO(ARGS) ACC_LINKS_OUT_LOG(INFO_LEVEL, AccLinks, ARGS)
#define LOG_WARN(ARGS) ACC_LINKS_OUT_LOG(WARN_LEVEL, AccLinks, ARGS)
#define LOG_ERROR(ARGS) ACC_LINKS_OUT_LOG(ERROR_LEVEL, AccLinks, ARGS)

#define AUDIT_LOG_INFO(ARGS) AUDIT_OUT_LOG(INFO_LEVEL, AccLinks, ARGS)
#define AUDIT_LOG_WARN(ARGS) AUDIT_OUT_LOG(WARN_LEVEL, AccLinks, ARGS)
#define AUDIT_LOG_ERROR(ARGS) AUDIT_OUT_LOG(ERROR_LEVEL, AccLinks, ARGS)

#ifndef ENABLE_TRACE_LOG
#define LOG_TRACE(x)
#elif
#define LOG_TRACE(x) ACC_LINKS_OUT_LOG(DEBUG_LEVEL, AccLinksTrace, ARGS)
#endif

#define ASSERT_RETURN(ARGS, RET)           \
    do {                                   \
        if (UNLIKELY(!(ARGS))) {           \
            LOG_ERROR("Assert " << #ARGS); \
            return RET;                    \
        }                                  \
    } while (0)

#define ASSERT_RET_VOID(ARGS)              \
    do {                                   \
        if (UNLIKELY(!(ARGS))) {           \
            LOG_ERROR("Assert " << #ARGS); \
            return;                        \
        }                                  \
    } while (0)

#define ASSERT(ARGS)                       \
    do {                                   \
        if (UNLIKELY(!(ARGS))) {           \
            LOG_ERROR("Assert " << #ARGS); \
        }                                  \
    } while (0)

#define LOG_ERROR_RETURN_IT_IF_NOT_OK(result, msg) \
    do {                                           \
        auto innerResult = (result);               \
        if (UNLIKELY(innerResult != ACC_OK)) {     \
            LOG_ERROR(msg);                        \
            return innerResult;                    \
        }                                          \
    } while (0)

}  // namespace acc
}  // namespace ock

#endif  // ACC_LINKS_ACC_OUT_LOGGER_H
