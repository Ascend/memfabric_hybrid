/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_SMEM_LOGGER_H
#define MEMFABRIC_HYBRID_SMEM_LOGGER_H

#include <ctime>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <unistd.h>
#include <sstream>
#include <sys/time.h>
#include <sys/syscall.h>

#include "smem_define.h"
#include "smem_types.h"

namespace ock {
namespace smem {
using ExternalLog = void (*)(int, const char*);

enum LogLevel : int {
    DEBUG_LEVEL = 0,
    INFO_LEVEL,
    WARN_LEVEL,
    ERROR_LEVEL,
    BUTT_LEVEL  // no use
};

class SMOutLogger {
public:
    static SMOutLogger* Instance()
    {
        static SMOutLogger* gLogger = nullptr;
        static std::mutex gMutex;

        if (__builtin_expect(gLogger == nullptr, 0) != 0) {
            gMutex.lock();
            if (gLogger == nullptr) {
                gLogger = new (std::nothrow) SMOutLogger();

                if (gLogger == nullptr) {
                    printf("Failed to new SMOutLogger, probably out of memory");
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

    inline void SetAuditLogLevel(LogLevel level)
    {
        mAuditLogLevel = level;
    }

    inline void SetExternalLogFunction(ExternalLog func, bool forceUpdate = false)
    {
        if (mLogFunc == nullptr || forceUpdate) {
            mLogFunc = func;
        }
    }

    inline void SetExternalAuditLogFunction(ExternalLog func, bool forceUpdate = false)
    {
        if (mAuditLogFunc == nullptr || forceUpdate) {
            mAuditLogFunc = func;
        }
    }

    inline void Log(int level, const std::ostringstream& oss)
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
            std::cout << strTime << std::setw(6) << std::setfill('0') << tv.tv_usec << " " << LogLevelDesc(level) << " "
                      << syscall(SYS_gettid) << " " << oss.str() << std::endl;
        } else {
            std::cout << " Invalid time " << LogLevelDesc(level) << " " << syscall(SYS_gettid) << " " << oss.str()
                      << std::endl;
        }
    }

    inline void AuditLog(int level, const std::ostringstream& oss)
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
            std::cout << strTime << std::setw(6) << std::setfill('0') << tv.tv_usec << " " << LogLevelDesc(level) << " "
                      << syscall(SYS_gettid) << " " << oss.str() << std::endl;
        } else {
            std::cout << " Invalid time " << LogLevelDesc(level) << " " << syscall(SYS_gettid) << " " << oss.str()
                      << std::endl;
        }
    }

    SMOutLogger(const SMOutLogger&) = delete;
    SMOutLogger(SMOutLogger&&) = delete;

    ~SMOutLogger()
    {
        mLogFunc = nullptr;
        mAuditLogFunc = nullptr;
    }

private:
    SMOutLogger() = default;

    inline const std::string& LogLevelDesc(int level)
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
    LogLevel mAuditLogLevel = INFO_LEVEL;
    ExternalLog mLogFunc = nullptr;
    ExternalLog mAuditLogFunc = nullptr;
};
}  // namespace smem
}  // namespace ock

// macro for log
#ifndef SMEM_LOG_FILENAME_SHORT
#define SMEM_LOG_FILENAME_SHORT (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif
#define SM_OUT_LOG(LEVEL, ARGS)                                                        \
    do {                                                                               \
        std::ostringstream oss;                                                        \
        oss << "[SMEM " << SMEM_LOG_FILENAME_SHORT << ":" << __LINE__ << "] " << ARGS; \
        ock::smem::SMOutLogger::Instance()->Log(LEVEL, oss);                           \
    } while (0)

#define SM_AUDIT_OUT_LOG(LEVEL, ARGS)                             \
    do {                                                          \
        std::ostringstream oss;                                   \
        oss << "[SMEM_AUDIT] " << ARGS;                           \
        ock::smem::SMOutLogger::Instance()->AuditLog(LEVEL, oss); \
    } while (0)

#define SM_LOG_DEBUG(ARGS) SM_OUT_LOG(ock::smem::DEBUG_LEVEL, ARGS)
#define SM_LOG_INFO(ARGS) SM_OUT_LOG(ock::smem::INFO_LEVEL, ARGS)
#define SM_LOG_WARN(ARGS) SM_OUT_LOG(ock::smem::WARN_LEVEL, ARGS)
#define SM_LOG_ERROR(ARGS) SM_OUT_LOG(ock::smem::ERROR_LEVEL, ARGS)

#define SM_AUDIT_LOG_INFO(MODULE, ARGS) SM_AUDIT_OUT_LOG(ock::smem::INFO_LEVEL, ARGS)
#define SM_AUDIT_LOG_WARN(MODULE, ARGS) SM_AUDIT_OUT_LOG(ock::smem::WARN_LEVEL, ARGS)
#define SM_AUDIT_LOG_ERROR(MODULE, ARGS) SM_AUDIT_OUT_LOG(ock::smem::ERROR_LEVEL, ARGS)

#define SM_ASSERT_RETURN(ARGS, RET)              \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            SM_LOG_ERROR("Assert " << #ARGS);    \
            return RET;                          \
        }                                        \
    } while (0)

#define SM_ASSERT_RET_VOID(ARGS)                 \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            SM_LOG_ERROR("Assert " << #ARGS);    \
            return;                              \
        }                                        \
    } while (0)

#define SM_ASSERT_RETURN_NOLOG(ARGS, RET)        \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            return RET;                          \
        }                                        \
    } while (0)

#define SM_ASSERT(ARGS)                          \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            SM_LOG_ERROR("Assert " << #ARGS);    \
        }                                        \
    } while (0)

#define SM_LOG_ERROR_RETURN_IT_IF_NOT_OK(result, msg) \
    do {                                              \
        auto innerResult = (result);                  \
        if (UNLIKELY(innerResult != 0)) {             \
            SM_LOG_ERROR(msg);                        \
            return innerResult;                       \
        }                                             \
    } while (0)

#define SM_RETURN_IT_IF_NOT_OK(result)    \
    do {                                  \
        auto innerResult = (result);      \
        if (UNLIKELY(innerResult != 0)) { \
            return innerResult;           \
        }                                 \
    } while (0)

#define SM_PARAM_VALIDATE(expression, msg, returnValue) \
    do {                                                \
        if ((expression)) {                             \
            SM_LOG_ERROR(msg);                          \
            return returnValue;                         \
        }                                               \
    } while (0)

template<class T>
inline std::string SmemArray2String(const T *arr, uint32_t len)
{
    std::ostringstream oss;
    oss << "[";
    for (uint32_t i = 0; i < len; i++) {
        oss << arr[i] << (i + 1 == len ? "]" : ",");
    }
    return oss.str();
}

#endif  // MEMFABRIC_HYBRID_SMEM_LOGGER_H
