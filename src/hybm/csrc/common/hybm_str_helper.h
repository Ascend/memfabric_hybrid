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
#ifndef MF_HYBRID_HYBM_STR_HELPER_H
#define MF_HYBRID_HYBM_STR_HELPER_H

#include <cmath>
#include <sstream>
#include <vector>
#include <string>
#include <limits.h>

namespace ock {
namespace mf {
constexpr float EPSINON = 0.000001;
constexpr int DECIMAL_DIGITS = 10;
class StrHelper {
public:
    static inline std::vector<std::string> Split(const std::string& str, char delimiter)
    {
        std::vector<std::string> result;
        std::stringstream ss(str);
        std::string item;

        while (std::getline(ss, item, delimiter)) {
            result.push_back(item);
        }

        return result;
    }

    static inline bool StartsWith(const std::string& str, const std::string &prefix)
    {
        return str.length() >= prefix.length() &&
               str.compare(0, prefix.length(), prefix) == 0;
    }

    static inline bool OckStol(const std::string &str, long &value)
    {
        char *remain = nullptr;
        errno = 0;
        value = std::strtol(str.c_str(), &remain, 10); // 10 is decimal digits
        if (remain == nullptr || strnlen(remain, PATH_MAX) > 0 ||
            ((value == LONG_MAX || value == LONG_MIN) && errno == ERANGE)) {
            return false;
        }
        if (value == 0 && str != "0") {
            return false;
        }
        return true;
    }

    static inline bool OckStol(const std::string &str, int &value)
    {
        long value0;
        if (!OckStol(str, value0)) {
            return false;
        }
        value = static_cast<int>(value0);
        return true;
    }

    static inline bool OckStoULL(const std::string &str, uint64_t &value)
    {
        char *remain = nullptr;
        errno = 0;
        value = std::strtoull(str.c_str(), &remain, DECIMAL_DIGITS);
        if (remain == nullptr || strnlen(remain, PATH_MAX) > 0 || (value == ULLONG_MAX && errno == ERANGE)) {
            return false;
        }
        if (value == 0 && str != "0") {
            return false;
        }
        return true;
    }

    static inline bool OckStoULL(const std::string &str, uint32_t &value)
    {
        uint64_t value0;
        if (!OckStoULL(str, value0)) {
            return false;
        }
        value = static_cast<uint32_t>(value0);
        return true;
    }

    static inline bool OckStof(const std::string &str, float &value)
    {
        errno = 0;
        value = std::strtof(str.c_str(), nullptr);
        if ((value - HUGE_VALF) >= -EPSINON && (value - HUGE_VALF) <= EPSINON && errno == ERANGE) {
            return false;
        }
        if ((value >= -EPSINON && value <= EPSINON) && (str != "0.0")) {
            return false;
        }
        return true;
    }
};
}
}

#endif // MF_HYBRID_HYBM_STR_HELPER_H