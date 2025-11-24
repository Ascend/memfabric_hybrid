/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include <fstream>
#include <string>

#include "hybm_logger.h"
#include "hybm_str_helper.h"
#include "hybm_types.h"

#include "hybm_gva_version.h"

namespace ock {
namespace mf {
const std::string DRIVER_VER_V4 = "V100R001C23B035";
const std::string DRIVER_VER_V3 = "V100R001C21B035";
const std::string DRIVER_VER_V2 = "V100R001C19SPC109B220";
const std::string DRIVER_VER_V1 = "V100R001C18B100";

HybmGvaVersion checkVer = HYBM_GVA_UNKNOWN;

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
        BM_LOG_WARN("driver version path is not a valid real path.");
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
    BM_LOG_WARN("cannot found version file in driverEnv.");
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
    val = -1;
    auto ret = StrHelper::OckStol(tmp, val);
    if (!ret) {
        BM_LOG_ERROR("tmp=" << tmp << ", val=" << val);
    }
    return val;
}

static bool DriverVersionCheck(const std::string &ver)
{
#ifndef USE_CANN
    return true;
#else
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
        BM_LOG_WARN("check driver version failed, Version not equal, limit:" << ver << " read:" << readVer);
        return false;
    }

    baseVal = GetValueFromVersion(ver, "R");
    readVal = GetValueFromVersion(readVer, "R");
    if (baseVal == -1 || readVal == -1 || baseVal != readVal) {
        BM_LOG_WARN("check driver version failed, Release not equal, limit:" << ver << " read:" << readVer);
        return false;
    }

    baseVal = GetValueFromVersion(ver, "C");
    readVal = GetValueFromVersion(readVer, "C");
    if (baseVal == -1 || readVal == -1 || readVal < baseVal) {
        BM_LOG_WARN("check driver version failed, Customer is too low, limit:" << ver << " read:" << readVer);
        return false;
    }
    if (readVal > baseVal) {
        return true;
    }

    baseVal = GetValueFromVersion(ver, "B");
    readVal = GetValueFromVersion(readVer, "B");
    if (baseVal == -1 || readVal == -1 || readVal < baseVal) {
        BM_LOG_WARN("check driver version failed, Build is too low, input:" << ver << " read:" << readVer);
        return false;
    }
    return true;
#endif
}

int32_t HalGvaPrecheck()
{
#ifdef USE_VMM
    checkVer = HYBM_GVA_V4;
    return BM_OK;
#endif
    if (DriverVersionCheck(DRIVER_VER_V4)) {
        BM_LOG_INFO("Driver version V4 found");
        checkVer = HYBM_GVA_V4;
        return BM_OK;
    }
    if (DriverVersionCheck(DRIVER_VER_V3)) {
        BM_LOG_INFO("Driver version V3 found");
        checkVer = HYBM_GVA_V3;
        return BM_OK;
    }
    if (DriverVersionCheck(DRIVER_VER_V2)) {
        BM_LOG_INFO("Driver version V2 found");
        checkVer = HYBM_GVA_V2;
        return BM_OK;
    }
    if (DriverVersionCheck(DRIVER_VER_V1)) {
        BM_LOG_INFO("Driver version V1 found");
        checkVer = HYBM_GVA_V1;
        return BM_OK;
    }
    BM_LOG_ERROR("Failed to determine driver version");
    return BM_ERROR;
}

}
}