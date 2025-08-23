/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEMFABRIC_STRING_UTIL_H
#define MEMFABRIC_STRING_UTIL_H

#include <string>

namespace ock {
namespace mf {
class StringUtil {
    
public:
    static std::string TrimString(const std::string& str);

};

inline std::string StringUtil::TrimString(const std::string &input)
{
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
}
}
#endif