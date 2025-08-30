/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEMFABRIC_STRING_UTIL_H
#define MEMFABRIC_STRING_UTIL_H

#include <string>
#include <vector>
#include <sstream>

namespace ock {
namespace mf {
class StringUtil {
public:
    static std::string TrimString(const std::string &str);
    static inline std::vector<std::string> Split(const std::string &str, char delimiter);
    static inline bool StartsWith(const std::string &str, const std::string &prefix);
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
}  // namespace mf
}  // namespace ock
#endif