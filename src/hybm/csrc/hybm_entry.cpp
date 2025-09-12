/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include <cstdlib>
#include <fstream>
#include <limits.h>
#include <mutex>
#include <string>

#include "devmm_svm_gva.h"
#include "hybm.h"
#include "hybm_ptracer.h"
#include "hybm_cmd.h"
#include "hybm_common_include.h"
#include "hybm_gvm_user.h"
#include "hybm_version.h"
#include "under_api/dl_acl_api.h"
#include "under_api/dl_api.h"
#include "under_api/dl_hal_api.h"

using namespace ock::mf;

namespace {
const std::string LEAST_DRIVER_VER = "V100R001C21B035";

int64_t initialized = 0;
int32_t initedDeviceId = -1;
int32_t initedLogicDeviceId = -1;
bool initedGvm = false;
std::mutex initMutex;
}

int32_t HybmGetInitDeviceId()
{
    return initedDeviceId;
}

int32_t HybmGetInitedLogicDeviceId()
{
    return initedLogicDeviceId;
}

bool HybmHasInited()
{
    return initialized > 0;
}

bool HybmGvmHasInited()
{
    return initedGvm;
}

static std::string GetDriverVersionPath(const std::string &driverEnvStr, const std::string &keyStr)
{
    std::string driverVersionPath;
    std::string tempPath; // 存放临时路径
    // 查找driver安装路径
    for (uint32_t i = 0; i < driverEnvStr.length(); ++i) {
        // 环境变量中存放的每段路径之间以':'隔开
        if (driverEnvStr[i] != ':') {
            tempPath += driverEnvStr[i];
        }
        // 对存放driver版本文件的路径进行搜索
        if (driverEnvStr[i] == ':' || i == driverEnvStr.length() - 1) {
            auto found = tempPath.find(keyStr);
            if (found == std::string::npos) {
                tempPath.clear();
                continue;
            }
            // 确保不是部分匹配
            if (tempPath.length() <= found + keyStr.length() || tempPath[found + keyStr.length()] == '/') {
                driverVersionPath = tempPath.substr(0, found);
                break;
            }
            tempPath.clear();
        }
    }
    return driverVersionPath;
}

static std::string LoadDriverVersionInfoFile(const std::string &realName, const std::string &keyStr)
{
    std::string driverVersion;
    // 打开该文件前，判断该文件路径是否有效、规范
    char realFile[PATH_MAX] = {0};
    if (realpath(realName.c_str(), realFile) == nullptr) {
        BM_LOG_WARN("driver version path " << realName << " is not a valid real path");
        return "";
    }

    // realFile转str,然后open这个str
    std::ifstream infile(realFile, std::ifstream::in);
    if (!infile.is_open()) {
        BM_LOG_WARN("driver version file " << realFile << " does not exist");
        return "";
    }

    // 逐行读取，结果放在line中，寻找带有keyStr的字符串
    std::string line;
    int32_t maxRows = 100; // 在文件中读取的最长行数为100，避免超大文件长时间读取
    while (getline(infile, line)) {
        --maxRows;
        if (maxRows < 0) {
            BM_LOG_WARN("driver version file content is too long.");
            return "";
        }
        auto found = line.find(keyStr);
        // 刚好匹配前缀
        if (found == 0) {
            uint32_t len = line.length() - keyStr.length();    // 版本字符串长度
            driverVersion = line.substr(keyStr.length(), len); // 从keyStr截断
            break;
        }
    }
    infile.close();
    return driverVersion;
}

static std::string CastDriverVersion(const std::string &driverEnv)
{
    std::string driverVersionPath = GetDriverVersionPath(driverEnv, "/driver/lib64");
    if (!driverVersionPath.empty()) {
        driverVersionPath += "/driver/version.info";
        std::string driverVersion = LoadDriverVersionInfoFile(driverVersionPath, "Innerversion=");
        return driverVersion;
    }
    BM_LOG_WARN("cannot found version file in :" << driverEnv);
    return "";
}

static int32_t GetValueFromVersion(const std::string &ver, std::string key)
{
    int32_t val = 0;
    auto found = ver.find(key);
    if (found == std::string::npos) {
        return -1;
    }

    std::string tmp;
    while (++found < ver.length()) {
        if (std::isdigit(ver[found])) {
            tmp += ver[found];
        } else {
            break;
        }
    }

    try {
        val = std::stoi(tmp);
    } catch (...) {
        val = -1;
    }
    return val;
}

static bool DriverVersionCheck(const std::string &ver)
{
    auto libPath = std::getenv("LD_LIBRARY_PATH");
    if (libPath == nullptr) {
        BM_LOG_ERROR("check driver version failed, Environment LD_LIBRARY_PATH not set.");
        return false;
    }

    std::string readVer = CastDriverVersion(libPath);
    if (readVer.empty()) {
        BM_LOG_ERROR("check driver version failed, read version is empty.");
        return false;
    }

    int32_t baseVal = GetValueFromVersion(ver, "V");
    int32_t readVal = GetValueFromVersion(readVer, "V");
    if (baseVal == -1 || readVal == -1 || baseVal != readVal) {
        BM_LOG_ERROR("check driver version failed, Version not equal, limit:" << ver << " read:" << readVer);
        return false;
    }

    baseVal = GetValueFromVersion(ver, "R");
    readVal = GetValueFromVersion(readVer, "R");
    if (baseVal == -1 || readVal == -1 || baseVal != readVal) {
        BM_LOG_ERROR("check driver version failed, Release not equal, limit:" << ver << " read:" << readVer);
        return false;
    }

    baseVal = GetValueFromVersion(ver, "C");
    readVal = GetValueFromVersion(readVer, "C");
    if (baseVal == -1 || readVal == -1 || readVal < baseVal) {
        BM_LOG_ERROR("check driver version failed, Customer is too low, limit:" << ver << " read:" << readVer);
        return false;
    }
    if (readVal > baseVal) {
        return true;
    }

    baseVal = GetValueFromVersion(ver, "B");
    readVal = GetValueFromVersion(readVer, "B");
    if (baseVal == -1 || readVal == -1 || readVal < baseVal) {
        BM_LOG_ERROR("check driver version failed, Build is too low, input:" << ver << " read:" << readVer);
        return false;
    }
    return true;
}

int32_t HalGvaPrecheck(void)
{
    if (DriverVersionCheck(LEAST_DRIVER_VER)) {
        return BM_OK;
    }
    return BM_ERROR;
}

static int32_t hybm_init_hbm_gva(uint16_t deviceId, uint64_t flags)
{
    if (HalGvaPrecheck() != BM_OK) {
        BM_LOG_WARN("the current version of ascend driver does not support hbm global virtual address!");
        return BM_OK;
    }

    initedLogicDeviceId = Func::GetLogicDeviceId(deviceId);
    if (initedLogicDeviceId < 0) {
        BM_LOG_ERROR("Failed to get logic deviceId: " << deviceId);
        return BM_ERROR;
    }

    auto ret = DlAclApi::AclrtSetDevice(deviceId);
    if (ret != BM_OK) {
        DlApi::CleanupLibrary();
        BM_LOG_ERROR("set device id to be " << deviceId << " failed: " << ret);
        return BM_ERROR;
    }

    if (flags & HYBM_INIT_GVM_FLAG) {
        ret = hybm_gvm_init(initedLogicDeviceId);
        if (ret != 0) {
            BM_LOG_ERROR("init hybm gvm failed: " << ret);
            return BM_ERROR;
        }
        initedGvm = true;
    } else {
        initedGvm = false;
    }

    void *globalMemoryBase = nullptr;
    size_t allocSize = HYBM_DEVICE_INFO_SIZE; // 申请meta空间
    drv::HybmInitialize(initedLogicDeviceId, DlHalApi::GetDevmmFd());
    ret = drv::HalGvaReserveMemory((uint64_t *)&globalMemoryBase, allocSize, initedLogicDeviceId, flags);
    if (ret != 0) {
        DlApi::CleanupLibrary();
        BM_LOG_ERROR("initialize mete memory with size: " << allocSize << ", flag: " << flags << " failed: " << ret);
        return BM_ERROR;
    }

    ret = drv::HalGvaAlloc(HYBM_DEVICE_META_ADDR, HYBM_DEVICE_INFO_SIZE, 0);
    if (ret != BM_OK) {
        DlApi::CleanupLibrary();
        (void)drv::HalGvaUnreserveMemory();
        BM_LOG_ERROR("HalGvaAlloc hybm meta memory failed: " << ret);
        return BM_MALLOC_FAILED;
    }

    return BM_OK;
}

HYBM_API int32_t hybm_init(uint16_t deviceId, uint64_t flags)
{
    std::unique_lock<std::mutex> lockGuard{initMutex};
    if (initialized > 0) {
        if (initedDeviceId != deviceId) {
            BM_LOG_ERROR("this deviceId(" << deviceId << ") is not equal to the deviceId(" << initedDeviceId
                                          << ") of other module!");
            return BM_ERROR;
        }

        initialized++;
        return 0;
    }

    ptracer_config_t config{.tracerType = 1, .dumpFilePath = "/var/log/mxc/memfabric_hybrid"};
    auto result = ptracer_init(&config);
    if (result != BM_OK) {
        BM_LOG_ERROR("init ptracer module failed, result: " << result);
        return result;
    }

    auto path = std::getenv("ASCEND_HOME_PATH");
    if (path == nullptr) {
        BM_LOG_ERROR("Environment ASCEND_HOME_PATH not set.");
        return BM_ERROR;
    }

    auto libPath = std::string(path).append("/lib64");
    auto ret = DlApi::LoadLibrary(libPath);
    BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "load library from path: " << libPath << " failed: " << ret, ret);

    ret = hybm_init_hbm_gva(deviceId, flags);
    if (ret != BM_OK) {
        DlApi::CleanupLibrary();
        BM_LOG_ERROR("set device id to be " << deviceId << " failed: " << ret);
        return BM_ERROR;
    }

    initedDeviceId = deviceId;
    initialized = 1L;
    BM_LOG_INFO("hybm init successfully, " << LIB_VERSION << ", deviceId: " << deviceId);
    return 0;
}

HYBM_API void hybm_uninit()
{
    std::unique_lock<std::mutex> lockGuard{initMutex};
    if (initialized <= 0L) {
        BM_LOG_WARN("hybm not initialized.");
        return;
    }

    ptracer_uninit();

    if (--initialized > 0L) {
        return;
    }

    auto ret = drv::HalGvaUnreserveMemory();
    BM_LOG_INFO("uninitialize GVA memory return: " << ret);
    DlApi::CleanupLibrary();
    initialized = 0;
}

HYBM_API int32_t hybm_set_extern_logger(void (*logger)(int level, const char *msg))
{
    auto instance = HyBMOutLogger::Instance();
    if (instance == nullptr) {
        return -1;
    }

    instance->SetExternalLogFunction(logger);
    return 0;
}

HYBM_API int32_t hybm_set_log_level(int level)
{
    auto instance = HyBMOutLogger::Instance();
    if (instance == nullptr) {
        return -1;
    }

    if (level < 0 || level >= BUTT_LEVEL) {
        BM_LOG_ERROR("Set log level error, invalid param level: " << level);
        return -1;
    }

    instance->SetLogLevel(static_cast<LogLevel>(level));
    return 0;
}

HYBM_API const char *hybm_get_error_string(int32_t errCode)
{
    static thread_local std::string info;
    info = std::string("error(").append(std::to_string(errCode)).append(")");
    return info.c_str();
}