/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <unistd.h>
#include <regex>

#include "mf_str_util.h"
#include "mf_file_util.h"
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

bool AccCommonUtil::IsValidIPv6(const std::string &ip)
{
    constexpr size_t maxIpv6Len = 39;
    if (ip.size() > maxIpv6Len) {
        return false;
    }

    const std::regex ipV6Pattern(
        "^" + ock::mf::ipv6_common_core + "$"
    );

    return std::regex_match(ip, ipV6Pattern);
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
        uint32_t envValue = 0;
        std::string str(tmpEnvValue);
        if (!ock::mf::StrUtil::String2Uint(str, envValue)) {
            LOG_ERROR("failed to convert str : " << str << " to uint32_t");
            return 0;
        }
        return envValue;
    }
    return 0;
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

#define CHECK_FILE_PATH_TLS(key, path)                                                         \
    do {                                                                                       \
        if (ock::mf::FileUtil::IsSymlink(path) || !ock::mf::FileUtil::Realpath(path)           \
            || !ock::mf::FileUtil::IsFile(path) || !ock::mf::FileUtil::CheckFileSize(path)) {  \
            LOG_ERROR("TLS " #key " check failed");                                            \
            return ACC_ERROR;                                                                  \
        }                                                                                      \
    } while (0)

#define CHECK_FILE_PATH(key, required)                                           \
    do {                                                                         \
        if (!tlsOption.key.empty()) {                                            \
            std::string path = tlsOption.tlsTopPath + "/" + tlsOption.key;       \
            CHECK_FILE_PATH_TLS(key, path);                                      \
        } else if (required) {                                                   \
            LOG_ERROR("TLS check failed, " #key " is required");                 \
            return ACC_ERROR;                                                    \
        }                                                                        \
    } while (0)

#define CHECK_DIR_PATH_TLS(key, path)                                                    \
    do {                                                                                 \
        if (ock::mf::FileUtil::IsSymlink(path) || !ock::mf::FileUtil::Realpath(path)     \
            || !ock::mf::FileUtil::IsDir(path)) {                                        \
            LOG_ERROR("TLS " #key " check failed");                                      \
            return ACC_ERROR;                                                            \
        }                                                                                \
    } while (0)

#define CHECK_DIR_PATH(key, required)                                                                               \
    do {                                                                                                            \
        if (!tlsOption.key.empty()) {                                                                               \
            std::string path = (#key == "tlsTopPath") ? tlsOption.key : tlsOption.tlsTopPath + "/" + tlsOption.key; \
            CHECK_DIR_PATH_TLS(key, path);                                                                          \
        } else if (required) {                                                                                      \
            LOG_ERROR("TLS check failed, " #key " is required");                                                    \
            return ACC_ERROR;                                                                                       \
        }                                                                                                           \
    } while (0)

#define CHECK_FILE_SET_TLS(key, topPath)                                                 \
    do {                                                                                 \
        for (const std::string &file : tlsOption.key) {                                  \
            std::string filePath = (topPath) + "/" + (file);                             \
            CHECK_FILE_PATH_TLS(key, filePath);                                          \
        }                                                                                \
    } while (0)

#define CHECK_FILE_SET(key, topPath, required)                                               \
    do {                                                                                     \
        if (!tlsOption.key.empty()) {                                                        \
            CHECK_FILE_SET_TLS(key, topPath);                                                \
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
    CHECK_FILE_SET(tlsCaFile, tlsOption.tlsTopPath + "/" + tlsOption.tlsCaPath, true);
    CHECK_FILE_SET(tlsCrlFile, tlsOption.tlsTopPath + "/" + tlsOption.tlsCrlPath, false);
    return ACC_OK;
}
}  // namespace acc
}  // namespace ock
