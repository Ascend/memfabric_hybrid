/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#pragma once

#include <climits>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
#include <string>
#include <random>
#include <vector>
#include <unistd.h>
#include <unordered_map>
#include <algorithm>

namespace ock {
namespace mmc {
constexpr float EPSINON = 0.000001;

void OckTrimString(std::string &str);

// handle string function set
void SplitStr(const std::string &str, const std::string &separator, std::set<std::string> &result);
void SplitStr(const std::string &str, const std::string &separator, std::vector<std::string> &result);

inline bool OckStol(const std::string &str, long &value)
{
    char *remain = nullptr;
    errno = 0;
    value = std::strtol(str.c_str(), &remain, 10); // 10 is decimal digits
    if (remain == nullptr || strnlen(remain, PATH_MAX) > 0 ||
        ((value == LONG_MAX || value == LONG_MIN) && errno == ERANGE)) {
        return false;
    } else if (value == 0 && str != "0") {
        return false;
    }
    return true;
}

inline bool OckStoULL(const std::string &str, uint64_t &value)
{
    char *remain = nullptr;
    errno = 0;
    value = std::strtoull(str.c_str(), &remain, 10);
    if (remain == nullptr || strnlen(remain, PATH_MAX) > 0 || (value == ULLONG_MAX && errno == ERANGE)) {
        return false;
    }
    if (value == 0 && str != "0") {
        return false;
    }
    return true;
}

inline bool OckStof(const std::string &str, float &value)
{
    errno = 0;
    value = std::strtof(str.c_str(), nullptr);
    if ((value - HUGE_VALF) >= -EPSINON && (value - HUGE_VALF) <= EPSINON && errno == ERANGE) {
        return false;
    } else if ((value >= -EPSINON && value <= EPSINON) && (str != "0.0")) {
        return false;
    }
    return true;
}

const std::unordered_map<std::string, bool> Str2Bool{
    {"0", false}, {"1", true},
    {"false", false}, {"true", true}};

inline bool IsBool(const std::string &str, bool &value)
{
    std::string tmp = str;
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
    if (Str2Bool.count(tmp) <= 0) {
        return false;
    }
    value = Str2Bool.at(tmp);
    return true;
}

inline bool GetRealPath(std::string &path)
{
    char realPath[PATH_MAX + 1] = {0};
    if (path.size() > PATH_MAX || realpath(path.c_str(), realPath) == nullptr) {
        return false;
    }
    path = std::string(realPath);
    return true;
}

} // namespace mmc
} // namespace ock
