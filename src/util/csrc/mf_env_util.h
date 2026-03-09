/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MEMFABRIC_HYBRID_HYBM_ENV_UTIL_H
#define MEMFABRIC_HYBRID_HYBM_ENV_UTIL_H

#include <cstdlib>
#include <string>

#include "mf_str_util.h"

namespace ock {
namespace mf {
class MfEnvUtil {
public:
    template<typename UIntType>
    static bool GetUint(const char *envName, UIntType &value)
    {
        const char *raw = std::getenv(envName);
        if (raw == nullptr) {
            return false;
        }

        UIntType parsedValue = 0;
        std::string str(raw);
        if (!StrUtil::String2Uint(str, parsedValue) || parsedValue == 0) {
            return false;
        }
        value = parsedValue;
        return true;
    }

    template<typename UIntType>
    static bool GetOptionalUint(const char *envName, UIntType &value)
    {
        const char *raw = std::getenv(envName);
        if (raw == nullptr) {
            return false;
        }

        UIntType parsedValue = 0;
        std::string str(raw);
        if (!StrUtil::String2Uint(str, parsedValue)) {
            return false;
        }
        value = parsedValue;
        return true;
    }

    template<typename UIntType>
    static UIntType GetUintOrDefault(const char *envName, UIntType defaultValue)
    {
        UIntType value = defaultValue;
        if (!GetUint(envName, value)) {
            return defaultValue;
        }
        return value;
    }

    template<typename UIntType>
    static UIntType GetOptionalUintOrDefault(const char *envName, UIntType defaultValue)
    {
        UIntType value = defaultValue;
        if (!GetOptionalUint(envName, value)) {
            return defaultValue;
        }
        return value;
    }
};
} // namespace mf
} // namespace ock

#endif // MEMFABRIC_HYBRID_HYBM_ENV_UTIL_H
