/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef __MF_TLS_DEF_H__
#define __MF_TLS_DEF_H__

#define PATH_MAX_SIZE 1024

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool tlsEnable;
    char caPath[PATH_MAX_SIZE];
    char crlPath[PATH_MAX_SIZE];
    char certPath[PATH_MAX_SIZE];
    char keyPath[PATH_MAX_SIZE];
    char keyPassPath[PATH_MAX_SIZE];
    char packagePath[PATH_MAX_SIZE];
    char decrypterLibPath[PATH_MAX_SIZE];
} tls_config;

#ifdef __cplusplus
}
#endif
#endif  // __MF_TLS_DEF_H__