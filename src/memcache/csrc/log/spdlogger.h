/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 * Description: ulog.h
 * date: 2023-06-20
 */

#ifndef MEMORYFABRIC_SPDLOGGER_FOR_H
#define MEMORYFABRIC_SPDLOGGER_FOR_H

#include <cstdint>
#include <string>
#include <sstream>
#include <chrono>
#include <memory>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

namespace ock::mmc::log {
enum class LogLevel {
    TRACE    = 0,
    DEBUG    = 1,
    INFO     = 2,
    WARN     = 3,
    ERROR    = 4,
    CRITICAL = 5,
    LOG_LEVEL_MAX,
};
class SpdLogger {
public:
    SpdLogger() = default;

    ~SpdLogger() = default;

    static SpdLogger& GetInstance()
    {
        static SpdLogger instance;
        return instance;
    }

    static SpdLogger& GetAuditInstance()
    {
        static SpdLogger instance;
        return instance;
    }

    int Initialize(const std::string &path, int minLogLevel, int rotationFileSize, int rotationFileCount);
    int SetLogMinLevel(int minLevel);
    void LogMessage(int level, const char *message);
    void AuditLogMessage(const char *message);
    static const char *GetLastErrorMessage();
    void Flush(void);

private:
    static int ValidateParams(int minLogLevel, const std::string& path, int rotationFileSize,
        int rotationFileCount);

    static void BeforeOpenCallback(const std::string& filename);
    static void AfterOpenCallback(const std::string& filename, std::FILE *file_stream);
    static void AfterCloseCallback(const std::string& filename);

    std::mutex mutex_;
    bool started_ = false;
    std::shared_ptr<spdlog::logger> mSPDLogger;
    std::string mFilePath;
    int mRotationFileSize = 0;
    int mRotationFileCount = 0;
    bool mDebugEnabled = false;
    static thread_local std::string gLastErrorMessage;
};
}
#endif // MEMORYFABRIC_SPDLOGGER_FOR_H