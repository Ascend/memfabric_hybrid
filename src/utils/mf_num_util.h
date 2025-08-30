/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEMFABRIC_NUM_UTIL_H
#define MEMFABRIC_NUM_UTIL_H

#include <type_traits>
#include <limits>

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
}
}
#endif