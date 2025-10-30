/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MEMFABRIC_NUM_UTIL_H
#define MEMFABRIC_NUM_UTIL_H

#include <type_traits>
#include <limits>
#include <string>
#include <cctype>

enum MfIndex : uint8_t {
    INDEX_0 = 0U,
    INDEX_1 = 1U,
    INDEX_2 = 2U,
    INDEX_3 = 3U,
    INDEX_4 = 4U,
    INDEX_5 = 5U,
    INDEX_6 = 6U,
};

namespace ock {
namespace mf {
template <typename T>
struct IsUnsignedNumber {
    static constexpr bool value =
        std::is_same<T, unsigned short>::value ||
        std::is_same<T, unsigned int>::value ||
        std::is_same<T, unsigned long>::value ||
        std::is_same<T, unsigned long long>::value;
};

class NumUtil {
public:
    /**
     * @brief Check whether an arithmetic operation will overflow
     *
     * checks potential overflow in addition and multiplication
     *
     * @tparam T      Numeric type (integral)
     * @param a       [in] first operand
     * @param b       [in] second operand
     * @param calc    [in] operation type: '+' for addition, '*' for multiplication
     * @return true if the operation overflow, false otherwise
     */
    template <typename T>
    static bool IsOverflowCheck(T a, T b, T max, char calc);

    /**
     * @brief Check whether the input string is all digits
     *
     * @param str input string
     * @return true if the input is all digits else false
     */
    static bool IsDigit(const std::string& str);
};

template <typename T>
inline bool NumUtil::IsOverflowCheck(T a, T b, T max, char calc)
{
    if (!(IsUnsignedNumber<T>::value)) {
        return false;
    }
    switch (calc) {
        case '+':
            return (a > max - b);
        case '*':
            return ((b != 0) && (a > max / b));
        default:
            return true;
    }
}

inline bool NumUtil::IsDigit(const std::string& str)
{
    if (str.empty()) {
        return false;
    }
    size_t start = str.find_first_not_of(" \t");
    if (start == std::string::npos) {
        return false;
    }

    for (size_t i = start; i < str.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(str[i]))) {
            return false;
        }
    }
    return true;
}
}
}
#endif