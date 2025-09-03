/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
        EraseDecryptData();
    }

    void EraseDecryptData();

    static AccResult NewSslLink(bool isServer, int fd, SSL_CTX *ctx, SSL *&ssl);
    void RegisterDecryptHandler(const AccDecryptHandler &h);

private:
    void InitTlsPath(AccTlsOption &otherConfig);
    AccResult InitSSL(SSL_CTX *sslCtx);

    static int CaVerifyCallback(X509_STORE_CTX *x509ctx, void *arg);
    static int ProcessCrlAndVerifyCert(std::vector<std::string> paths, X509_STORE_CTX *x509ctx);
    AccResult ReadFile(const std::string &path, std::string &content);
    AccResult LoadCaFileList(std::vector<std::string> &caFileList);
    AccResult LoadCaCert(SSL_CTX *sslCtx);
    AccResult LoadServerCert(SSL_CTX *sslCtx);
    AccResult LoadPrivateKey(SSL_CTX *sslCtx);
    AccResult CertVerify(X509 *cert);
    AccResult CheckCertExpiredTask();
    AccResult StartCheckCertExpired();
    void StopCheckCertExpired(bool afterFork);
    AccResult HandleCertExpiredCheck();
    AccResult CertExpiredCheck(std::string path, std::string type);
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
    std::string tlsCaPath;
    std::set<std::string> tlsCaFile;
    std::string tlsCrlPath;
    std::set<std::string> tlsCrlFile;
    std::string tlsCert;
    std::string tlsPk;
    std::string tlsPkPwd;
};
using AccTcpSslHelperPtr = AccRef<AccTcpSslHelper>;
}  // namespace acc
}  // namespace ock

#endif  // ACC_LINKS_ACC_TCP_SSL_HELPER_H
