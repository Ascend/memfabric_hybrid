/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEMFABRIC_NUM_UTIL_H
#define MEMFABRIC_NUM_UTIL_H

#include <type_traits>
#include <limits>
#include <string>
#include <cctype>
#include <cstdint>

namespace ock {
namespace mf {
template<typename T>
struct IsUnsignedNumber {
    static constexpr bool value = std::is_same<T, unsigned short>::value || std::is_same<T, unsigned int>::value ||
                                  std::is_same<T, unsigned long>::value || std::is_same<T, unsigned long long>::value;
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
    template<typename T>
    static bool IsOverflowCheck(T a, T b, T max, char calc);

    /**
     * @brief Check whether the input string is all digits
     *
     * @param str input string
     * @return true if the input is all digits else false
     */
    static bool IsDigit(const std::string &str);

    /**
    * Extracts a bit field from the given flag value.
    *
    * @param flag        The 32-bit source value.
    * @param startBit    The starting bit position (0-based, 0 = least significant bit).
    * @param bitLength   The number of bits to extract (1 to 32).
    * @return            extraction flags value
    */
    static uint32_t ExtractBits(uint32_t flag, uint8_t startBit, uint8_t bitLength)
    {
        if (startBit >= UINT32_WIDTH) {
            return UINT32_MAX;
        }
        if (bitLength == 0 || bitLength >= UINT32_WIDTH) {
            return UINT32_MAX;
        }
        if (startBit + bitLength > UINT32_WIDTH) {
            return UINT32_MAX;
        }

        return (flag >> startBit) & ((1U << bitLength) - 1);
    }
};

template<typename T>
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

inline bool NumUtil::IsDigit(const std::string &str)
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
} // namespace mf
} // namespace ock
#endif