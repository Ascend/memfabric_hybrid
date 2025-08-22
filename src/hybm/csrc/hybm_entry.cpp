/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include <cstdlib>
#include <string>
#include <thread>
#include <mutex>
#include <fstream>
#include <limits.h>

#include "hybm_version.h"
#include "hybm_common_include.h"
#include "under_api/dl_api.h"
#include "under_api/dl_acl_api.h"
#include "under_api/dl_hal_api.h"
#include "devmm_svm_gva.h"
#include "hybm_cmd.h"
#include "hybm.h"

using namespace ock::mf;

namespace {
const std::string DRIVER_VER_V2 = "V100R001C21B035";
const std::string DRIVER_VER_V1 = "V100R001C18B100";

uint64_t g_baseAddr = 0ULL;
int64_t initialized = 0;
uint16_t initedDeviceId = 0;
HybmGvaVersion checkVer = HYBM_GVA_UNKNOWN;
std::mutex initMutex;
}

int32_t HybmGetInitDeviceId()
{
    return static_cast<int32_t>(initedDeviceId);
}

bool HybmHasInited()
{
    return initialized > 0;
}

HybmGvaVersion HybmGetGvaVersion()
{
    return checkVer;
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
            if (!ock::mf::FileUtil::Realpath(tempPath)) {
                tempPath.clear();
                continue;
            }
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
    char realFile[ock::mf::FileUtil::GetSafePathMax()] = {0};
    if (ock::mf::FileUtil::IsSymlink(realName) || realpath(realName.c_str(), realFile) == nullptr) {
        BM_LOG_WARN("driver version path " << realName << " is not a valid real path");
        return "";
    }

    // realFile转str,然后open这个str
    std::ifstream infile(realFile, std::ifstream::in);
    if (!infile.is_open()) {
        BM_LOG_WARN("driver version file does not exist");
        return "";
    }

    // 逐行读取，结果放在line中，寻找带有keyStr的字符串
    std::string line;
    int32_t maxRows = 100; // 在文件中读取的最长行数为100，避免超大文件长时间读取
    while (getline(infile, line)) {
        --maxRows;
        if (maxRows < 0) {
            BM_LOG_WARN("driver version file content is too long.");
            infile.close();
            return "";
        }
        auto found = line.find(keyStr);
        // 刚好匹配前缀
        if (found == 0) {
            uint32_t len = line.length() - keyStr.length(); // 版本字符串长度
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
    found += 1;
    while (found < ver.length() && std::isdigit(ver[found])) {
        tmp += ver[found];
        ++found;
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

#ifdef UT_ENABLED
    return true;
#endif

    std::string readVer = CastDriverVersion(libPath);
    if (readVer.empty()) {
        BM_LOG_ERROR("check driver version failed, read version is empty.");
        return false;
    }

    int32_t baseVal = GetValueFromVersion(ver, "V");
    int32_t readVal = GetValueFromVersion(readVer, "V");
    if (baseVal == -1 || readVal == -1 || baseVal != readVal) {
        BM_LOG_INFO("check driver version failed, Version not equal, limit:" << ver << " read:" << readVer);
        return false;
    }

    baseVal = GetValueFromVersion(ver, "R");
    readVal = GetValueFromVersion(readVer, "R");
    if (baseVal == -1 || readVal == -1 || baseVal != readVal) {
        BM_LOG_INFO("check driver version failed, Release not equal, limit:" << ver << " read:" << readVer);
        return false;
    }

    baseVal = GetValueFromVersion(ver, "C");
    readVal = GetValueFromVersion(readVer, "C");
    if (baseVal == -1 || readVal == -1 || readVal < baseVal) {
        BM_LOG_INFO("check driver version failed, Customer is too low, limit:" << ver << " read:" << readVer);
        return false;
    }
    if (readVal > baseVal) {
        return true;
    }

    baseVal = GetValueFromVersion(ver, "B");
    readVal = GetValueFromVersion(readVer, "B");
    if (baseVal == -1 || readVal == -1 || readVal < baseVal) {
        BM_LOG_INFO("check driver version failed, Build is too low, input:" << ver << " read:" << readVer);
        return false;
    }
    return true;
}

int32_t HalGvaPrecheck(void)
{
    if (DriverVersionCheck(DRIVER_VER_V2)) {
        checkVer = HYBM_GVA_V2;
        return BM_OK;
    }
    if (DriverVersionCheck(DRIVER_VER_V1)) {
        checkVer = HYBM_GVA_V1;
        return BM_OK;
    }
    return BM_ERROR;
}

HYBM_API int32_t hybm_init(uint16_t deviceId, uint64_t flags)
{
    std::unique_lock<std::mutex> lockGuard{initMutex};
    if (initialized > 0) {
        if (initedDeviceId != deviceId) {
            BM_LOG_ERROR("this deviceId(" << deviceId << ") is not equal to the deviceId(" <<
                initedDeviceId << ") of other module!");
            return BM_ERROR;
        }

        initialized++;
        return 0;
    }

    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(HalGvaPrecheck(), "the current version of ascend driver does not support mf!");

    auto path = std::getenv("ASCEND_HOME_PATH");
    BM_VALIDATE_RETURN(path != nullptr, "Environment ASCEND_HOME_PATH not set.", BM_ERROR);

    auto libPath = std::string(path).append("/lib64");
    if (!ock::mf::FileUtil::Realpath(libPath) || !ock::mf::FileUtil::IsDir(libPath)) {
        BM_LOG_ERROR("Environment ASCEND_HOME_PATH check failed.");
        return BM_ERROR;
    }
    auto ret = DlApi::LoadLibrary(libPath);
    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "load library from path failed: " << ret);

    ret = DlAclApi::AclrtSetDevice(deviceId);
    if (ret != BM_OK) {
        DlApi::CleanupLibrary();
        BM_LOG_ERROR("set device id to be " << deviceId << " failed: " << ret);
        return BM_ERROR;
    }

    void *globalMemoryBase = nullptr;
    size_t allocSize = HYBM_DEVICE_INFO_SIZE;  // 申请meta空间
    drv::HybmInitialize(deviceId, DlHalApi::GetFd());
    ret = drv::HalGvaReserveMemory((uint64_t *)&globalMemoryBase, allocSize, (int32_t)deviceId, flags);
    if (ret != 0) {
        DlApi::CleanupLibrary();
        BM_LOG_ERROR("initialize mete memory with size: " << allocSize << ", flag: " << flags << " failed: " << ret);
        return BM_ERROR;
    }

    ret = drv::HalGvaAlloc(HYBM_DEVICE_META_ADDR, HYBM_DEVICE_INFO_SIZE, 0);
    if (ret != BM_OK) {
        DlApi::CleanupLibrary();
        int32_t hal_ret = drv::HalGvaUnreserveMemory((uint64_t)globalMemoryBase);
        BM_LOG_ERROR("HalGvaAlloc hybm meta memory failed: " << ret << ", un-reserve memory " << hal_ret);
        return BM_MALLOC_FAILED;
    }

    initedDeviceId = deviceId;
    initialized = 1L;
    g_baseAddr = (uint64_t)globalMemoryBase;
    BM_LOG_INFO("hybm init successfully, " << LIB_VERSION);
    return 0;
}

HYBM_API void hybm_uninit()
{
    std::unique_lock<std::mutex> lockGuard{initMutex};
    if (initialized <= 0L) {
        BM_LOG_WARN("hybm not initialized.");
        return;
    }

    if (--initialized > 0L) {
        return;
    }

    drv::HalGvaFree(HYBM_DEVICE_META_ADDR, HYBM_DEVICE_INFO_SIZE);
    auto ret = drv::HalGvaUnreserveMemory(g_baseAddr);
    g_baseAddr = 0ULL;
    BM_LOG_INFO("uninitialize GVA memory return: " << ret);
    DlApi::CleanupLibrary();
    initialized = 0;
}

HYBM_API void hybm_set_extern_logger(void (*logger)(int level, const char *msg))
{
    if (logger == nullptr) {
        return;
    }
    ock::mf::OutLogger::Instance().SetExternalLogFunction(logger, true);
}

HYBM_API int32_t hybm_set_log_level(int level)
{
    BM_VALIDATE_RETURN(ock::mf::OutLogger::ValidateLevel(level),
                       "set log level failed, invalid param, level should be 0~3", -1);
    ock::mf::OutLogger::Instance().SetLogLevel(static_cast<ock::mf::LogLevel>(level));
    return 0;
}

HYBM_API const char *hybm_get_error_string(int32_t errCode)
{
    static thread_local std::string info;
    info = std::string("error(").append(std::to_string(errCode)).append(")");
    return info.c_str();
}