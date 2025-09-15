/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlogger.h"

namespace ock::mmc::log {
thread_local std::string SpdLogger::gLastErrorMessage;
constexpr int ROTATION_FILE_SIZE_MAX = 500 * 1024 * 1024; // 500MB
constexpr int ROTATION_FILE_SIZE_MIN = 1 * 1024 * 1024;   // 1MB
constexpr int ROTATION_FILE_COUNT_MAX = 50;
constexpr mode_t LOG_FILE_CREATE_MODE = 0640;
constexpr mode_t LOG_FILE_READ_ONLY_MODE = 0440;

int SpdLogger::ValidateParams(int minLogLevel, const std::string& path, int rotationFileSize, int rotationFileCount)
{
    if (minLogLevel < static_cast<int>(LogLevel::TRACE) || minLogLevel >= static_cast<int>(LogLevel::CRITICAL)) {
        gLastErrorMessage = "Invalid min log level, which should be 0,1,2,3,4,5";
        return -1;
    }

    // for file
    if (rotationFileSize > ROTATION_FILE_SIZE_MAX || rotationFileSize < ROTATION_FILE_SIZE_MIN) {
        gLastErrorMessage = "Invalid max file size, which should be between 2MB to 100MB";
        return -1;
    } else if (rotationFileCount > ROTATION_FILE_COUNT_MAX || rotationFileCount < 1) {
        gLastErrorMessage = "Invalid max file count, which should be less than 50";
        return -1;
    }
    return 0;
}

void SpdLogger::LogMessage(int level, const char *message)
{
    if (mSPDLogger == nullptr) {
        gLastErrorMessage = "No logger created";
        return;
    }
    mSPDLogger->log(static_cast<spdlog::level::level_enum>(level), "{}", message);
}

void SpdLogger::AuditLogMessage(const char *message)
{
    if (mSPDLogger == nullptr) {
        gLastErrorMessage = "No logger created";
        return;
    }
    const int level = 3;
    mSPDLogger->log(static_cast<spdlog::level::level_enum>(level), "{}", message);
}

const char *SpdLogger::GetLastErrorMessage()
{
    return gLastErrorMessage.c_str();
}

int SpdLogger::Initialize(const std::string &path, int minLogLevel, int rotationFileSize, int rotationFileCount)
{
    try {
        std::lock_guard<std::mutex> guard(mutex_);
        if (started_) {
            return 0;
        }
        if (ValidateParams(minLogLevel, path, rotationFileSize, rotationFileCount) != 0) {
            return -1;
        }
        std::string logName = "log:" + path;
        spdlog::file_event_handlers handlers;
        handlers.before_open = &BeforeOpenCallback;
        handlers.after_open = &AfterOpenCallback;
        handlers.after_close = &AfterCloseCallback;
        mSPDLogger = spdlog::rotating_logger_mt(logName.c_str(), path,
                                                rotationFileSize, rotationFileCount,
                                                true, handlers);
        if (mSPDLogger == nullptr) {
            gLastErrorMessage = "spdlog logger is not created yet";
            return -1;
        }
        mSPDLogger->set_pattern("%v");
        mSPDLogger->info("", "");
        mSPDLogger->set_pattern("%Y-%m-%d %H:%M:%S.%f %t %v");
        mSPDLogger->info("Log default format: yyyy-mm-dd hh:mm:ss.uuuuuu threadid loglevel msg");
        mSPDLogger->set_pattern("%Y-%m-%d %H:%M:%S.%f %t %l %v");
        spdlog::flush_every(std::chrono::seconds(1));
        mSPDLogger->set_level(static_cast<spdlog::level::level_enum>(minLogLevel));
        mSPDLogger->flush_on(spdlog::level::err);
        started_ = true;
    } catch (const spdlog::spdlog_ex &ex) {
        gLastErrorMessage = "Failed to create log: ";
        gLastErrorMessage += ex.what();
        return -1;
    } catch (...) {
        return -1;
    }

    return 0;
}

int SpdLogger::SetLogMinLevel(int minLevel)
{
    if (minLevel < static_cast<int>(LogLevel::TRACE) ||
        minLevel > static_cast<int>(LogLevel::CRITICAL)) {
        gLastErrorMessage = "Invalid log level, which should be 0~5";
        return -1;
    }
    if (mSPDLogger != nullptr) {
        mSPDLogger->set_level(static_cast<spdlog::level::level_enum>(minLevel));
        return 0;
    }
    return -1;
}

void SpdLogger::Flush(void)
{
    if (mSPDLogger != nullptr) {
        mSPDLogger->flush();
    }
}

void SpdLogger::BeforeOpenCallback(const std::string &filename)
{
    chmod(filename.c_str(), LOG_FILE_CREATE_MODE);
}

void SpdLogger::AfterOpenCallback(const std::string &filename, std::FILE *file_stream)
{
    chmod(filename.c_str(), LOG_FILE_READ_ONLY_MODE);
}

void SpdLogger::AfterCloseCallback(const std::string &filename)
{
    chmod(filename.c_str(), LOG_FILE_READ_ONLY_MODE);
}

}
