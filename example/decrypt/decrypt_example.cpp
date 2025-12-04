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