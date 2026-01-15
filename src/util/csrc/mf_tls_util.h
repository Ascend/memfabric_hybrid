/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef MEMFABRIC_HYBRID_TLS_UTIL_H
#define MEMFABRIC_HYBRID_TLS_UTIL_H

#include <algorithm>
#include <cstdint>
#include <dlfcn.h>

namespace ock {
namespace mf {

using DecryptFunc = int (*)(const char *cipherText, const size_t cipherTextLen, char *plainText, size_t plainTextLen);

class MfTlsUtil {
public:
    static inline int32_t DefaultDecrypter(const char *cipherText, const size_t cipherTextLen, char *plainText,
                                           const size_t plainTextLen)
    {
        std::copy_n(cipherText, plainTextLen, plainText);
        return 0;
    }

    static inline void **GetTlsLibHandler()
    {
        static void *decryptLibHandle = nullptr;
        return &decryptLibHandle;
    }

    static inline DecryptFunc LoadDecryptFunction(const char *decrypterLibPath)
    {
        void **decryptLibHandlePtr = GetTlsLibHandler();
        if (*decryptLibHandlePtr == nullptr) {
            *decryptLibHandlePtr = dlopen(decrypterLibPath, RTLD_LAZY);
        }
        if (*decryptLibHandlePtr != nullptr) {
            const auto decryptFunc = (DecryptFunc)dlsym(*decryptLibHandlePtr, "DecryptPassword");
            if (decryptFunc != nullptr) {
                return decryptFunc;
            } else {
                CloseTlsLib();
                return nullptr;
            }
        }
        return nullptr;
    }

    static inline void CloseTlsLib()
    {
        void **decryptLibHandlePtr = GetTlsLibHandler();
        if (*decryptLibHandlePtr != nullptr) {
            dlclose(*decryptLibHandlePtr);
            decryptLibHandlePtr = nullptr;
        }
    }
};
} // namespace mf
} // namespace ock

#endif // MEMFABRIC_HYBRID_TLS_UTIL_H