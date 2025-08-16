/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#ifndef __MEMFABRIC_SMEM_SECURITY_H__
#define __MEMFABRIC_SMEM_SECURITY_H__

#include <string>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback function of private key password decryptor, see smem_register_decrypt_handler
 *
 * @param cipherText       [in] the encrypted text(private password)
 * @param cipherTextLen    [in] the length of encrypted text
 * @param plainText        [out] the decrypted text(private password)
 * @param plaintextLen     [out] the length of plainText
 */
typedef int (*smem_decrypt_handler)(const char *cipherText, size_t cipherTextLen, char *plainText, size_t &plainTextLen);

/**
 * @brief Register the handler for decryption of private key password.
 * If the private key is encrypted, this hanlder is needed to be set.
 *
 * @param h            [in] handler
 */
int32_t smem_register_decrypt_handler(const smem_decrypt_handler h);

#ifdef __cplusplus
}
#endif

#endif