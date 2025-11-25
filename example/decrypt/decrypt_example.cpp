/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include <string>
#include <iostream>
#include <algorithm>

#ifdef __cplusplus
extern "C" {
#endif

int DecryptPassword(const char* cipherText, const size_t cipherTextLen, char* plainText, const int32_t plainTextLen)
{
    if (plainText == nullptr || cipherText == nullptr) {
        return -1;
    }

    auto text = std::string(cipherText, cipherTextLen);
    std::reverse(text.begin(), text.end());
    std::copy_n(text.c_str(), plainTextLen, plainText);
    
    return 0; // 成功
}

#ifdef __cplusplus
}
#endif