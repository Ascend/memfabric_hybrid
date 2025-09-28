/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "mmc_meta_service_process.h"

#include <condition_variable>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <unistd.h>
#include <pybind11/embed.h>
#include <pybind11/numpy.h>
#include "spdlogger4c.h"
#include "spdlogger.h"

#include "mmc_configuration.h"
#include "mmc_env.h"
#include "mmc_logger.h"
#include "mmc_meta_service_default.h"
#include "mmc_ptracer.h"


namespace ock {
namespace mmc {
static std::mutex g_exitMtx;
static std::condition_variable g_exitCv;
static bool g_processExit = false;


int MmcMetaServiceProcess::MainForExecutable()
{
    pybind11::scoped_interpreter guard{};
    pybind11::gil_scoped_release release;

    // Python 方式启动已经在解释器中，不能再次创建解释器
    return MainForPython();
}

int MmcMetaServiceProcess::MainForPython()
{
    if (CheckIsRunning()) {
        std::cerr << "Error, meta service is already running." << std::endl;
        return -1;
    }

    if (LoadConfig() != 0) {
        std::cerr << "Error, failed to load config." << std::endl;
        return -1;
    }

    RegisterSignal();

    ptracer_config_t ptraceConfig{.tracerType = 1, .dumpFilePath = "/var/log/mxc/memfabric_hybrid"};
    const auto result = ptracer_init(&ptraceConfig);
    if (result != MMC_OK) {
        std::cerr << "init ptracer module failed, result: " << result << std::endl;
        return -1;
    }

    if (InitLogger(config_)) {
        std::cerr << "Error, failed to init logger." << std::endl;
        return -1;
    }

    if (config_.haEnable) {
        leaderElection_ = new(std::nothrow)MmcMetaServiceLeaderElection(
            "leader_election", META_POD_NAME, META_NAMESPACE, META_LEASE_NAME);
        if (leaderElection_ == nullptr || leaderElection_->Start(config_) != MMC_OK) {
            std::cerr << "Error, failed to start meta service leader election." << std::endl;
            return -1;
        }
    }

    metaService_ = new (std::nothrow)MmcMetaServiceDefault("meta_service");
    if (metaService_ == nullptr || metaService_->Start(config_) != MMC_OK) {
        delete leaderElection_;
        std::cerr << "Error, failed to start MmcMetaService." << std::endl;
        return -1;
    }

    MMC_AUDIT_LOG("Meta Service launched successfully");

    std::unique_lock<std::mutex> lock(g_exitMtx);
    g_exitCv.wait(lock, []() { return g_processExit; });
    Exit();

    MMC_AUDIT_LOG("Meta Service stopped");

    return 0;
}


bool MmcMetaServiceProcess::CheckIsRunning()
{
    const std::string filePath = "/tmp/mmc_meta_service";
    const std::string fileName = filePath + ".lock";
    const int fd = open(fileName.c_str(), O_WRONLY | O_CREAT, 0600);
    if (fd < 0) {
        std::cerr << "Open file " << fileName.c_str() << " failed, error message is " << strerror(errno) << "."
                  << std::endl;
        return true;
    }
    flock lock{};
    lock.l_type = F_WRLCK;
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    lock.l_len = 0;
    const auto ret = fcntl(fd, F_SETLK, &lock);
    if (ret < 0) {
        std::cerr << "Fail to start mmc_meta_service, process lock file is locked." << std::endl;
        close(fd);
        return true;
    }
    return false;
}

int MmcMetaServiceProcess::LoadConfig()
{
    if (MMC_META_CONF_PATH.empty()) {
        std::cerr << "MMC_META_CONFIG_PATH is not set." << std::endl;
        return -1;
    }
    const auto confPath = MMC_META_CONF_PATH;
    MetaServiceConfig configManager;
    if (!configManager.LoadFromFile(confPath)) {
        std::cerr << "Failed to load config from file" << std::endl;
        return -1;
    }
    const std::vector<std::string> validationError = configManager.ValidateConf();
    if (!validationError.empty()) {
        std::cerr << "Wrong configuration in file <" << confPath << ">, because of following mistakes:" << std::endl;
        for (auto &item : validationError) {
            std::cout << item << std::endl;
        }
        return -1;
    }
    configManager.GetMetaServiceConfig(config_);
    if (MetaServiceConfig::ValidateTLSConfig(config_.accTlsConfig) != MMC_OK) {
        std::cerr << "Invalid tls config." << std::endl;
        return -1;
    }
    if (MetaServiceConfig::ValidateTLSConfig(config_.configStoreTlsConfig) != MMC_OK) {
        std::cerr << "Invalid tls config." << std::endl;
        return -1;
    }
    if (MetaServiceConfig::ValidateLogPathConfig(config_.logPath) != MMC_OK) {
        std::cerr << "Invalid log path, please check 'ock.mmc.log_path' " << std::endl;
        return -1;
    }

    return 0;
}

void MmcMetaServiceProcess::RegisterSignal()
{
    const sighandler_t oldIntHandler = signal(SIGINT, SignalInterruptHandler);
    if (oldIntHandler == SIG_ERR) {
        std::cerr << "Register SIGINT handler failed" << std::endl;
    }

    const sighandler_t oldTermHandler = signal(SIGTERM, SignalInterruptHandler);
    if (oldTermHandler == SIG_ERR) {
        std::cerr << "Register SIGTERM handler failed" << std::endl;
    }
}

void MmcMetaServiceProcess::SignalInterruptHandler(const int signal)
{
    std::cout << "Received exit signal[" << signal << "]" << std::endl;

    {
        std::unique_lock<std::mutex> lock(g_exitMtx);
        g_processExit = true;
    }
    g_exitCv.notify_all();
}

int MmcMetaServiceProcess::InitLogger(const mmc_meta_service_config_t& options)
{
    const std::string logPath = std::string(options.logPath) + "/logs/mmc-meta.log";
    const std::string logAuditPath = std::string(options.logPath) + "/logs/mmc-meta-audit.log";

    std::cout << "Meta service log level " << options.logLevel
              << ", log path: " << logPath
              << ", audit log path: " << logAuditPath
              << ", log rotation file size: " << options.logRotationFileSize
              << ", log rotation file count: " << options.logRotationFileCount
              << std::endl;

    auto ret = MmcOutLogger::Instance().SetLogLevel(static_cast<LogLevel>(options.logLevel));
    if (ret != 0) {
        std::cerr << "Failed to set log level " << options.logLevel << std::endl;
        return -1;
    }

    ret = SPDLOG_Init(logPath.c_str(), options.logLevel, options.logRotationFileSize, options.logRotationFileCount);
    if (ret != 0) {
        std::cerr << "Failed to init spdlog, error: " << SPDLOG_GetLastErrorMessage() << std::endl;
        return -1;
    }
    MmcOutLogger::Instance().SetExternalLogFunction(SPDLOG_LogMessage);

    ret = SPDLOG_AuditInit(logAuditPath.c_str(), options.logRotationFileSize, options.logRotationFileCount);
    if (ret != 0) {
        std::cerr << "Failed to init audit spdlog, error: " << SPDLOG_GetLastErrorMessage() << std::endl;
        return -1;
    }
    MmcOutLogger::Instance().SetExternalAuditLogFunction(SPDLOG_AuditLogMessage);

    return 0;
}

void MmcMetaServiceProcess::Exit()
{
    if (metaService_ != nullptr) {
        metaService_->Stop();
    }
    if (leaderElection_ != nullptr) {
        leaderElection_->Stop();
    }
    ptracer_uninit();
}

} // namespace mmc
} // namespace ock
