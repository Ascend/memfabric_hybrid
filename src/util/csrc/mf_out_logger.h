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

#ifndef MEMFABRIC_HYBRID_BASE_LOGGER_H
#define MEMFABRIC_HYBRID_BASE_LOGGER_H

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

#include "mf_spinlock.h"

// macro for gcc optimization for prediction of if/else
#ifndef LIKELY
#define LIKELY(x) (__builtin_expect(!!(x), 1) != 0)
#endif

#ifndef UNLIKELY
#define UNLIKELY(x) (__builtin_expect(!!(x), 0) != 0)
#endif

#define PID_TID " [" << getpid() << "-" << syscall(SYS_gettid) << "]"

namespace ock {
namespace mf {
using ExternalLog = void (*)(int, const char *);

enum LogLevel : int {
    DEBUG_LEVEL = 0,
    INFO_LEVEL,
    WARN_LEVEL,
    ERROR_LEVEL,
    FATAL_LEVEL,
    BUTT_LEVEL // no use
};

class LockFreeLogThrottler {
public:
    static bool ShouldLog()
    {
        using namespace std::chrono;
        static thread_local ThrottleState state(0, 0);
        const uint64_t now = duration_cast<seconds>(steady_clock::now().time_since_epoch()).count();
        uint64_t windowTime = state.windowStartTimeSec.load(std::memory_order_relaxed);
        if (now - windowTime >= INTERVAL) {
            state.windowStartTimeSec.store(now, std::memory_order_relaxed);
            state.counter.store(1, std::memory_order_relaxed);
            return true;
        }
        const auto count = state.counter.fetch_add(1, std::memory_order_relaxed);
        return count < BURST;
    }

private:
    struct ThrottleState {
        std::atomic<uint64_t> windowStartTimeSec;
        std::atomic<uint32_t> counter;

        ThrottleState(uint64_t time = 0, uint32_t count = 0) : windowStartTimeSec(time), counter(count) {}
    };
    static constexpr uint64_t INTERVAL = 7ULL;
    static constexpr uint64_t BURST = 5ULL;
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

    inline ExternalLog GetExternalLogFunction() const
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

    inline void LogLimit(int level, std::string logMsg)
    {
        if (LockFreeLogThrottler::ShouldLog()) {
            Log(level, logMsg);
        }
    }

    inline void Log(int level, std::string logMsg)
    {
        // LCOV_EXCL_START
        logMsg.erase(std::remove_if(logMsg.begin(), logMsg.end(), [](char c) { return c == '\r' || c == '\n'; }),
                     logMsg.end());
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
            const uint8_t TIME_WIDTH = 6U;
            consoleSpinLock_.lock();
            std::cout << strTime << std::setw(TIME_WIDTH) << std::setfill('0') << tv.tv_usec << " "
                      << LogLevelDesc(level) << PID_TID << logMsg << std::endl;
            consoleSpinLock_.unlock();
        } else {
            consoleSpinLock_.lock();
            std::cout << " Invalid time " << LogLevelDesc(level) << PID_TID << logMsg << std::endl;
            consoleSpinLock_.unlock();
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

    mutable SpinLock consoleSpinLock_;

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
} // namespace mf
} // namespace ock

// macro for log
#ifndef UT_ENABLED
#define MF_LOG_FILENAME_SHORT (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#else
#define MF_LOG_FILENAME_SHORT (__FILE__)
#endif

#define MF_LOG_FORMAT MF_LOG_FILENAME_SHORT << ":" << __LINE__ << " " << __FUNCTION__ << "] "
#define MF_OUT_LOG(TAG, LEVEL, ARGS)                                                  \
    do {                                                                              \
        if (static_cast<int>(LEVEL) < ock::mf::OutLogger::Instance().GetLogLevel()) { \
            break;                                                                    \
        }                                                                             \
        std::ostringstream oss;                                                       \
        oss << (TAG) << MF_LOG_FORMAT << ARGS;                                        \
        ock::mf::OutLogger::Instance().Log(static_cast<int>(LEVEL), oss.str());       \
    } while (0)

#define MF_OUT_LOG_LIMIT(TAG, LEVEL, ARGS)                                            \
    do {                                                                              \
        if (static_cast<int>(LEVEL) < ock::mf::OutLogger::Instance().GetLogLevel()) { \
            break;                                                                    \
        }                                                                             \
        std::ostringstream oss;                                                       \
        oss << (TAG) << MF_LOG_FORMAT << ARGS;                                        \
        ock::mf::OutLogger::Instance().LogLimit(static_cast<int>(LEVEL), oss.str());  \
    } while (0)

#endif // MEMFABRIC_HYBRID_LOGGER_H