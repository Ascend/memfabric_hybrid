/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef ACC_LINKS_ACC_COMMON_UTIL_H
#define ACC_LINKS_ACC_COMMON_UTIL_H

#include <cstdint>
#include <iostream>
#include <regex>

#include "acc_includes.h"
#include "openssl_api_wrapper.h"

namespace ock {
namespace acc {
class AccCommonUtil {
public:
    static bool IsValidIPv4(const std::string &ip);
    static Result SslShutdownHelper(SSL *s);
    static uint32_t GetEnvValue2Uint32(const char *envName);
    static uint32_t String2Uint(const char *str);
    static bool IsAllDigits(const std::string &str);
};
}  // namespace acc
}  // namespace ock

#endif  // ACC_LINKS_ACC_COMMON_UTIL_H
