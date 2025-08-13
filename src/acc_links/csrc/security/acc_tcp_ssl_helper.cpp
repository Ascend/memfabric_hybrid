/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "acc_tcp_ssl_helper.h"
#include "acc_common_util.h"
#include "file_util.h"
#include "openssl_api_wrapper.h"

namespace {
constexpr uint32_t CERT_CHECK_AHEAD_DAYS = 30;
constexpr uint32_t SECONDS_OF_ONE_DAY = 60 * 60 * 24;
constexpr uint32_t CHECK_PERIOD_HOURS = 7 * 24;
constexpr uint32_t HOURS_OF_ONE_DAY = 24;
constexpr std::pair<uint32_t, uint32_t> CERT_CHECK_AHEAD_DAYS_RANGE(7, 180);
constexpr std::pair<uint32_t, uint32_t> CHECK_PERIOD_HOURS_RANGE(24, 30 * 24);
}  // namespace

#define SSL_LAYER_CHECK_RET(_condition, _msg) \
    do {                                      \
        if (_condition) {                     \
            LOG_ERROR(_msg);                  \
            return ACC_ERROR;                 \
        }                                     \
    } while (0)

namespace ock {
namespace acc {

AccResult AccTcpSslHelper::Start(SSL_CTX* sslCtx, AccTlsOption &param)
{
    InitTlsPath(param);
    if (mDecryptHandler_ == nullptr && !tlsPkPwd.empty()) {
        LOG_ERROR("with private key password, decrypt handler must be set");
        return ACC_ERROR;
    }

    ReadCheckCertParams();
    auto ret = StartCheckCertExpired();
    if (ret != ACC_OK) {
        LOG_ERROR("check cert expired failed");
        return ACC_ERROR;
    }

    ret = InitSSL(sslCtx);
    if (ret != ACC_OK) {
        LOG_ERROR("load init ssl failed");
        return ACC_ERROR;
    }

    return ACC_OK;
}

void AccTcpSslHelper::InitTlsPath(AccTlsOption &param)
{
    tlsTopPath = param.tlsTopPath;
    tlsCaPath = param.tlsCaPath;
    tlsCaFile = param.tlsCaFile;
    tlsCrlPath = param.tlsCrlPath;
    tlsCrlFile = param.tlsCrlFile;
    tlsCert = param.tlsCert;
    tlsPk = param.tlsPk;
    tlsPkPwd = param.tlsPkPwd;
}

void AccTcpSslHelper::Stop(bool afterFork)
{
    StopCheckCertExpired(afterFork);
    EraseDecryptData();
}

AccResult AccTcpSslHelper::InitSSL(SSL_CTX* sslCtx)
{
    auto ret = OpenSslApiWrapper::OpensslInitSsl(0, nullptr);
    SSL_LAYER_CHECK_RET((ret <= 0), "Failed to init openssl");

    ret = OpenSslApiWrapper::OpensslInitSsl(OpenSslApiWrapper::OPENSSL_INIT_LOAD_SSL_STRINGS |
        OpenSslApiWrapper::OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
    SSL_LAYER_CHECK_RET((ret <= 0), "Failed to load error strings");

    auto sslRet = OpenSslApiWrapper::SslCtxCtrl(sslCtx, OpenSslApiWrapper::SSL_CTRL_SET_MIN_PROTO_VERSION,
                                                OpenSslApiWrapper::TLS1_3_VERSION, nullptr);
    SSL_LAYER_CHECK_RET(sslRet <= 0, "Failed to set ssl proto version");

    ret = OpenSslApiWrapper::SslCtxSetCipherSuites(sslCtx, "TLS_AES_128_GCM_SHA256:"
                                                           "TLS_AES_256_GCM_SHA384:"
                                                           "TLS_CHACHA20_POLY1305_SHA256:"
                                                           "TLS_AES_128_CCM_SHA256");
    SSL_LAYER_CHECK_RET(ret <= 0, "Failed to set cipher suites to TLS context");

    ret = LoadCaCert(sslCtx);
    SSL_LAYER_CHECK_RET(ret != ACC_OK, "Failed to load ca cert");

    ret = LoadServerCert(sslCtx);
    SSL_LAYER_CHECK_RET(ret != ACC_OK, "Failed to load server cert");

    ret = LoadPrivateKey(sslCtx);
    SSL_LAYER_CHECK_RET(ret != ACC_OK, "Failed to load private key");
    return ACC_OK;
}

AccResult AccTcpSslHelper::LoadCaFileList(std::vector<std::string> &caFileList)
{
    std::string path = tlsTopPath;
    path = path + "/" + tlsCaPath;
    caFileList.clear();
    for (auto &file : tlsCaFile) {
        auto tmpPath = path + "/" + file;
        if (!FileUtil::Realpath(tmpPath)) {
            LOG_ERROR("Failed to check ca path with ca file " << file);
            return ACC_ERROR;
        }
        caFileList.emplace_back(tmpPath);
    }
    return ACC_OK;
}

AccResult AccTcpSslHelper::LoadCaCert(SSL_CTX* sslCtx)
{
    // 设置校验函数
    OpenSslApiWrapper::SslCtxSetVerify(sslCtx, OpenSslApiWrapper::SSL_VERIFY_PEER |
        OpenSslApiWrapper::SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);

    if (!tlsCrlPath.empty() && !tlsCrlFile.empty()) {
        crlFullPath = "";
        std::string crlDirPath = tlsTopPath + "/" + tlsCrlPath;
        bool isFirstFile = true;
        for (auto &file : tlsCrlFile) {
            std::string tmpPath = crlDirPath + "/" + file;
            if (!FileUtil::Realpath(tmpPath)) {
                LOG_ERROR("Failed to check crl path with crl file " << file);
                return ACC_ERROR;
            }
            if (!isFirstFile) {
                crlFullPath += ",";
            } else {
                isFirstFile = false;
            }
            crlFullPath += tmpPath;
        }
        OpenSslApiWrapper::SslCtxSetCertVerifyCallback(sslCtx, AccTcpSslHelper::CaVerifyCallback,
                                                       reinterpret_cast<void *>
                                                       (const_cast<char *>(crlFullPath.c_str())));
    }

    std::vector<std::string> caFileList;
    SSL_LAYER_CHECK_RET(LoadCaFileList(caFileList) != ACC_OK, "Failed to load ca file list");

    for (auto &caFile : caFileList) {
        auto ret = OpenSslApiWrapper::SslCtxLoadVerifyLocations(sslCtx, caFile.c_str(), nullptr);
        SSL_LAYER_CHECK_RET(ret <= 0, "TLS load verify file failed");
    }

    return ACC_OK;
}

AccResult AccTcpSslHelper::LoadServerCert(SSL_CTX *sslCtx)
{
    auto tmpPath = tlsTopPath + "/" + tlsCert;
    SSL_LAYER_CHECK_RET(!FileUtil::Realpath(tmpPath), "get invalid cert path");

    /* load cert */
    auto ret = OpenSslApiWrapper::SslCtxUseCertificateFile(sslCtx, tmpPath.c_str(),
                                                           OpenSslApiWrapper::SSL_FILETYPE_PEM);
    SSL_LAYER_CHECK_RET(ret <= 0, "TLS use certification file failed!");

    X509 *cert = OpenSslApiWrapper::SslCtxGet0Certificate(sslCtx);
    return CertVerify(cert);
}

AccResult AccTcpSslHelper::LoadPrivateKey(SSL_CTX *sslCtx)
{
    auto tmpPath = tlsTopPath + "/" + tlsPk;
    if (!FileUtil::Realpath(tmpPath)) {
        LOG_ERROR("Failed to get private key path");
        return ACC_ERROR;
    }

    int ret = 0;
    auto tmpTlsPriKeyPwdPath = tlsTopPath + "/" + tlsPkPwd;
    if (!tlsPkPwd.empty()) {
        std::string encryptedText;
        auto ret = ReadFile(tmpTlsPriKeyPwdPath, encryptedText);
        if (ret != ACC_OK) {
            LOG_ERROR("Read private key file failed");
            return ACC_ERROR;
        }
        auto buffer = new (std::nothrow) char[encryptedText.length()];
        if (buffer == nullptr) {
            LOG_ERROR("allocate memory for buffer failed");
            return ACC_ERROR;
        }
        auto dataLength = static_cast<int>(encryptedText.length());
        ret = static_cast<AccResult>(mDecryptHandler_(encryptedText, buffer, dataLength));
        if (ret != ACC_OK) {
            LOG_ERROR("Failed to decrypt private key password");
            delete[] buffer;
            buffer = nullptr;
            return ret;
        }
        mKeyPass = std::make_pair(buffer, dataLength);
        OpenSslApiWrapper::SslCtxSetDefaultPasswdCbUserdata(sslCtx, mKeyPass.first);
    }

    /* load private key */
    ret = OpenSslApiWrapper::SslCtxUsePrivateKeyFile(sslCtx, tmpPath.c_str(), OpenSslApiWrapper::SSL_FILETYPE_PEM);
    if (ret <= 0) {
        LOG_ERROR("Failed to set use private key file");
        EraseDecryptData();
        return ACC_ERROR;
    }

    /* check private key */
    ret = OpenSslApiWrapper::SslCtxCheckPrivateKey(sslCtx);
    if (ret <= 0) {
        LOG_ERROR("Failed to set use private key file");
        EraseDecryptData();
        return ACC_ERROR;
    }
    return ACC_OK;
}

AccResult AccTcpSslHelper::ReadFile(const std::string &path, std::string &content)
{
    std::ifstream in(path);
    if (!in.is_open()) {
        LOG_ERROR("Failed to open the file");
        return ACC_ERROR;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    content = buffer.str();
    in.close();
    return ACC_OK;
}

void AccTcpSslHelper::EraseDecryptData()
{
    if (mKeyPass.first != nullptr) {
        for (auto i = 0; i < mKeyPass.second; i++) {
            mKeyPass.first[i] = '\0';
        }
        delete[] mKeyPass.first;
        mKeyPass.first = nullptr;
    }
    mKeyPass.second = 0;
}

AccResult AccTcpSslHelper::NewSslLink(bool isServer, int fd, SSL_CTX *ctx, SSL *& ssl)
{
    auto tmpSsl = OpenSslApiWrapper::SslNew(ctx);
    if (tmpSsl == nullptr) {
        LOG_ERROR("Failed to new ssl object");
        return ACC_MALLOC_FAIL;
    }

    auto ret = OpenSslApiWrapper::SslSetFd(tmpSsl, fd);
    if (ret <= 0) {
        LOG_ERROR("Failed to set fd to TLS, result " << ret);
        OpenSslApiWrapper::SslFree(tmpSsl);
        tmpSsl = nullptr;
        return ACC_ERROR;
    }

    if (isServer) {
        ret = OpenSslApiWrapper::SslAccept(tmpSsl);
        if (ret <= 0) {
            int sslErr = OpenSslApiWrapper::SslGetError(tmpSsl, ret);
            LOG_ERROR("Failed to ssl accept, result " << ret << ", ssl error " << sslErr);
            OpenSslApiWrapper::SslFree(tmpSsl);
            tmpSsl = nullptr;
            return ACC_ERROR;
        }
    } else {
        ret = OpenSslApiWrapper::SslConnect(tmpSsl);
        if (ret <= 0) {
            int sslErr = OpenSslApiWrapper::SslGetError(tmpSsl, ret);
            LOG_ERROR("Failed to ssl connect, result " << ret << ", ssl error " << sslErr);
            OpenSslApiWrapper::SslFree(tmpSsl);
            tmpSsl = nullptr;
            return ACC_ERROR;
        }
    }

    // tmpSsl is free in the external function.
    ssl = tmpSsl;
    return ACC_OK;
}

void AccTcpSslHelper::RegisterDecryptHandler(const AccDecryptHandler &h)
{
    ASSERT_RET_VOID(h != nullptr);
    ASSERT_RET_VOID(mDecryptHandler_ == nullptr);
    mDecryptHandler_ = h;
}

static X509_CRL *LoadCertRevokeListFile(const char *crlFile)
{
    // check whether file is exist
    char *realCrlPath = realpath(crlFile, nullptr);
    if (realCrlPath == nullptr) {
        return nullptr;
    }

    // load crl file
    BIO *in = OpenSslApiWrapper::BioNew(OpenSslApiWrapper::BioSFile());
    if (in == nullptr) {
        free(realCrlPath);
        realCrlPath = nullptr;
        return nullptr;
    }

    int result = OpenSslApiWrapper::BioCtrl(in, OpenSslApiWrapper::BIO_C_SET_FILENAME,
                                            OpenSslApiWrapper::BIO_CLOSE | OpenSslApiWrapper::BIO_FP_READ,
                                            const_cast<char *>(realCrlPath));
    if (result <= 0) {
        (void)OpenSslApiWrapper::BioFree(in);
        free(realCrlPath);
        realCrlPath = nullptr;
        return nullptr;
    }

    X509_CRL *crl = OpenSslApiWrapper::PemReadBioX509Crl(in, nullptr, nullptr, nullptr);
    if (crl == nullptr) {
        (void)OpenSslApiWrapper::BioFree(in);
        free(realCrlPath);
        realCrlPath = nullptr;
        return nullptr;
    }

    (void)OpenSslApiWrapper::BioFree(in);
    free(realCrlPath);
    realCrlPath = nullptr;

    return crl;
}

int AccTcpSslHelper::CaVerifyCallback(X509_STORE_CTX *x509ctx, void *arg)
{
    if (x509ctx == nullptr || arg == nullptr) {
        return 0;
    }

    auto crlPath = reinterpret_cast<char*>(arg);
    std::vector<std::string> paths;
    if (crlPath != nullptr) {
        std::string crlListStr(crlPath);
        std::stringstream crlStream(crlListStr);
        std::string item;
        while (std::getline(crlStream, item, ',')) {
            if (!item.empty()) {
                paths.push_back(item);
            }
        }
    }
    return ProcessCrlAndVerifyCert(paths, x509ctx);
}

int AccTcpSslHelper::ProcessCrlAndVerifyCert(std::vector<std::string> paths, X509_STORE_CTX *x509ctx)
{
    const int checkSuccess = 1;
    const int checkFailed = -1;

    if (!paths.empty()) {
        X509_STORE *x509Store = OpenSslApiWrapper::X509StoreCtxGet0Store(x509ctx);
        if (x509Store == nullptr) {
            LOG_ERROR("Failed to get cert in store");
            return checkFailed;
        }
        unsigned long flags = OpenSslApiWrapper::X509_V_FLAG_CRL_CHECK | OpenSslApiWrapper::X509_V_FLAG_CRL_CHECK_ALL;
        OpenSslApiWrapper::X509StoreCtxSetFlags(x509ctx, flags);
        for (auto singleCrlPath : paths) {
            X509_CRL *crl = LoadCertRevokeListFile(singleCrlPath.c_str());
            if (crl == nullptr) {
                LOG_ERROR("Failed to load cert revocation list");
                return checkFailed;
            }
            if (OpenSslApiWrapper::X509CmpCurrentTime(OpenSslApiWrapper::X509CrlGet0NextUpdate(crl)) <= 0) {
                LOG_WARN("Crl has expired! current time after next update time.");
            }
            auto result = OpenSslApiWrapper::X509StoreAddCrl(x509Store, crl);
            if (result != 1U) {
                LOG_ERROR("Store add crl failed ret:" << result);
                OpenSslApiWrapper::X509CrlFree(crl);
                return checkFailed;
            }
            OpenSslApiWrapper::X509CrlFree(crl);
        }
    }

    auto verifyResult = OpenSslApiWrapper::X509VerifyCert(x509ctx);
    if (verifyResult != 1U) {
        LOG_INFO("Verify failed in callback"
                 << " error: "
                 << OpenSslApiWrapper::X509VerifyCertErrorString(OpenSslApiWrapper::X509StoreCtxGetError(x509ctx)));
        return checkFailed;
    }

    return checkSuccess;
}

AccResult AccTcpSslHelper::CertVerify(X509 *cert)
{
    if (cert == nullptr) {
        LOG_ERROR("get cert failed");
        return ACC_ERROR;
    }

    // Validity period of the proofreading certificate
    if (OpenSslApiWrapper::X509CmpCurrentTime(OpenSslApiWrapper::X509GetNotAfter(cert)) < 0) {
        LOG_ERROR("Certificate has expired! current time after cert time.");
        return ACC_ERROR;
    }
    if (OpenSslApiWrapper::X509CmpCurrentTime(OpenSslApiWrapper::X509GetNotBefore(cert)) > 0) {
        LOG_ERROR("Certificate has expired! current time before cert time.");
        return ACC_ERROR;
    }

    // The length of the private key of the verification certificate
    EVP_PKEY* pkey = OpenSslApiWrapper::X509GetPubkey(cert);
    int keyLength = OpenSslApiWrapper::EvpPkeyBits(pkey);
    if (keyLength < MIN_PRIVATE_KEY_CONTENT_BIT_LEN) {
        LOG_ERROR("Certificate key length is too short, key length < " << MIN_PRIVATE_KEY_CONTENT_BIT_LEN);
        OpenSslApiWrapper::EvpPkeyFree(pkey);
        return ACC_ERROR;
    }
    OpenSslApiWrapper::EvpPkeyFree(pkey);

    return ACC_OK;
}

AccResult AccTcpSslHelper::StartCheckCertExpired()
{
    {
        std::unique_lock<std::mutex> lockGuard{ mMutex };
        checkExpiredRunning = true;
    }

    auto ret = HandleCertExpiredCheck();
    if (ret != ACC_OK) {
        return ACC_ERROR;
    }

    checkExpiredThread = std::thread([this]() {  return CheckCertExpiredTask(); });
    return ret;
}

AccResult AccTcpSslHelper::CheckCertExpiredTask()
{
    while (true) {
        {
            std::unique_lock<std::mutex> lockGuard {mMutex};
            if (!checkExpiredRunning) {
                return ACC_ERROR;
            }

            mCond.wait_for(lockGuard, std::chrono::hours(this->checkPeriodHours));
            if (!checkExpiredRunning) {
                return ACC_ERROR;
            }
        }

        auto ret = HandleCertExpiredCheck();
        if (ret != ACC_OK) {
            LOG_WARN("Failed to handle cert expired check");
        }
    }
}

void AccTcpSslHelper::StopCheckCertExpired(bool afterFork)
{
    if (checkExpiredThread.joinable()) {
        if (afterFork) {
            checkExpiredThread.detach();
        } else {
            {
                std::unique_lock<std::mutex> lockGuard{mMutex};
                checkExpiredRunning = false;
            }
            mCond.notify_one();

            checkExpiredThread.join();
        }
    }
}

AccResult AccTcpSslHelper::HandleCertExpiredCheck()
{
    auto certPath = tlsTopPath + "/" + tlsCert;
    if (!FileUtil::Realpath(certPath)) {
        LOG_ERROR("Failed to get cert path");
        return ACC_ERROR;
    }
    auto ret = CertExpiredCheck(certPath, "cert");
    if (ret != ACC_OK) {
        return ACC_ERROR;
    }

    auto caDirPath = tlsTopPath + "/" + tlsCaPath;
    for (auto &file : tlsCaFile) {
        auto caPath = caDirPath + "/" + file;
        if (!FileUtil::Realpath(caPath)) {
            LOG_ERROR("Failed to get ca path");
            return ACC_ERROR;
        }
        ret = CertExpiredCheck(caPath, "ca");
        if (ret != ACC_OK) {
            return ACC_ERROR;
        }
    }
    return ret;
}

void AccTcpSslHelper::ReadCheckCertParams()
{
    uint32_t tempCheckPeriod = AccCommonUtil::GetEnvValue2Uint32("ACCLINK_CHECK_PERIOD_HOURS");
    if (tempCheckPeriod < CHECK_PERIOD_HOURS_RANGE.first || tempCheckPeriod > CHECK_PERIOD_HOURS_RANGE.second) {
        LOG_WARN("ACCLINK_CHECK_PERIOD_HOURS exceeds safe range, use default value:" << CHECK_PERIOD_HOURS);
        tempCheckPeriod = CHECK_PERIOD_HOURS;
    }
    this->checkPeriodHours = static_cast<int32_t>(tempCheckPeriod);

    uint32_t tempAheadDays = AccCommonUtil::GetEnvValue2Uint32("ACCLINK_CERT_CHECK_AHEAD_DAYS");
    if (tempAheadDays < CERT_CHECK_AHEAD_DAYS_RANGE.first || tempAheadDays > CERT_CHECK_AHEAD_DAYS_RANGE.second ||
        tempAheadDays * HOURS_OF_ONE_DAY < static_cast<uint32_t>(this->checkPeriodHours)) {
        LOG_WARN("ACCLINK_CERT_CHECK_AHEAD_DAYS exceeds safe range, use default value:" << CERT_CHECK_AHEAD_DAYS);
        tempAheadDays = CERT_CHECK_AHEAD_DAYS;
    }
    this->certCheckAheadDays = static_cast<int32_t>(tempAheadDays);

    LOG_INFO("cert check period:" << this->checkPeriodHours
                                  << "hours, cert check alert ahead days:" << this->certCheckAheadDays << "days.");
}

AccResult AccTcpSslHelper::CertExpiredCheck(std::string path, std::string type)
{
    FILE *fp = fopen(path.c_str(), "r");
    if (fp == nullptr) {
        LOG_ERROR("check " << type << " expired failed by unable to open cert file, " << path);
        return ACC_ERROR;
    }
    X509 *cert = OpenSslApiWrapper::PemReadX509(fp, nullptr, nullptr, nullptr);
    if (cert == nullptr) {
        LOG_ERROR("check " << type << " expired failed by unable to parse cert file");
        fclose(fp);
        return ACC_ERROR;
    }
    ASN1_TIME *notAfter = OpenSslApiWrapper::X509GetNotAfter(cert);

    time_t now = time(nullptr);
    struct tm notAfterTm;
    if (!OpenSslApiWrapper::Asn1Time2Tm(notAfter, &notAfterTm)) {
        LOG_ERROR("failed to converting expiration time.");
        fclose(fp);
        OpenSslApiWrapper::X509Free(cert);
        return ACC_ERROR;
    }
    time_t notAfterTime = mktime(&notAfterTm);

    double timeDiffDouble = difftime(notAfterTime, now);
    if (timeDiffDouble > INT_MAX || timeDiffDouble < INT_MIN) {
        LOG_ERROR("failed to converting difftime.");
        fclose(fp);
        OpenSslApiWrapper::X509Free(cert);
        return ACC_ERROR;
    }
    int timeDiff = static_cast<int>(timeDiffDouble);
    int daysRemaining = (timeDiff + SECONDS_OF_ONE_DAY - 1) / SECONDS_OF_ONE_DAY;
    if (daysRemaining > 0) {
        LOG_INFO("check " << type << " expired success ");
        if (daysRemaining <= this->certCheckAheadDays) {
            LOG_WARN(type << " near expired, please update it in time");
        }
    } else {
        LOG_ERROR("check " << type << " expired failed");
        fclose(fp);
        OpenSslApiWrapper::X509Free(cert);
        return ACC_ERROR;
    }

    OpenSslApiWrapper::X509Free(cert);
    if (fclose(fp) != 0) {
        LOG_ERROR("check " << type << " expired failed by unable to close cert file, " << path);
    }
    return ACC_OK;
}
}  // namespace acc
}  // namespace ock