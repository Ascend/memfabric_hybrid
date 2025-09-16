/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef __MF_TLS_DEF_H__
#define __MF_TLS_DEF_H__

#define TLS_PATH_SIZE 256
#define TLS_PATH_MAX_LEN (TLS_PATH_SIZE - 1)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool tlsEnable;
    char caPath[TLS_PATH_SIZE];
    char crlPath[TLS_PATH_SIZE];
    char certPath[TLS_PATH_SIZE];
    char keyPath[TLS_PATH_SIZE];
    char keyPassPath[TLS_PATH_SIZE];
    char packagePath[TLS_PATH_SIZE];
    char decrypterLibPath[TLS_PATH_SIZE];
} tls_config;

#ifdef __cplusplus
}
#endif
#endif  // __MF_TLS_DEF_H__