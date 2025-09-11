/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef MEMFABRIC_HYBRID_TLS_UTIL_H
#define MEMFABRIC_HYBRID_TLS_UTIL_H

#include <algorithm>
#include <cstdint>
#include <dlfcn.h>

#define PATH_MAX_SIZE 1024

namespace ock {
namespace mf {

using DecryptFunc = int (*)(const char* cipherText, const size_t cipherTextLen, char* plainText, size_t plainTextLen);

inline int32_t DefaultDecrypter(const char* cipherText, const size_t cipherTextLen,
                                char* plainText, const int32_t plainTextLen)
{
    std::copy_n(cipherText, plainTextLen, plainText);
    return 0;
}

inline DecryptFunc LoadDecryptFunction(const char* decrypterLibPath)
{
    void* decryptLibHandle = dlopen(decrypterLibPath, RTLD_LAZY);
    if (decryptLibHandle != nullptr) {
        const auto decryptFunc = (DecryptFunc)dlsym(decryptLibHandle, "DecryptPassword");
        if (decryptFunc != nullptr) {
            return decryptFunc;
        } else {
            dlclose(decryptLibHandle);
            return nullptr;
        }
    }

    return nullptr;
}

}
}

#endif // MEMFABRIC_HYBRID_TLS_UTIL_H