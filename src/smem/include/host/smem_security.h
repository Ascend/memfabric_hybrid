/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#ifndef __MEMFABRIC_SMEM_SECURITY_H__
#define __MEMFABRIC_SMEM_SECURITY_H__

#include <set>
#include <string>


typedef struct {
    bool enableTls = false;
    std::string tlsTopPath;           /* root path of certifications */
    std::string tlsCert;              /* certification of server */
    std::string tlsCrlPath;           /* optional, crl file path */
    std::string tlsCaPath;            /* ca file path */
    std::set<std::string> tlsCaFile;  /* paths of ca */
    std::set<std::string> tlsCrlFile; /* path of crl file */
    std::string tlsPk;                /* private key */
    std::string tlsPkPwd;             /* private key加密文件->可选传入 */
    std::string kmcKsfMaster;         /* private key加密文件 */
    std::string kmcKsfStandby;        /* private key加密文件 */
    std::string packagePath;          /* lib库路径 */
} smem_tls_option;

/**
 * @brief Set ssl option, required if TLS enabled
 *
 * @param tlsOption     [in] tlsOption set
 * @return 0 if successful
 */
int32_t smem_set_ssl_option(const smem_tls_option *tlsOption);

#endif