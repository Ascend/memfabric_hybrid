/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "mmc_functions.h"

#include <string>

namespace ock {
namespace mmc {
void OckTrimString(std::string &str)
{
    if (str.empty()) {
        return;
    }

    str.erase(0, str.find_first_not_of(" \t\n\r"));
    str.erase(str.find_last_not_of(" \t\n\r") + 1);
}

void SplitStr(const std::string &str, const std::string &separator, std::set<std::string> &result)
{
    if (separator.empty()) {
        if (str.empty()) {
            return;
        }
        result.insert(str);
        return;
    }
    std::string::size_type pos1 = 0;
    std::string::size_type pos2 = str.find(separator);

    std::string tmpStr;
    while (pos2 != std::string::npos) {
        tmpStr = str.substr(pos1, pos2 - pos1);
        // trim the string, i.e. remove space at the begin and end position
        OckTrimString(tmpStr);
        result.insert(tmpStr);

        pos1 = pos2 + separator.size();
        pos2 = str.find(separator, pos1);
    }

    if (pos1 != str.length()) {
        tmpStr = str.substr(pos1);
        // trim the string, i.e. remove space at the begin and end position
        OckTrimString(tmpStr);
        result.insert(tmpStr);
    }
}

void SplitStr(const std::string &str, const std::string &separator, std::vector<std::string> &result)
{
    if (separator.empty()) {
        if (str.empty()) {
            return;
        }
        result.emplace_back(str);
        return;
    }
    std::string::size_type pos1 = 0;
    std::string::size_type pos2 = str.find(separator);

    std::string tmpStr;
    while (pos2 != std::string::npos) {
        tmpStr = str.substr(pos1, pos2 - pos1);
        OckTrimString(tmpStr);
        result.emplace_back(tmpStr);
        pos1 = pos2 + separator.size();
        pos2 = str.find(separator, pos1);
    }

    if (pos1 != str.length()) {
        tmpStr = str.substr(pos1);
        OckTrimString(tmpStr);
        result.emplace_back(tmpStr);
    }
}

} // namespace common
} // namespace ock