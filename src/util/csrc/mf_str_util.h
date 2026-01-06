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
#ifndef MEMFABRIC_HYBRID_STR_UTIL_H
#define MEMFABRIC_HYBRID_STR_UTIL_H

#include <string>
#include <vector>
#include <sstream>
#include <limits>
#include <chrono>

namespace ock {
namespace mf {
static const std::string ipv6_common_core = R"((?:[0-9a-fA-F]{1,4}(?::[0-9a-fA-F]{1,4}){7})|)"
                                            R"((?:[0-9a-fA-F]{1,4}(?::[0-9a-fA-F]{1,4}){0,6})?::)"
                                            R"((?:[0-9a-fA-F]{1,4}(?::[0-9a-fA-F]{1,4}){0,6})?)";
class StrUtil {
public:
    static std::string StrTrim(const std::string &str);
    static inline std::vector<std::string> Split(const std::string &str, char delimiter);
    static inline bool StartWith(const std::string &str, const std::string &prefix);
    template<typename UIntType>
    static inline bool String2Uint(const std::string &str, UIntType &val);
    template<typename IntType>
    static inline bool String2Int(const std::string &str, IntType &val);
    static inline int64_t GetNowTime();
};

inline std::string StrUtil::StrTrim(const std::string &input)
{
    if (input.empty()) {
        return "";
    }
    auto start = input.begin();
    while (start != input.end() && std::isspace(*start)) {
        start++;
    }

    auto end = input.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));

    return std::string(start, end + 1);
}

inline std::vector<std::string> StrUtil::Split(const std::string &str, char delimiter)
{
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }

    return result;
}

inline bool StrUtil::StartWith(const std::string &str, const std::string &prefix)
{
    return str.length() >= prefix.length() && str.compare(0, prefix.length(), prefix) == 0;
}

template<typename UIntType>
inline bool StrUtil::String2Uint(const std::string &str, UIntType &val)
{
    if (str.empty()) {
        return false;
    }

    if (str[0] == '-') {
        return false;
    }

    char *end = nullptr;
    errno = 0;

    unsigned long long result = std::strtoull(str.c_str(), &end, 10);

    if (end == str.c_str()) {
        return false;
    }

    while (*end != '\0' && std::isspace(*end)) {
        ++end;
    }
    if (*end != '\0') {
        return false;
    }

    if (errno == ERANGE || result > static_cast<unsigned long long>(std::numeric_limits<UIntType>::max())) {
        return false;
    }

    val = static_cast<UIntType>(result);
    return true;
}

template<typename IntType>
inline bool StrUtil::String2Int(const std::string &str, IntType &val)
{
    if (str.empty()) {
        return false;
    }

    char *end = nullptr;
    errno = 0;

    int64_t result = std::strtoll(str.c_str(), &end, 10);

    if (end == str.c_str()) {
        return false;
    }

    while (*end != '\0' && std::isspace(*end)) {
        ++end;
    }
    if (*end != '\0') {
        return false;
    }

    if (errno == ERANGE || result > static_cast<int64_t>(std::numeric_limits<IntType>::max())) {
        return false;
    }

    val = static_cast<IntType>(result);
    return true;
}

inline int64_t StrUtil::GetNowTime()
{
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto value = now_ms.time_since_epoch().count();
    return static_cast<int64_t>(value);
}
} // namespace mf
} // namespace ock

#endif // MEMFABRIC_HYBRID_STR_UTIL_H
