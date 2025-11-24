/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACC_LINKS_ACC_TCP_SSL_HELPER_H
#define ACC_LINKS_ACC_TCP_SSL_HELPER_H

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include <sstream>
#include <cstdint>
#include <fstream>
#include <climits>

#include "acc_includes.h"
#include "openssl_api_dl.h"

namespace ock {
namespace acc {

constexpr int MIN_PRIVATE_KEY_CONTENT_BIT_LEN = 3072; // RSA密钥长度要求大于3072
constexpr int MIN_PRIVATE_KEY_CONTENT_BYTE_LEN = MIN_PRIVATE_KEY_CONTENT_BIT_LEN / 8;

class AccTcpSslHelper : public AccReferable {
public:
    AccResult Start(SSL_CTX *sslCtx, AccTlsOption &param);
    void Stop(bool afterFork = false);

    ~AccTcpSslHelper()
    {
        Stop();
    }

    void EraseDecryptData();

    static AccResult NewSslLink(bool isServer, int fd, SSL_CTX *ctx, SSL *&ssl);
    void RegisterDecryptHandler(const AccDecryptHandler &h);

private:
    AccResult InitTlsPath(AccTlsOption &param);
    AccResult InitSSL(SSL_CTX *sslCtx);

    static int CaVerifyCallback(X509_STORE_CTX *x509ctx, void *arg);
    static int ProcessCrlAndVerifyCert(std::vector<std::string> paths, X509_STORE_CTX *x509ctx);
    AccResult ReadFile(const std::string &path, std::string &content);
    AccResult LoadCaCert(SSL_CTX *sslCtx);
    AccResult LoadServerCert(SSL_CTX *sslCtx);
    AccResult LoadPrivateKey(SSL_CTX *sslCtx);
    AccResult CertVerify(X509 *cert) const;
    AccResult CheckCertExpiredTask();
    AccResult StartCheckCertExpired();
    void StopCheckCertExpired(bool afterFork);
    AccResult HandleCertExpiredCheck() const;
    static AccResult CertExpiredCheck(std::string path, std::string type);
    void ReadCheckCertParams();
    AccResult GetPkPass();

private:
    AccDecryptHandler mDecryptHandler_ = nullptr; // 解密回调
    std::pair<char *, int> mKeyPass = { nullptr, 0 };
    std::thread checkExpiredThread;
    std::mutex mMutex;
    std::condition_variable mCond;
    bool checkExpiredRunning = false;
    int32_t certCheckAheadDays = 0;
    int32_t checkPeriodHours = 0;

    std::string crlFullPath;
    // 证书相关路径
    std::string tlsTopPath;
    std::string tlsCert;
    std::string tlsPk;
    std::string tlsPkPwd;
    std::vector<std::string> tlsCaPaths;
    std::vector<std::string> tlsCrlPaths;
};
using AccTcpSslHelperPtr = AccRef<AccTcpSslHelper>;
}  // namespace acc
}  // namespace ock

#endif  // ACC_LINKS_ACC_TCP_SSL_HELPER_H
