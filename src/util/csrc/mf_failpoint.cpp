/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "mf_failpoint.h"

#include <cctype>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <sstream>
#include <sys/stat.h>
#include <thread>
#include <unordered_map>
#include <unistd.h>

#include "mf_out_logger.h"
#include "mf_str_util.h"

namespace ock {
namespace mf {

#ifndef MF_ENABLE_TRACEPOINT

FaultInjectionPointStatus FaultInjectionPointManager::Init()
{
    return FaultInjectionPointStatus::OK;
}

FaultInjectionPointStatus FaultInjectionPointManager::Exit()
{
    return FaultInjectionPointStatus::OK;
}

FaultInjectionPointStatus FaultInjectionPointManager::Reload()
{
    return FaultInjectionPointStatus::OK;
}

FaultInjectionPointStatus FaultInjectionPointManager::Register(const std::string &name, const std::string &desc)
{
    (void)name;
    (void)desc;
    return FaultInjectionPointStatus::OK;
}

FaultInjectionPointStatus FaultInjectionPointManager::Unregister(const std::string &name)
{
    (void)name;
    return FaultInjectionPointStatus::OK;
}

FaultInjectionPointStatus FaultInjectionPointManager::Activate(const std::string &name, FaultInjectionPointType type,
                                                               uint32_t timeAlive,
                                                               const FaultInjectionPointParam &userParam)
{
    (void)name;
    (void)type;
    (void)timeAlive;
    (void)userParam;
    return FaultInjectionPointStatus::OK;
}

FaultInjectionPointStatus FaultInjectionPointManager::Deactivate(const std::string &name)
{
    (void)name;
    return FaultInjectionPointStatus::OK;
}

FaultInjectionPointStatus FaultInjectionPointManager::DeactivateAll()
{
    return FaultInjectionPointStatus::OK;
}

bool FaultInjectionPointManager::IsActive(const std::string &name)
{
    (void)name;
    return false;
}

FaultInjectionPointParam FaultInjectionPointManager::MakeParam(const std::string &value)
{
    (void)value;
    return {};
}

FaultInjectionPointStatus
FaultInjectionPointManager::RegisterImpl(const std::string &name, const std::string &desc,
                                         const std::shared_ptr<detail::FaultInjectionPointCallbackBase> &callback)
{
    (void)name;
    (void)desc;
    (void)callback;
    return FaultInjectionPointStatus::OK;
}

FaultInjectionPointExecution FaultInjectionPointManager::BeginImpl(const char *name, const std::type_index &signature,
                                                                   void **args)
{
    (void)name;
    (void)signature;
    (void)args;
    return {};
}

#else

namespace {

constexpr char FAULT_INJECTION_POINT_LOG_TAG[] = "[FIP] ";
constexpr char FAULT_INJECTION_POINT_CONFIG_FILE_PREFIX[] = "/tmp/mf_failpoints_";
constexpr char FAULT_INJECTION_POINT_CONFIG_FILE_SUFFIX[] = ".conf";
constexpr uint32_t FAULT_INJECTION_POINT_DEFAULT_PAUSE_MS = 10000;

struct FaultInjectionPointConfig {
    FaultInjectionPointType type = FaultInjectionPointType::BUTT;
    uint32_t timeAlive = 0;
    FaultInjectionPointParam userParam{};
};

struct ConfigFileState {
    bool exists = false;
    std::time_t modifiedTime = 0;
    long long size = 0;
};

struct FaultInjectionPointRecord {
    explicit FaultInjectionPointRecord(std::string descriptionValue) : description(std::move(descriptionValue)) {}

    std::mutex mutex;
    std::string description;
    std::shared_ptr<detail::FaultInjectionPointCallbackBase> callback;
    std::size_t registerCount = 0;
    bool active = false;
    FaultInjectionPointType type = FaultInjectionPointType::BUTT;
    uint32_t timeAlive = 0;
    uint32_t timeCalled = 0;
    FaultInjectionPointParam userParam{};
};

struct FaultInjectionPointState {
    std::mutex mutex;
    std::unordered_map<std::string, std::shared_ptr<FaultInjectionPointRecord> > records;
    std::unordered_map<std::string, FaultInjectionPointConfig> fileConfigs;
    uint32_t initCount = 0;
    bool configFileStateInitialized = false;
    ConfigFileState lastConfigFileState{};
};

FaultInjectionPointState &GetFaultInjectionPointState()
{
    static FaultInjectionPointState state;
    return state;
}

std::string GetConfigPath()
{
    return std::string(FAULT_INJECTION_POINT_CONFIG_FILE_PREFIX) + std::to_string(getpid()) +
           FAULT_INJECTION_POINT_CONFIG_FILE_SUFFIX;
}

ConfigFileState ReadConfigFileState(const std::string &configPath)
{
    ConfigFileState state;
    struct stat statBuffer {};
    if (stat(configPath.c_str(), &statBuffer) != 0) {
        return state;
    }

    state.exists = true;
    state.modifiedTime = statBuffer.st_mtime;
    state.size = static_cast<long long>(statBuffer.st_size);
    return state;
}

bool IsConfigFileStateChanged(const ConfigFileState &lhs, const ConfigFileState &rhs)
{
    return lhs.exists != rhs.exists || lhs.modifiedTime != rhs.modifiedTime || lhs.size != rhs.size;
}

FaultInjectionPointType ParseFaultInjectionPointType(const std::string &value)
{
    if (value == "callback") {
        return FaultInjectionPointType::CALLBACK;
    }
    if (value == "reset") {
        return FaultInjectionPointType::RESET;
    }
    if (value == "pause") {
        return FaultInjectionPointType::PAUSE;
    }
    if (value == "abort") {
        return FaultInjectionPointType::ABORT;
    }
    return FaultInjectionPointType::BUTT;
}

uint32_t ParsePauseMs(const FaultInjectionPointParam &param)
{
    uint32_t pauseMs = 0;
    if (!StrUtil::String2Uint(param.paramData, pauseMs) || pauseMs == 0) {
        return FAULT_INJECTION_POINT_DEFAULT_PAUSE_MS;
    }
    return pauseMs;
}

void NormalizeConfig(FaultInjectionPointConfig &config)
{
    if (config.type != FaultInjectionPointType::PAUSE) {
        return;
    }

    uint32_t pauseMs = ParsePauseMs(config.userParam);
    std::snprintf(config.userParam.paramData, sizeof(config.userParam.paramData), "%u", pauseMs);
}

void SetInactiveLocked(FaultInjectionPointRecord &record)
{
    record.active = false;
    record.type = FaultInjectionPointType::BUTT;
    record.timeAlive = 0;
    record.timeCalled = 0;
    std::memset(record.userParam.paramData, 0, sizeof(record.userParam.paramData));
}

FaultInjectionPointStatus ApplyConfigLocked(FaultInjectionPointRecord &record, const FaultInjectionPointConfig &config)
{
    if (config.type == FaultInjectionPointType::CALLBACK && record.callback == nullptr) {
        return FaultInjectionPointStatus::CALLBACK_NULL;
    }

    record.active = config.timeAlive > 0;
    record.type = config.type;
    record.timeAlive = config.timeAlive;
    record.timeCalled = 0;
    record.userParam = config.userParam;
    return FaultInjectionPointStatus::OK;
}

bool ShouldReloadFromConfigFile()
{
    const ConfigFileState currentConfigFileState = ReadConfigFileState(GetConfigPath());
    FaultInjectionPointState &state = GetFaultInjectionPointState();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (!state.configFileStateInitialized) {
        if (!currentConfigFileState.exists) {
            state.lastConfigFileState = currentConfigFileState;
            state.configFileStateInitialized = true;
            return false;
        }
        return true;
    }
    return IsConfigFileStateChanged(currentConfigFileState, state.lastConfigFileState);
}

void ReloadIfRequested()
{
    if (ShouldReloadFromConfigFile()) {
        (void)FaultInjectionPointManager::Reload();
    }
}

bool ParseConfigLine(const std::string &line, std::string &name, FaultInjectionPointConfig &config)
{
    std::istringstream input(line);
    std::string typeString;
    std::string timeAliveString;

    if (!(input >> name >> typeString >> timeAliveString)) {
        return false;
    }

    config.type = ParseFaultInjectionPointType(typeString);
    if (config.type == FaultInjectionPointType::BUTT) {
        return false;
    }

    if (!StrUtil::String2Uint(timeAliveString, config.timeAlive)) {
        return false;
    }

    std::string userParam;
    std::getline(input, userParam);
    userParam = StrUtil::StrTrim(userParam);
    std::snprintf(config.userParam.paramData, sizeof(config.userParam.paramData), "%s", userParam.c_str());
    NormalizeConfig(config);
    return true;
}

FaultInjectionPointConfig MakeConfig(FaultInjectionPointType type, uint32_t timeAlive,
                                     const FaultInjectionPointParam &userParam)
{
    FaultInjectionPointConfig config;
    config.type = type;
    config.timeAlive = timeAlive;
    config.userParam = userParam;
    NormalizeConfig(config);
    return config;
}

void LogTrigger(const std::string &name, FaultInjectionPointType type, const FaultInjectionPointParam &param)
{
    switch (type) {
        case FaultInjectionPointType::CALLBACK:
            MF_OUT_LOG(FAULT_INJECTION_POINT_LOG_TAG, INFO_LEVEL, "Triggered callback FIP '" << name << "'");
            break;
        case FaultInjectionPointType::PAUSE:
            MF_OUT_LOG(FAULT_INJECTION_POINT_LOG_TAG, INFO_LEVEL,
                       "Triggered pause FIP '" << name << "' ms=" << ParsePauseMs(param));
            break;
        case FaultInjectionPointType::RESET:
            MF_OUT_LOG(FAULT_INJECTION_POINT_LOG_TAG, WARN_LEVEL, "Triggered reset FIP '" << name << "'");
            break;
        case FaultInjectionPointType::ABORT:
            MF_OUT_LOG(FAULT_INJECTION_POINT_LOG_TAG, FATAL_LEVEL, "Triggered abort FIP '" << name << "'");
            break;
        default:
            break;
    }
}

}  // namespace

FaultInjectionPointStatus FaultInjectionPointManager::Init()
{
    FaultInjectionPointState &state = GetFaultInjectionPointState();
    std::lock_guard<std::mutex> lock(state.mutex);
    ++state.initCount;
    return FaultInjectionPointStatus::OK;
}

FaultInjectionPointStatus FaultInjectionPointManager::Exit()
{
    FaultInjectionPointState &state = GetFaultInjectionPointState();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.initCount == 0) {
        return FaultInjectionPointStatus::OK;
    }

    --state.initCount;
    if (state.initCount > 0) {
        return FaultInjectionPointStatus::OK;
    }

    state.records.clear();
    state.fileConfigs.clear();
    state.configFileStateInitialized = false;
    state.lastConfigFileState = {};
    return FaultInjectionPointStatus::OK;
}

FaultInjectionPointStatus FaultInjectionPointManager::Reload()
{
    const std::string configPath = GetConfigPath();
    std::unordered_map<std::string, FaultInjectionPointConfig> configs;
    std::ifstream input(configPath);
    if (!input.is_open()) {
        MF_OUT_LOG(FAULT_INJECTION_POINT_LOG_TAG, INFO_LEVEL, "Config file not found: " << configPath);
    } else {
        std::string line;
        int lineNumber = 0;
        while (std::getline(input, line)) {
            ++lineNumber;
            std::string trimmed = StrUtil::StrTrim(line);
            if (trimmed.empty() || trimmed[0] == '#') {
                continue;
            }

            std::string name;
            FaultInjectionPointConfig config;
            if (!ParseConfigLine(trimmed, name, config)) {
                MF_OUT_LOG(FAULT_INJECTION_POINT_LOG_TAG, WARN_LEVEL,
                           "Skip invalid config line " << lineNumber << ": " << trimmed);
                continue;
            }
            if (configs.find(name) != configs.end()) {
                MF_OUT_LOG(FAULT_INJECTION_POINT_LOG_TAG, WARN_LEVEL,
                           "Ignore duplicated config for FIP '" << name << "'");
                continue;
            }
            configs.emplace(name, config);
        }
    }

    FaultInjectionPointState &state = GetFaultInjectionPointState();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.fileConfigs = configs;
    state.lastConfigFileState = ReadConfigFileState(configPath);
    state.configFileStateInitialized = true;
    for (auto &item : state.records) {
        std::lock_guard<std::mutex> recordLock(item.second->mutex);
        SetInactiveLocked(*item.second);
        auto configIt = state.fileConfigs.find(item.first);
        if (configIt == state.fileConfigs.end()) {
            continue;
        }

        FaultInjectionPointStatus status = ApplyConfigLocked(*item.second, configIt->second);
        if (status == FaultInjectionPointStatus::CALLBACK_NULL) {
            MF_OUT_LOG(FAULT_INJECTION_POINT_LOG_TAG, WARN_LEVEL,
                       "Skip callback activation for '" << item.first << "' because callback is not registered");
        }
    }
    return FaultInjectionPointStatus::OK;
}

FaultInjectionPointStatus FaultInjectionPointManager::Register(const std::string &name, const std::string &desc)
{
#ifdef MF_ENABLE_TRACEPOINT
    return RegisterImpl(name, desc, nullptr);
#else
    (void)name;
    (void)desc;
    return FaultInjectionPointStatus::OK;
#endif
}

FaultInjectionPointStatus
FaultInjectionPointManager::RegisterImpl(const std::string &name, const std::string &desc,
                                         const std::shared_ptr<detail::FaultInjectionPointCallbackBase> &callback)
{
    FaultInjectionPointState &state = GetFaultInjectionPointState();
    std::lock_guard<std::mutex> lock(state.mutex);
    auto it = state.records.find(name);
    if (it == state.records.end()) {
        std::shared_ptr<FaultInjectionPointRecord> record = std::make_shared<FaultInjectionPointRecord>(desc);
        record->callback = callback;
        record->registerCount = 1;
        state.records.emplace(name, record);

        auto configIt = state.fileConfigs.find(name);
        if (configIt != state.fileConfigs.end()) {
            std::lock_guard<std::mutex> recordLock(record->mutex);
            FaultInjectionPointStatus status = ApplyConfigLocked(*record, configIt->second);
            if (status == FaultInjectionPointStatus::CALLBACK_NULL) {
                MF_OUT_LOG(FAULT_INJECTION_POINT_LOG_TAG, WARN_LEVEL,
                           "Skip callback activation for '" << name << "' because callback is not registered");
            }
        }
        return FaultInjectionPointStatus::OK;
    }

    std::shared_ptr<FaultInjectionPointRecord> record = it->second;
    std::lock_guard<std::mutex> recordLock(record->mutex);
    if (record->description != desc) {
        return FaultInjectionPointStatus::ERROR;
    }

    uintptr_t callbackKey = callback == nullptr ? 0 : callback->GetKey();
    uintptr_t recordCallbackKey = record->callback == nullptr ? 0 : record->callback->GetKey();
    std::type_index callbackType = callback == nullptr ? std::type_index(typeid(void)) : callback->GetSignature();
    std::type_index recordType =
        record->callback == nullptr ? std::type_index(typeid(void)) : record->callback->GetSignature();
    if (callbackKey != recordCallbackKey || callbackType != recordType) {
        return FaultInjectionPointStatus::ERROR;
    }

    ++record->registerCount;
    return FaultInjectionPointStatus::OK;
}

FaultInjectionPointStatus FaultInjectionPointManager::Unregister(const std::string &name)
{
    FaultInjectionPointState &state = GetFaultInjectionPointState();
    std::lock_guard<std::mutex> lock(state.mutex);
    auto it = state.records.find(name);
    if (it == state.records.end()) {
        return FaultInjectionPointStatus::NOT_FOUND;
    }

    std::lock_guard<std::mutex> recordLock(it->second->mutex);
    if (it->second->registerCount > 1) {
        --it->second->registerCount;
        return FaultInjectionPointStatus::OK;
    }

    state.records.erase(it);
    return FaultInjectionPointStatus::OK;
}

FaultInjectionPointStatus FaultInjectionPointManager::Activate(const std::string &name, FaultInjectionPointType type,
                                                               uint32_t timeAlive,
                                                               const FaultInjectionPointParam &userParam)
{
    ReloadIfRequested();
    FaultInjectionPointState &state = GetFaultInjectionPointState();
    std::shared_ptr<FaultInjectionPointRecord> record;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        auto it = state.records.find(name);
        if (it == state.records.end()) {
            return FaultInjectionPointStatus::NOT_FOUND;
        }
        record = it->second;
    }

    FaultInjectionPointConfig config = MakeConfig(type, timeAlive, userParam);
    std::lock_guard<std::mutex> recordLock(record->mutex);
    return ApplyConfigLocked(*record, config);
}

FaultInjectionPointStatus FaultInjectionPointManager::Deactivate(const std::string &name)
{
    ReloadIfRequested();
    FaultInjectionPointState &state = GetFaultInjectionPointState();
    std::shared_ptr<FaultInjectionPointRecord> record;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        auto configIt = state.fileConfigs.find(name);
        if (configIt != state.fileConfigs.end()) {
            state.fileConfigs.erase(configIt);
        }
        auto it = state.records.find(name);
        if (it == state.records.end()) {
            return FaultInjectionPointStatus::NOT_FOUND;
        }
        record = it->second;
    }

    std::lock_guard<std::mutex> recordLock(record->mutex);
    SetInactiveLocked(*record);
    return FaultInjectionPointStatus::OK;
}

FaultInjectionPointStatus FaultInjectionPointManager::DeactivateAll()
{
    ReloadIfRequested();
    FaultInjectionPointState &state = GetFaultInjectionPointState();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.fileConfigs.clear();
    for (auto &item : state.records) {
        std::lock_guard<std::mutex> recordLock(item.second->mutex);
        SetInactiveLocked(*item.second);
    }
    return FaultInjectionPointStatus::OK;
}

bool FaultInjectionPointManager::IsActive(const std::string &name)
{
    ReloadIfRequested();
    FaultInjectionPointState &state = GetFaultInjectionPointState();
    std::shared_ptr<FaultInjectionPointRecord> record;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        auto it = state.records.find(name);
        if (it == state.records.end()) {
            return false;
        }
        record = it->second;
    }

    std::lock_guard<std::mutex> recordLock(record->mutex);
    return record->active && record->timeAlive > 0;
}

FaultInjectionPointParam FaultInjectionPointManager::MakeParam(const std::string &value)
{
    FaultInjectionPointParam param{};
    std::snprintf(param.paramData, sizeof(param.paramData), "%s", value.c_str());
    return param;
}

FaultInjectionPointExecution FaultInjectionPointManager::BeginImpl(const char *name, const std::type_index &signature,
                                                                   void **args)
{
    ReloadIfRequested();

    FaultInjectionPointState &state = GetFaultInjectionPointState();
    std::shared_ptr<FaultInjectionPointRecord> record;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        auto it = state.records.find(name);
        if (it == state.records.end()) {
            return {};
        }
        record = it->second;
    }

    FaultInjectionPointType type = FaultInjectionPointType::BUTT;
    FaultInjectionPointParam userParam{};
    std::shared_ptr<detail::FaultInjectionPointCallbackBase> callback;
    {
        std::lock_guard<std::mutex> recordLock(record->mutex);
        if (!record->active || record->timeAlive == 0) {
            SetInactiveLocked(*record);
            return {};
        }

        if (record->type == FaultInjectionPointType::CALLBACK) {
            if (record->callback == nullptr) {
                MF_OUT_LOG(FAULT_INJECTION_POINT_LOG_TAG, WARN_LEVEL,
                           "FIP '" << name << "' is activated as callback without a registered callback");
                return {};
            }
            if (record->callback->GetSignature() != signature) {
                MF_OUT_LOG(FAULT_INJECTION_POINT_LOG_TAG, WARN_LEVEL,
                           "FIP '" << name << "' callback signature mismatch");
                return {};
            }
        }

        type = record->type;
        userParam = record->userParam;
        callback = record->callback;
        --record->timeAlive;
        ++record->timeCalled;
        if (record->timeAlive == 0) {
            record->active = false;
            record->type = FaultInjectionPointType::BUTT;
        }
    }

    FaultInjectionPointExecution execution;
    execution.skipBlock = (type == FaultInjectionPointType::CALLBACK || type == FaultInjectionPointType::RESET ||
                           type == FaultInjectionPointType::ABORT);
    LogTrigger(name, type, userParam);

    if (type == FaultInjectionPointType::CALLBACK) {
        callback->Invoke(&userParam, args);
    } else if (type == FaultInjectionPointType::PAUSE) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ParsePauseMs(userParam)));
    } else if (type == FaultInjectionPointType::RESET) {
        std::raise(SIGTERM);
    } else if (type == FaultInjectionPointType::ABORT) {
        std::abort();
    }

    return execution;
}

#endif

}  // namespace mf
}  // namespace ock
