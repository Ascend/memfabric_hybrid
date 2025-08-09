/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <linux/limits.h>

#include <dlfcn.h>
#include <unistd.h>

#include "acc_includes.h"
#include "acc_file_validator.h"
#include "openssl_api_dl.h"

#define DLSYM(handle, type, ptr, sym)              \
    do {                                           \
        auto ptr1 = dlsym((handle), (sym));        \
        if (ptr1 == nullptr) {                     \
            LOG_ERROR("Failed to load " << (sym)); \
            return -1;                             \
        }                                          \
        (ptr) = (type)ptr1;                        \
    } while (0)

namespace ock {
namespace acc {
FuncInit OPENSSLAPIDL::initSsl = nullptr;
FuncInit OPENSSLAPIDL::initCypto = nullptr;
FuncOpensslCleanup OPENSSLAPIDL::opensslCleanup = nullptr;

FuncGetMethod OPENSSLAPIDL::tlsServerMethod = nullptr;
FuncGetMethod OPENSSLAPIDL::tlsClientMethod = nullptr;
FuncGetMethod OPENSSLAPIDL::tlsMethod = nullptr;
FuncSslOperation OPENSSLAPIDL::sslShutdown = nullptr;
FuncSslFd OPENSSLAPIDL::sslSetFd = nullptr;
FuncSslNew OPENSSLAPIDL::sslNew = nullptr;
FuncSslFree OPENSSLAPIDL::sslFree = nullptr;
FuncSslCtxNew OPENSSLAPIDL::sslCtxNew = nullptr;
FuncSslCtxFree OPENSSLAPIDL::sslCtxFree = nullptr;
FuncSslWrite OPENSSLAPIDL::sslWrite = nullptr;
FuncSslRead OPENSSLAPIDL::sslRead = nullptr;
FuncSslOperation OPENSSLAPIDL::sslConnect = nullptr;
FuncSslOperation OPENSSLAPIDL::sslConnectState = nullptr;
FuncSslOperation OPENSSLAPIDL::sslAccept = nullptr;
FuncSslOperation OPENSSLAPIDL::sslAcceptState = nullptr;
FuncSslOperation OPENSSLAPIDL::sslGetShutdown = nullptr;
FuncSslGetError OPENSSLAPIDL::sslGetError = nullptr;
FuncSslWriteEx OPENSSLAPIDL::sslWriteEx = nullptr;
FuncSslReadEx OPENSSLAPIDL::sslReadEx = nullptr;

FuncSslCtxCtrl OPENSSLAPIDL::sslCtxCtrl = nullptr;
FuncSslGetCurrentCipher OPENSSLAPIDL::sslGetCurrentCipher = nullptr;
FuncSslGetVersion OPENSSLAPIDL::sslGetVersion = nullptr;
FuncSslIsServer OPENSSLAPIDL::sslIsServer = nullptr;
FuncSetCipherSuites OPENSSLAPIDL::setCipherSuites = nullptr;
FuncUsePrivKeyFile OPENSSLAPIDL::usePrivKeyFile = nullptr;
FuncUseCertFile OPENSSLAPIDL::useCertFile = nullptr;
FuncSslCtxSetVerify OPENSSLAPIDL::sslCtxSetVerify = nullptr;
FuncSetDefaultPasswdCbUserdata OPENSSLAPIDL::setDefaultPasswdCbUserdata = nullptr;
FuncSetCertVerifyCallback OPENSSLAPIDL::setCertVerifyCallback = nullptr;
FuncLoadVerifyLocations OPENSSLAPIDL::loadVerifyLocations = nullptr;
FuncCheckPrivateKey OPENSSLAPIDL::checkPrivateKey = nullptr;
FuncSslGetVerifyResult OPENSSLAPIDL::sslGetVerifyResult = nullptr;
FuncSslGetPeerCertificate OPENSSLAPIDL::sslGetPeerCertificate = nullptr;
FuncSsLCtxGet0Certificate OPENSSLAPIDL::ssLCtxGet0Certificate = nullptr;

FuncEvpAesCipher OPENSSLAPIDL::evpAes128Gcm = nullptr;
FuncEvpAesCipher OPENSSLAPIDL::evpAes256Gcm = nullptr;

FuncEvpCipherCtxNew OPENSSLAPIDL::evpCipherCtxNew = nullptr;
FuncEvpCipherCtxFree OPENSSLAPIDL::evpCipherCtxFree = nullptr;
FuncEvpCipherCtxCtrl OPENSSLAPIDL::evpCipherCtxCtrl = nullptr;

FuncEvpEncryptInitEx OPENSSLAPIDL::evpEncryptInitEx = nullptr;
FuncEvpEncryptUpdate OPENSSLAPIDL::evpEncryptUpdate = nullptr;
FuncEvpEncryptFinalEx OPENSSLAPIDL::evpEncryptFinalEx = nullptr;
FuncEvpDecryptInitEx OPENSSLAPIDL::evpDecryptInitEx = nullptr;
FuncEvpDecryptUpdate OPENSSLAPIDL::evpDecryptUpdate = nullptr;
FuncEvpDecryptFinalEx OPENSSLAPIDL::evpDecryptFinalEx = nullptr;

FuncRandPoll OPENSSLAPIDL::randPoll = nullptr;
FuncRandStatus OPENSSLAPIDL::randStatus = nullptr;
FuncRandBytes OPENSSLAPIDL::randBytes = nullptr;
FuncRandBytes OPENSSLAPIDL::randPrivBytes = nullptr;
FuncRandSeed OPENSSLAPIDL::randSeed = nullptr;

FuncX509VerifyCert OPENSSLAPIDL::x509VerifyCert = nullptr;
FuncX509VerifyCertErrorString OPENSSLAPIDL::x509VerifyCertErrorString = nullptr;
FuncX509StoreCtxGetError OPENSSLAPIDL::x509StoreCtxGetError = nullptr;
FuncPemReadBioX509Crl OPENSSLAPIDL::pemReadBioX509Crl = nullptr;
FuncBioSFile OPENSSLAPIDL::bioSFile = nullptr;
FuncBioNew OPENSSLAPIDL::bioNew = nullptr;
FuncBioFree OPENSSLAPIDL::bioFree = nullptr;
FuncBioCtrl OPENSSLAPIDL::bioCtrl = nullptr;
FuncX509StoreCtxGet0Store OPENSSLAPIDL::x509StoreCtxGet0Store = nullptr;
FuncX509StoreCtxSetFlags OPENSSLAPIDL::x509StoreCtxSetFlags = nullptr;
FuncX509StoreAddCrl OPENSSLAPIDL::x509StoreAddCrl = nullptr;
FuncX509CrlFree OPENSSLAPIDL::x509CrlFree = nullptr;

FuncX509CmpCurrentTime OPENSSLAPIDL::x509CmpCurrentTime = nullptr;
FuncX509CrlGet0NextUpdate OPENSSLAPIDL::x509CrlGet0NextUpdate = nullptr;
FuncX509GetNotAfter OPENSSLAPIDL::x509GetNotAfter = nullptr;
FuncX509GetNotBefore OPENSSLAPIDL::x509GetNotBefore = nullptr;
FuncX509GetPubkey OPENSSLAPIDL::x509GetPubkey = nullptr;
FuncEvpPkeyBits OPENSSLAPIDL::evpPkeyBits = nullptr;
FuncEvpPkeyFree OPENSSLAPIDL::evpPkeyFree = nullptr;
FuncPemReadX509 OPENSSLAPIDL::pemReadX509 = nullptr;
FuncX509Free OPENSSLAPIDL::x509Free = nullptr;
FuncAsn1Time2Tm OPENSSLAPIDL::asn1Time2Tm = nullptr;

bool OPENSSLAPIDL::gLoaded = false;
const char *OPENSSLAPIDL::gOpensslEnvPath = "EP_OPENSSL_PATH";
const char *OPENSSLAPIDL::gOpensslLibSslName = "libssl.so";
const char *OPENSSLAPIDL::gOpensslLibCryptoName = "libcrypto.so";
const char *OPENSSLAPIDL::gSep = "/";

int OPENSSLAPIDL::GetLibPath(std::string &libDir, std::string &libSslPath, std::string &libCryptoPath)
{
    if (FileUtil::IsSymlink(libDir)) {
        LOG_ERROR("Path for openssl library un-support symlink.");
        return -1;
    }

    if (!FileUtil::Realpath(libDir)) {
        LOG_ERROR("Path for openssl library is invalid.");
        return -1;
    }

    if (libDir.back() != '/') {
        libDir.push_back('/');
    }

    std::string subFolder = "";
#ifdef UT_ENABLED  // ut测试跳过shutdown操作
    subFolder = "";
#endif

    libSslPath = libDir + subFolder + gOpensslLibSslName;
    if (::access(libSslPath.c_str(), F_OK) != 0) {
        LOG_ERROR("libssl.so path set in env is invalid");
        return -1;
    }

    libCryptoPath = libDir + subFolder + gOpensslLibCryptoName;
    if (::access(libCryptoPath.c_str(), F_OK) != 0) {
        LOG_ERROR("libcrypto.so path set in env is invalid");
        return -1;
    }
    return 0;
}

int OPENSSLAPIDL::LoadSSLSymbols(void *sslHandle)
{
    DLSYM(sslHandle, FuncInit, initSsl, "OPENSSL_init_ssl");
    DLSYM(sslHandle, FuncInit, initCypto, "OPENSSL_init_crypto");
    DLSYM(sslHandle, FuncOpensslCleanup, opensslCleanup, "OPENSSL_cleanup");
    DLSYM(sslHandle, FuncGetMethod, tlsServerMethod, "TLS_server_method");
    DLSYM(sslHandle, FuncGetMethod, tlsClientMethod, "TLS_client_method");
    DLSYM(sslHandle, FuncGetMethod, tlsMethod, "TLS_method");
    DLSYM(sslHandle, FuncSslOperation, sslShutdown, "SSL_shutdown");
    DLSYM(sslHandle, FuncSslFd, sslSetFd, "SSL_set_fd");
    DLSYM(sslHandle, FuncSslNew, sslNew, "SSL_new");
    DLSYM(sslHandle, FuncSslFree, sslFree, "SSL_free");
    DLSYM(sslHandle, FuncSslCtxNew, sslCtxNew, "SSL_CTX_new");
    DLSYM(sslHandle, FuncSslCtxFree, sslCtxFree, "SSL_CTX_free");
    DLSYM(sslHandle, FuncSslWrite, sslWrite, "SSL_write");
    DLSYM(sslHandle, FuncSslRead, sslRead, "SSL_read");
    DLSYM(sslHandle, FuncSslOperation, sslConnect, "SSL_connect");
    DLSYM(sslHandle, FuncSslOperation, sslConnectState, "SSL_set_connect_state");
    DLSYM(sslHandle, FuncSslOperation, sslAccept, "SSL_accept");
    DLSYM(sslHandle, FuncSslOperation, sslAcceptState, "SSL_set_accept_state");
    DLSYM(sslHandle, FuncSslOperation, sslGetShutdown, "SSL_get_shutdown");
    DLSYM(sslHandle, FuncSslGetError, sslGetError, "SSL_get_error");
    DLSYM(sslHandle, FuncSetCipherSuites, setCipherSuites, "SSL_CTX_set_ciphersuites");
    DLSYM(sslHandle, FuncSslCtxCtrl, sslCtxCtrl, "SSL_CTX_ctrl");
    DLSYM(sslHandle, FuncSslGetCurrentCipher, sslGetCurrentCipher, "SSL_get_current_cipher");
    DLSYM(sslHandle, FuncSslGetVersion, sslGetVersion, "SSL_get_version");
    DLSYM(sslHandle, FuncUsePrivKeyFile, usePrivKeyFile, "SSL_CTX_use_PrivateKey_file");
    DLSYM(sslHandle, FuncUseCertFile, useCertFile, "SSL_CTX_use_certificate_file");
    DLSYM(sslHandle, FuncSslCtxSetVerify, sslCtxSetVerify, "SSL_CTX_set_verify");
    DLSYM(sslHandle, FuncSetDefaultPasswdCbUserdata, setDefaultPasswdCbUserdata,
          "SSL_CTX_set_default_passwd_cb_userdata");
    DLSYM(sslHandle, FuncSetCertVerifyCallback, setCertVerifyCallback, "SSL_CTX_set_cert_verify_callback");
    DLSYM(sslHandle, FuncLoadVerifyLocations, loadVerifyLocations, "SSL_CTX_load_verify_locations");
    DLSYM(sslHandle, FuncCheckPrivateKey, checkPrivateKey, "SSL_CTX_check_private_key");
    DLSYM(sslHandle, FuncSslGetVerifyResult, sslGetVerifyResult, "SSL_get_verify_result");
    DLSYM(sslHandle, FuncSslGetPeerCertificate, sslGetPeerCertificate, "SSL_get1_peer_certificate");
    DLSYM(sslHandle, FuncSsLCtxGet0Certificate, ssLCtxGet0Certificate, "SSL_CTX_get0_certificate");
    DLSYM(sslHandle, FuncSslWriteEx, sslWriteEx, "SSL_write_ex");
    DLSYM(sslHandle, FuncSslReadEx, sslReadEx, "SSL_read_ex");
    DLSYM(sslHandle, FuncSslIsServer, sslIsServer, "SSL_is_server");
    return 0;
}

int OPENSSLAPIDL::LoadCryptoSymbols(void *cryptoHandle)
{
    DLSYM(cryptoHandle, FuncEvpCipherCtxNew, evpCipherCtxNew, "EVP_CIPHER_CTX_new");
    DLSYM(cryptoHandle, FuncEvpCipherCtxFree, evpCipherCtxFree, "EVP_CIPHER_CTX_free");
    DLSYM(cryptoHandle, FuncEvpCipherCtxCtrl, evpCipherCtxCtrl, "EVP_CIPHER_CTX_ctrl");
    DLSYM(cryptoHandle, FuncEvpEncryptInitEx, evpEncryptInitEx, "EVP_EncryptInit_ex");
    DLSYM(cryptoHandle, FuncEvpEncryptUpdate, evpEncryptUpdate, "EVP_EncryptUpdate");
    DLSYM(cryptoHandle, FuncEvpEncryptFinalEx, evpEncryptFinalEx, "EVP_EncryptFinal_ex");
    DLSYM(cryptoHandle, FuncEvpDecryptInitEx, evpDecryptInitEx, "EVP_DecryptInit_ex");
    DLSYM(cryptoHandle, FuncEvpDecryptUpdate, evpDecryptUpdate, "EVP_DecryptUpdate");
    DLSYM(cryptoHandle, FuncEvpDecryptFinalEx, evpDecryptFinalEx, "EVP_DecryptFinal_ex");
    DLSYM(cryptoHandle, FuncEvpAesCipher, evpAes128Gcm, "EVP_aes_128_gcm");
    DLSYM(cryptoHandle, FuncEvpAesCipher, evpAes256Gcm, "EVP_aes_256_gcm");

    DLSYM(cryptoHandle, FuncRandPoll, randPoll, "RAND_poll");
    DLSYM(cryptoHandle, FuncRandStatus, randStatus, "RAND_status");
    DLSYM(cryptoHandle, FuncRandBytes, randBytes, "RAND_bytes");
    DLSYM(cryptoHandle, FuncRandBytes, randPrivBytes, "RAND_priv_bytes");
    DLSYM(cryptoHandle, FuncRandSeed, randSeed, "RAND_seed");

    DLSYM(cryptoHandle, FuncX509VerifyCert, x509VerifyCert, "X509_verify_cert");
    DLSYM(cryptoHandle, FuncX509VerifyCertErrorString, x509VerifyCertErrorString, "X509_verify_cert_error_string");
    DLSYM(cryptoHandle, FuncX509StoreCtxGetError, x509StoreCtxGetError, "X509_STORE_CTX_get_error");
    DLSYM(cryptoHandle, FuncPemReadBioX509Crl, pemReadBioX509Crl, "PEM_read_bio_X509_CRL");
    DLSYM(cryptoHandle, FuncBioSFile, bioSFile, "BIO_s_file");
    DLSYM(cryptoHandle, FuncBioNew, bioNew, "BIO_new");
    DLSYM(cryptoHandle, FuncBioFree, bioFree, "BIO_free");
    DLSYM(cryptoHandle, FuncBioCtrl, bioCtrl, "BIO_ctrl");
    DLSYM(cryptoHandle, FuncX509StoreCtxGet0Store, x509StoreCtxGet0Store, "X509_STORE_CTX_get0_store");
    DLSYM(cryptoHandle, FuncX509StoreCtxSetFlags, x509StoreCtxSetFlags, "X509_STORE_CTX_set_flags");
    DLSYM(cryptoHandle, FuncX509StoreAddCrl, x509StoreAddCrl, "X509_STORE_add_crl");
    DLSYM(cryptoHandle, FuncX509CrlFree, x509CrlFree, "X509_CRL_free");

    DLSYM(cryptoHandle, FuncX509CmpCurrentTime, x509CmpCurrentTime, "X509_cmp_current_time");
    DLSYM(cryptoHandle, FuncX509CrlGet0NextUpdate, x509CrlGet0NextUpdate, "X509_CRL_get0_nextUpdate");
    DLSYM(cryptoHandle, FuncX509GetNotAfter, x509GetNotAfter, "X509_getm_notAfter");
    DLSYM(cryptoHandle, FuncX509GetNotBefore, x509GetNotBefore, "X509_getm_notBefore");
    DLSYM(cryptoHandle, FuncX509GetPubkey, x509GetPubkey, "X509_get_pubkey");
    DLSYM(cryptoHandle, FuncEvpPkeyBits, evpPkeyBits, "EVP_PKEY_get_bits");
    DLSYM(cryptoHandle, FuncEvpPkeyFree, evpPkeyFree, "EVP_PKEY_free");
    DLSYM(cryptoHandle, FuncPemReadX509, pemReadX509, "PEM_read_X509");
    DLSYM(cryptoHandle, FuncX509Free, x509Free, "X509_free");
    DLSYM(cryptoHandle, FuncAsn1Time2Tm, asn1Time2Tm, "ASN1_TIME_to_tm");
    return 0;
}

int OPENSSLAPIDL::LoadOpensslAPI(const std::string &libPath)
{
    LOG_INFO("Starting to load openssl api");
    if (gLoaded) {
        return 0;
    }

    std::string libDir = libPath;
    std::string libSslPath;
    std::string libCryptoPath;
    if (GetLibPath(libDir, libSslPath, libCryptoPath) != 0) {
        return -1;
    }

    std::string errMsg;
    if (!FileValidator::RegularFilePath(libSslPath, libDir, errMsg) ||
        !FileValidator::IsFileValid(libSslPath, errMsg) ||
        !FileValidator::CheckPermission(libSslPath, 0b101101000, false, errMsg)) {
        LOG_ERROR(errMsg);
        return -1;
    }
    if (!FileValidator::RegularFilePath(libCryptoPath, libDir, errMsg) ||
        !FileValidator::IsFileValid(libCryptoPath, errMsg) ||
        !FileValidator::CheckPermission(libCryptoPath, 0b101101000, false, errMsg)) {
        LOG_ERROR(errMsg);
        return -1;
    }

    auto cryptoHandle = dlopen(libCryptoPath.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (cryptoHandle == nullptr) {
        LOG_ERROR("Failed to dlopen libcrypto.so err: " << dlerror());
        return -1;
    }

    if (LoadCryptoSymbols(cryptoHandle) == -1) {
        LOG_ERROR("Failed to dlopen libcrypto.so err: " << dlerror());
        return -1;
    }

    auto sslHandle = dlopen(libSslPath.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (sslHandle == nullptr) {
        LOG_ERROR("Failed to dlopen libssl.so err: " << dlerror());
        return -1;
    }

    if (LoadSSLSymbols(sslHandle) == -1) {
        LOG_ERROR("Failed to dlopen libssl.so err: " << dlerror());
        return -1;
    }

    gLoaded = true;
    return 0;
}
}  // namespace acc
}  // namespace ock