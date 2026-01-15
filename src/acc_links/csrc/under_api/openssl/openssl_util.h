/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef ACC_LINKS_OPENSSL_UTIL_H
#define ACC_LINKS_OPENSSL_UTIL_H

#include "acc_includes.h"
#include "openssl_api_dl.h"

namespace ock {
namespace acc {

Result SslShutdownHelper(SSL *s);

}
} // namespace ock

#endif
