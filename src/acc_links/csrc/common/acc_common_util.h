/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACC_LINKS_ACC_COMMON_UTIL_H
#define ACC_LINKS_ACC_COMMON_UTIL_H

#include <cstdint>
#include <iostream>
#include <regex>

#include "acc_includes.h"
#include "mf_file_util.h"
#include "openssl_api_wrapper.h"

namespace ock {
namespace acc {
class AccCommonUtil {
public:
    static bool IsValidIPv4(const std::string &ip);
    static Result SslShutdownHelper(SSL *s);
    static uint32_t GetEnvValue2Uint32(const char *envName);
    static bool IsAllDigits(const std::string &str);
    static Result CheckTlsOptions(const AccTlsOption &tlsOption);
};
}  // namespace acc
}  // namespace ock

#endif  // ACC_LINKS_ACC_COMMON_UTIL_H
