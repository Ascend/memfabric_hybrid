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
 * @brief Callback function of private key password decryptor, see smem_set_config_store_tls_key
 *
 * @param cipherText       [in] the encrypted text(private password)
 * @param cipherTextLen    [in] the length of encrypted text
 * @param plainText        [out] the decrypted text(private password)
 * @param plaintextLen     [out] the length of plainText
 */
typedef int (*smem_decrypt_handler)(const char *cipherText, size_t cipherTextLen, char *plainText, size_t &plainTextLen);

/**
 * @brief Set the TLS private key and password.
 *
 * @param tls_pk          [in] content of tls private key
 * @param tls_pk_len      [in] size of tls private key
 * @param tls_pk_pw       [in] content of tls private key password
 * @param tls_pk_pw_len   [in] size of tls private key password
 * @param h               [in] handler
 */
int32_t smem_set_config_store_tls_key(const char *tls_pk, const uint32_t tls_pk_len, const char *tls_pk_pw,
    const uint32_t tls_pk_pw_len, const smem_decrypt_handler h);

#ifdef __cplusplus
}
#endif

#endif