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
#ifndef __MEMFABRIC_SMEM_H__
#define __MEMFABRIC_SMEM_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ASYNC_COPY_FLAG                     (1UL << (0))
#define COPY_EXTEND_FLAG                    (1UL << (1))
#define SMEM_WORLD_SIZE_MAX                 1024U
#define SMEM_INVALID_DEV_ID                 (-1)

/**
 * @brief Initialize the smem running environment
 *
 * @param flags            [in] optional flags, reserved
 * @return 0 if successful,
 */
int32_t smem_init(uint32_t flags);

/**
 * @brief Create configure store server for SMEM used.
 *
 * @param storeURL         [in] configure store url for control, e.g. tcp:://ip:port
 * @return 0 if successful
 */
int32_t smem_create_config_store(const char *storeUrl);

/**
 * @brief Set external log function, user can set customized logger function,
 * in the customized logger function, user can use unified logger utility,
 * then the log message can be written into the same log file as caller's,
 * if it is not set, acc_links log message will be printed to stdout.
 *
 * level description:
 * 0 DEBUG,
 * 1 INFO,
 * 2 WARN,
 * 3 ERROR
 *
 * @param func             [in] external logger function
 * @return 0 if successful
 */
int32_t smem_set_extern_logger(void (*func)(int level, const char *msg));

/**
 * @brief Set log level
 *
 * @param level            [in] log level, 0:debug 1:info 2:warn 3:error
 * @return 0 if successful
 */
int32_t smem_set_log_level(int level);

/**
 * @brief Set TLS related for config store
 *
 * @param enable whether to enable tls
 * @param tls_info the format describle in memfabric SECURITYNOTE.md, if disabled tls_info won't be use
 * @param tls_info_len length of tls_info, if disabled tls_info_len won't be use
 * @return Returns 0 on success or an error code on failure
 */
int32_t smem_set_conf_store_tls(bool enable, const char *tls_info, const uint32_t tls_info_len);

/**
 * @brief Callback function definition of get private key password de-crypted, see smem_set_config_store_tls_key
 *
 * @param cipherText       [in] the encrypted text(private key password)
 * @param cipherTextLen    [in] the length of encrypted text
 * @param plainText        [out] the decrypted text(private key password)
 * @param plaintextLen     [out] the length of plainText
 */
typedef int (*smem_decrypt_handler)(const char *cipherText, size_t cipherTextLen, char *plainText,
                                    size_t &plainTextLen);

/**
 * @brief Set the TLS private key and password
 *
 * @param tls_pk          [in] content of tls private key
 * @param tls_pk_len      [in] size of tls private key
 * @param tls_pk_pw       [in] content of tls private key password
 * @param tls_pk_pw_len   [in] size of tls private key password
 * @param h               [in] handler
 */
int32_t smem_set_config_store_tls_key(const char *tls_pk, const uint32_t tls_pk_len, const char *tls_pk_pw,
                                      const uint32_t tls_pk_pw_len, const smem_decrypt_handler h);

/**
 * @brief Un-Initialize the smem running environment
 */
void smem_uninit(void);

/**
 * @brief Get the last error message
 *
 * @return last error message
 */
const char *smem_get_last_err_msg(void);

/**
 * @brief Get the last error message and clear
 *
 * @return last error message
 */
const char *smem_get_and_clear_last_err_msg(void);

#ifdef __cplusplus
}
#endif

#endif // __MEMFABRIC_SMEM_H__
