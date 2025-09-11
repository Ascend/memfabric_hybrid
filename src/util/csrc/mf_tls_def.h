/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef MEMFABRIC_HYBRID_TLS_DEF_H
#define MEMFABRIC_HYBRID_TLS_DEF_H

#define PATH_MAX_SIZE 1024

namespace ock {
namespace mf {

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

}
}

#endif // MEMFABRIC_HYBRID_TLS_DEF_H