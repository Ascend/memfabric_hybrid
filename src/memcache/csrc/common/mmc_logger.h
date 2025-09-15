/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_MMC_LOGGER_H
#define MEMFABRIC_HYBRID_MMC_LOGGER_H

#include <ctime>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <unistd.h>
#include <sstream>
#include <sys/time.h>
#include <sys/syscall.h>

#include "mmc_define.h"

namespace ock {
namespace mmc {
#define OBJ_MAX_LOG_FILE_SIZE 20971520 // 每个日志文件的最大大小
#define OBJ_MAX_LOG_FILE_NUM 50
constexpr int MICROSECOND_WIDTH = 6;

using ExternalLog = void (*)(int, const char *);
using ExternalAuditLog = void (*)(const char *);

enum LogLevel : int {
    DEBUG_LEVEL = 0,
    INFO_LEVEL,
    WARN_LEVEL,
    ERROR_LEVEL,
    BUTT_LEVEL  // no use
};

class MmcOutLogger {
public:
    static MmcOutLogger &Instance()
    {
        static MmcOutLogger gLogger;
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

    inline ExternalAuditLog GetAuditLogExtraFunc() const
    {
        return auditLogFunc_;
    }

    inline int32_t SetLogLevel(LogLevel level)
    {
        if (level < DEBUG_LEVEL || level >= BUTT_LEVEL) {
            return -1;
        }
        logLevel_ = level;
        return 0;
    }

    inline void SetExternalLogFunction(ExternalLog func, bool forceUpdate = false)
    {
        if (logFunc_ == nullptr || forceUpdate) {
            logFunc_ = func;
        }
    }

    inline void SetExternalAuditLogFunction(ExternalAuditLog func, bool forceUpdate = false)
    {
        if (auditLogFunc_ == nullptr || forceUpdate) {
            auditLogFunc_ = func;
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
            std::cout << strTime << std::setw(MICROSECOND_WIDTH) << std::setfill('0') << tv.tv_usec << " "
                      << LogLevelDesc(level) << " " << syscall(SYS_gettid) << " " << oss.str() << std::endl;
        } else {
            std::cout << " Invalid time " << LogLevelDesc(level) << " " << syscall(SYS_gettid) << " " << oss.str()
                      << std::endl;
        }
    }

    inline void AuditLog(const std::ostringstream &oss)
    {
        if (auditLogFunc_ != nullptr) {
            auditLogFunc_(oss.str().c_str());
            return;
        }
    }

    int32_t GetLogLevel(const std::string &logLevelDesc)
    {
        for (uint32_t count = DEBUG_LEVEL; count < BUTT_LEVEL; count++) {
            if (logLevelDesc == logLevelDesc_[count]) {
                return count;
            }
        }
        // 没有匹配到日志级别，使用默认级别INFO
        return INFO_LEVEL;
    }

    MmcOutLogger(const MmcOutLogger &) = delete;
    MmcOutLogger(MmcOutLogger &&) = delete;
    MmcOutLogger& operator=(const MmcOutLogger& other) = delete;
    MmcOutLogger& operator=(MmcOutLogger &&) = delete;

    ~MmcOutLogger()
    {
        logFunc_ = nullptr;
        auditLogFunc_ = nullptr;
    }

private:
    MmcOutLogger() = default;

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
    ExternalAuditLog auditLogFunc_ = nullptr;

    const char *logLevelDesc_[BUTT_LEVEL] = {"debug", "info", "warn", "error"};
};
}  // namespace mmc
}  // namespace ock

// macro for log
#ifndef MMC_LOG_FILENAME_SHORT
#define MMC_LOG_FILENAME_SHORT (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif
#define MMC_OUT_LOG(LEVEL, ARGS)                                                      \
    do {                                                                              \
        std::ostringstream oss;                                                       \
        oss << "[MMC " << MMC_LOG_FILENAME_SHORT << ":" << __LINE__ << "] " << ARGS;  \
        ock::mmc::MmcOutLogger::Instance().Log(LEVEL, oss);                           \
    } while (0)
#define MMC_OUT_AUDIT_LOG(MSG)                                                        \
    do {                                                                              \
        std::ostringstream oss;                                                       \
        oss << "[MMC " << MMC_LOG_FILENAME_SHORT << ":" << __LINE__ << "] " << (MSG); \
        ock::mmc::MmcOutLogger::Instance().AuditLog(oss);                             \
    } while (0)
#define MMC_LOG_ERROR_WITH_ERRCODE(ARGS, ERRCODE)                                                                  \
    do {                                                                                                           \
        std::ostringstream oss;                                                                                    \
        oss << "[MMC " << MMC_LOG_FILENAME_SHORT << ":" << __LINE__ << "] " << ARGS << ", error code " << ERRCODE; \
        ock::mmc::MmcOutLogger::Instance().Log(ock::mmc::ERROR_LEVEL, oss);                                        \
    } while (0)


#define MMC_LOG_DEBUG(ARGS) MMC_OUT_LOG(ock::mmc::DEBUG_LEVEL, ARGS)
#define MMC_LOG_INFO(ARGS) MMC_OUT_LOG(ock::mmc::INFO_LEVEL, ARGS)
#define MMC_LOG_WARN(ARGS) MMC_OUT_LOG(ock::mmc::WARN_LEVEL, ARGS)
#define MMC_LOG_ERROR(ARGS) MMC_OUT_LOG(ock::mmc::ERROR_LEVEL, ARGS)

#define MMC_AUDIT_LOG(MSG) MMC_OUT_AUDIT_LOG(MSG)

// if ARGS is false, print error
#define MMC_ASSERT_RETURN(ARGS, RET)             \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            MMC_LOG_ERROR("Assert " << #ARGS);   \
            return RET;                          \
        }                                        \
    } while (0)

#define MMC_ASSERT_RET_VOID(ARGS)                \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            MMC_LOG_ERROR("Assert " << #ARGS);   \
            return;                              \
        }                                        \
    } while (0)

#define MMC_ASSERT(ARGS)                         \
    do {                                         \
        if (__builtin_expect(!(ARGS), 0) != 0) { \
            MMC_LOG_ERROR("Assert " << #ARGS);   \
        }                                        \
    } while (0)

#define MMC_RETURN_ERROR(result, msg)                        \
    do {                                                     \
        auto innerResult = (result);                         \
        if (UNLIKELY(innerResult != 0)) {                    \
            MMC_LOG_ERROR_WITH_ERRCODE(msg, innerResult);    \
            return innerResult;                              \
        }                                                    \
    } while (0)

#define MMC_FALSE_ERROR(result, msg)                         \
    do {                                                     \
        auto innerResult = (result);                         \
        if (UNLIKELY(!innerResult)) {                        \
            MMC_LOG_ERROR(msg);                              \
            return innerResult;                              \
        }                                                    \
    } while (0)

#endif  // MEMFABRIC_HYBRID_MMC_LOGGER_H
