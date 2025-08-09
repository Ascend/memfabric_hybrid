/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <unistd.h>

#include "acc_includes.h"
#include "acc_common_util.h"

namespace ock {
namespace acc {
bool AccCommonUtil::IsValidIPv4(const std::string &ip)
{
    constexpr size_t maxIpv4Len = 15;
    if (ip.size() > maxIpv4Len) {
        return false;
    }
    std::regex ipv4Regex("^(?:(?:25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)($|(?!\\.$)\\.)){4}$");
    return std::regex_match(ip, ipv4Regex);
}

Result AccCommonUtil::SslShutdownHelper(SSL *ssl)
{
    if (!ssl) {
        LOG_ERROR("ssl ptr is nullptr");
        return ACC_ERROR;
    }

    const int sslShutdownTimes = 5;
    const int sslRetryInterval = 1;  // s
    int ret = OpenSslApiWrapper::SslShutdown(ssl);
    if (ret == 1) {
        return ACC_OK;
    } else if (ret < 0) {
        ret = OpenSslApiWrapper::SslGetError(ssl, ret);
        LOG_ERROR("ssl shutdown failed!, error code is:" << ret);
        return ACC_ERROR;
    } else if (ret != 0) {
        LOG_ERROR("unknown ssl shutdown ret val!");
        return ACC_ERROR;
    }

    for (int i = UNO_1; i <= sslShutdownTimes; ++i) {
        sleep(sslRetryInterval);
        LOG_INFO("ssl showdown retry times:" << i);
        ret = OpenSslApiWrapper::SslShutdown(ssl);
        if (ret == 1) {
            return ACC_OK;
        } else if (ret < 0) {
            LOG_ERROR("ssl shutdown failed!, error code is:" << OpenSslApiWrapper::SslGetError(ssl, ret));
            return ACC_ERROR;
        } else if (ret != 0) {
            LOG_ERROR("unknown ssl shutdown ret val!");
            return ACC_ERROR;
        }
    }
    return ACC_ERROR;
}

uint32_t AccCommonUtil::GetEnvValue2Uint32(const char *envName)
{
    // 0 should be illegal for this env variable
    constexpr uint32_t maxUint32Len = 35;
    const char *tmpEnvValue = std::getenv(envName);
    if (tmpEnvValue != nullptr && strlen(tmpEnvValue) <= maxUint32Len && IsAllDigits(tmpEnvValue)) {
        uint32_t envValue = String2Uint(tmpEnvValue);
        return envValue;
    }
    return 0;
}

uint32_t AccCommonUtil::String2Uint(const char *str)    // avoid throwing ex during converting to std::string
{
    try {
        auto num = static_cast<uint32_t>(std::stoi(str));
        return num;
    } catch (...) {
        return 0;
    }
}

bool AccCommonUtil::IsAllDigits(const std::string &str)
{
    if (str.empty()) {
        return false;
    }
    return std::all_of(str.begin(), str.end(), [](unsigned char ch) {
        return std::isdigit(ch);
    });
}

#define CHECK_FILE_PATH(key, required)                                           \
    do {                                                                         \
        if (!tlsOption.key.empty()) {                                            \
            std::string path = tlsOption.tlsTopPath + tlsOption.key;             \
            if (FileUtil::IsSymlink(path) || !FileUtil::Realpath(path)           \
                || !FileUtil::IsFile(path) || !FileUtil::CheckFileSize(path)) {  \
                LOG_ERROR("TLS " #key " check failed");                          \
                return ACC_ERROR;                                                \
            }                                                                    \
        } else if (required) {                                                   \
            LOG_ERROR("TLS check failed, " #key " is required");                 \
            return ACC_ERROR;                                                    \
        }                                                                        \
    } while (0)

#define CHECK_DIR_PATH(key, required)                                                                               \
    do {                                                                                                            \
        if (!tlsOption.key.empty()) {                                                                               \
            std::string path = (#key == "tlsTopPath") ? tlsOption.key : tlsOption.tlsTopPath + "/" + tlsOption.key; \
            if (FileUtil::IsSymlink(path) || !FileUtil::Realpath(path) || !FileUtil::IsDir(path)) {                 \
                LOG_ERROR("TLS " #key " check failed");                                                             \
                return ACC_ERROR;                                                                                   \
            }                                                                                                       \
        } else if (required) {                                                                                      \
            LOG_ERROR("TLS check failed, " #key " is required");                                                    \
            return ACC_ERROR;                                                                                       \
        }                                                                                                           \
    } while (0)

#define CHECK_FILE_SET(key, topPath, required)                                               \
    do {                                                                                     \
        if (!tlsOption.key.empty()) {                                                        \
            for (const std::string &file : tlsOption.key) {                                  \
                std::string filePath = (topPath) + "/" + (file);                             \
                if (FileUtil::IsSymlink(filePath) || !FileUtil::Realpath(filePath)           \
                    || !FileUtil::IsFile(filePath) || !FileUtil::CheckFileSize(filePath)) {  \
                    LOG_ERROR("TLS " #key " check failed");                                  \
                    return ACC_ERROR;                                                        \
                }                                                                            \
            }                                                                                \
        } else if (required) {                                                               \
            LOG_ERROR("TLS check failed, " #key " is required");                             \
            return ACC_ERROR;                                                                \
        }                                                                                    \
    } while (0)

Result AccCommonUtil::CheckTlsOptions(const AccTlsOption &tlsOption)
{
    if (!tlsOption.enableTls) {
        return ACC_OK;
    }
    CHECK_DIR_PATH(tlsTopPath, false);
    CHECK_DIR_PATH(tlsCaPath, true);
    CHECK_DIR_PATH(tlsCrlPath, false);
    CHECK_FILE_PATH(tlsCert, true);
    CHECK_FILE_PATH(tlsPk, true);
    CHECK_FILE_PATH(tlsPkPwd, false);
    CHECK_FILE_SET(tlsCaFile, tlsOption.tlsTopPath + "/" + tlsOption.tlsCaPath, true);
    CHECK_FILE_SET(tlsCrlFile, tlsOption.tlsTopPath + "/" + tlsOption.tlsCrlPath, false);
    return ACC_OK;
}
}  // namespace acc
}  // namespace ock
