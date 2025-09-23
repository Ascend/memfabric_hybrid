/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MEMFABRIC_STRING_UTIL_H
#define MEMFABRIC_STRING_UTIL_H

#include <string>
#include <vector>
#include <sstream>
#include <limits>

namespace ock {
namespace mf {
class StringUtil {
public:
    static std::string TrimString(const std::string &str);
    static inline std::vector<std::string> Split(const std::string &str, char delimiter);
    static inline bool StartsWith(const std::string &str, const std::string &prefix);
    template <typename UIntType>
    static inline bool String2Uint(const std::string& str, UIntType& val);
};

inline std::string StringUtil::TrimString(const std::string &input)
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

inline std::vector<std::string> StringUtil::Split(const std::string &str, char delimiter)
{
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }

    return result;
}

inline bool StringUtil::StartsWith(const std::string &str, const std::string &prefix)
{
    return str.length() >= prefix.length() && str.compare(0, prefix.length(), prefix) == 0;
}

template <typename UIntType>
inline bool StringUtil::String2Uint(const std::string& str, UIntType& val)
{
    if (str.empty()) {
        return false;
    }

    if (str[0] == '-') {
        return false;
    }

    char* end = nullptr;
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
}  // namespace mf
}  // namespace ock
#endif