/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef ACC_LINKS_OPENSSL_API_DL_H
#define ACC_LINKS_OPENSSL_API_DL_H

#include <string>

namespace ock {
namespace acc {
using OPENSSL_INIT_SETTINGS = struct ossl_init_settings_st;
using SSL_METHOD = struct ssl_method_st;
using SSL = struct ssl_st;
using SSL_CTX = struct ssl_ctx_st;
using X509_STORE_CTX = struct x509_store_ctx_st;
using X509_CRL = struct x509_crl;
using ENGINE = struct engine_st;
using EVP_CIPHER = struct evp_cipher_st;
using EVP_CIPHER_CTX = struct evp_cipher_ctx_st;
using SSL_CIPHER = struct ssl_cipher_st;
using X509 = struct x509_st;
using BIO = struct bio;
using PEM_PASSWORD_CB = struct pem_password_cb;
using BIO_METHOD = struct bio_method;
using X509_STORE = struct x509_store;
using ASN1_TIME = struct asn1_string_st;
using EVP_PKEY = struct evp_pkey_st;

using FuncInit = int (*)(uint64_t, const OPENSSL_INIT_SETTINGS *);
using FuncOpensslCleanup = void (*)();
using FuncGetMethod = const SSL_METHOD *(*)(void);
using FuncSslOperation = int (*)(SSL *);
using FuncSslFd = int (*)(SSL *, int);
using FuncSslNew = SSL *(*)(SSL_CTX *);
using FuncSslFree = void (*)(SSL *);
using FuncSslCtxNew = SSL_CTX *(*)(const SSL_METHOD *);
using FuncSslCtxFree = void (*)(SSL_CTX *);
using FuncSslWrite = int (*)(SSL *, const void *, int);
using FuncSslRead = int (*)(SSL *, void *, int);
using FuncSslGetError = int (*)(const SSL *, int);
using FuncSslWriteEx = int (*)(SSL *, const void *, size_t, size_t *);
using FuncSslReadEx = int (*)(SSL *, void *, size_t, size_t *);

using FuncSetCipherSuites = int (*)(SSL_CTX *, const char *);
// SSL_CTX_set_min_proto_version
using FuncSslCtxCtrl = long (*)(SSL_CTX *, int, long, void *);
using FuncSslGetCurrentCipher = const SSL_CIPHER *(*)(const SSL *);
using FuncSslGetVersion = const char *(*)(const SSL *);
using FuncSslIsServer = int (*)(SSL *);

using FuncUsePrivKeyFile = int (*)(SSL_CTX *ctx, const char *, int);
using FuncUseCertFile = int (*)(SSL_CTX *, const char *, int);
using FuncPemReadX509 = X509 *(*)(FILE *fp, X509 **x, pem_password_cb *cb, void *u);
using FuncX509Free = X509 *(*)(X509 *cert);
using FuncAsn1Time2Tm = int (*)(const ASN1_TIME *s, struct tm *tm);
using FuncSslCtxSetVerify = void (*)(SSL_CTX *, int mode, int (*)(int, X509_STORE_CTX *));
using FuncSetDefaultPasswdCbUserdata = void (*)(SSL_CTX *, void *);
using FuncSetCertVerifyCallback = void (*)(SSL_CTX *, int (*cb)(X509_STORE_CTX *, void *), void *);
using FuncLoadVerifyLocations = int (*)(SSL_CTX *, const char *, const char *);
using FuncCheckPrivateKey = int (*)(const SSL_CTX *);
using FuncSslGetVerifyResult = long (*)(const SSL *);
using FuncSslGetPeerCertificate = X509 *(*)(const SSL *);
using FuncSsLCtxGet0Certificate =X509 *(*)(const SSL_CTX *ctx);

using FuncEvpAesCipher = const EVP_CIPHER *(*)();
using FuncEvpCipherCtxNew = EVP_CIPHER_CTX *(*)();
using FuncEvpCipherCtxFree = void (*)(EVP_CIPHER_CTX *);
using FuncEvpCipherCtxCtrl = int (*)(EVP_CIPHER_CTX *, int, int, void *);
using FuncEvpEncryptInitEx = int (*)(EVP_CIPHER_CTX *, const EVP_CIPHER *, ENGINE *, const unsigned char *,
    const unsigned char *);
using FuncEvpEncryptUpdate = int (*)(EVP_CIPHER_CTX *, unsigned char *, int *, const unsigned char *, int);
using FuncEvpEncryptFinalEx = int (*)(EVP_CIPHER_CTX *, unsigned char *, int *);
using FuncEvpDecryptInitEx = FuncEvpEncryptInitEx;
using FuncEvpDecryptUpdate = FuncEvpEncryptUpdate;
using FuncEvpDecryptFinalEx = FuncEvpEncryptFinalEx;

using FuncRandPoll = int (*)(void);
using FuncRandStatus = FuncRandPoll;
using FuncRandBytes = int (*)(unsigned char *buf, int num);
using FuncRandSeed = void (*)(const void *, int);

using FuncX509VerifyCert = int (*)(X509_STORE_CTX *ctx);
using FuncX509VerifyCertErrorString = const char *(*)(long n);
using FuncX509StoreCtxGetError = int (*)(const X509_STORE_CTX *ctx);
using FuncPemReadBioX509Crl = X509_CRL *(*)(BIO *bp, X509_CRL **x, PEM_PASSWORD_CB *cb, void *u);
using FuncBioSFile = const BIO_METHOD *(*)(void);
using FuncBioNew = BIO *(*)(const BIO_METHOD *);
using FuncBioFree = void (*)(BIO *b);
using FuncBioCtrl = long (*)(BIO *bp, int cmd, long larg, void *parg);
using FuncX509StoreCtxGet0Store = X509_STORE *(*)(const X509_STORE_CTX *ctx);
using FuncX509StoreCtxSetFlags = void (*)(X509_STORE_CTX *ctx, unsigned long flags);
using FuncX509StoreAddCrl = int (*)(X509_STORE *xs, X509_CRL *x);
using FuncX509CrlFree = void (*)(X509_CRL *x);

using FuncX509CmpCurrentTime = int (*)(const ASN1_TIME *s);
using FuncX509CrlGet0NextUpdate = const ASN1_TIME *(*)(const X509_CRL *crl);
using FuncX509GetNotAfter = ASN1_TIME *(*)(const X509 *x);
using FuncX509GetNotBefore = ASN1_TIME *(*)(const X509 *x);
using FuncX509GetPubkey = EVP_PKEY *(*)(X509 *x);
using FuncEvpPkeyBits = int (*)(const EVP_PKEY *pkey);
using FuncEvpPkeyFree = void (*)(EVP_PKEY *pkey);

class OPENSSLAPIDL {
public:
    static FuncInit initSsl;
    static FuncInit initCypto;
    static FuncOpensslCleanup opensslCleanup;
    static FuncGetMethod tlsServerMethod;
    static FuncGetMethod tlsClientMethod;
    static FuncGetMethod tlsMethod;
    static FuncSslOperation sslShutdown;
    static FuncSslFd sslSetFd;
    static FuncSslNew sslNew;
    static FuncSslFree sslFree;
    static FuncSslCtxNew sslCtxNew;
    static FuncSslCtxFree sslCtxFree;
    static FuncSslWrite sslWrite;
    static FuncSslRead sslRead;
    static FuncSslOperation sslConnect;
    static FuncSslOperation sslConnectState;
    static FuncSslOperation sslAccept;
    static FuncSslOperation sslAcceptState;
    static FuncSslOperation sslGetShutdown;
    static FuncSslGetError sslGetError;
    static FuncSslWriteEx sslWriteEx;
    static FuncSslReadEx sslReadEx;

    static FuncSslCtxCtrl sslCtxCtrl;
    static FuncSslGetCurrentCipher sslGetCurrentCipher;
    static FuncSslGetVersion sslGetVersion;
    static FuncSslIsServer sslIsServer;
    static FuncSetCipherSuites setCipherSuites;
    static FuncUsePrivKeyFile usePrivKeyFile;
    static FuncUseCertFile useCertFile;
    static FuncPemReadX509 pemReadX509;
    static FuncX509Free x509Free;
    static FuncAsn1Time2Tm asn1Time2Tm;
    static FuncSslCtxSetVerify sslCtxSetVerify;
    static FuncSetDefaultPasswdCbUserdata setDefaultPasswdCbUserdata;
    static FuncSetCertVerifyCallback setCertVerifyCallback;
    static FuncLoadVerifyLocations loadVerifyLocations;
    static FuncCheckPrivateKey checkPrivateKey;
    static FuncSslGetVerifyResult sslGetVerifyResult;
    static FuncSslGetPeerCertificate sslGetPeerCertificate;
    static FuncSsLCtxGet0Certificate ssLCtxGet0Certificate;

    static FuncEvpAesCipher evpAes128Gcm;
    static FuncEvpAesCipher evpAes256Gcm;

    static FuncEvpCipherCtxNew evpCipherCtxNew;
    static FuncEvpCipherCtxFree evpCipherCtxFree;
    static FuncEvpCipherCtxCtrl evpCipherCtxCtrl;

    static FuncEvpEncryptInitEx evpEncryptInitEx;
    static FuncEvpEncryptUpdate evpEncryptUpdate;
    static FuncEvpEncryptFinalEx evpEncryptFinalEx;
    static FuncEvpDecryptInitEx evpDecryptInitEx;
    static FuncEvpDecryptUpdate evpDecryptUpdate;
    static FuncEvpDecryptFinalEx evpDecryptFinalEx;

    static FuncRandPoll randPoll;
    static FuncRandStatus randStatus;
    static FuncRandBytes randBytes;
    static FuncRandBytes randPrivBytes;
    static FuncRandSeed randSeed;

    static FuncX509VerifyCert x509VerifyCert;
    static FuncX509VerifyCertErrorString x509VerifyCertErrorString;
    static FuncX509StoreCtxGetError x509StoreCtxGetError;
    static FuncPemReadBioX509Crl pemReadBioX509Crl;
    static FuncBioSFile bioSFile;
    static FuncBioNew bioNew;
    static FuncBioFree bioFree;
    static FuncBioCtrl bioCtrl;
    static FuncX509StoreCtxGet0Store x509StoreCtxGet0Store;
    static FuncX509StoreCtxSetFlags x509StoreCtxSetFlags;
    static FuncX509StoreAddCrl x509StoreAddCrl;
    static FuncX509CrlFree x509CrlFree;

    static FuncX509CmpCurrentTime x509CmpCurrentTime;
    static FuncX509CrlGet0NextUpdate x509CrlGet0NextUpdate;
    static FuncX509GetNotAfter x509GetNotAfter;
    static FuncX509GetNotBefore x509GetNotBefore;
    static FuncX509GetPubkey x509GetPubkey;
    static FuncEvpPkeyBits evpPkeyBits;
    static FuncEvpPkeyFree evpPkeyFree;

    static int LoadOpensslAPI(const std::string &libPath);

private:
    static const char *gOpensslEnvPath;
    static const char *gOpensslLibSslName;
    static const char *gOpensslLibCryptoName;
    static const char *gSep;
    static bool gLoaded;

    static int GetLibPath(std::string &libDir, std::string &libSslPath, std::string &libCryptoPath);
    static int LoadSSLSymbols(void *sslHandle);
    static int LoadCryptoSymbols(void *cryptoHandle);
};
}  // namespace acc
}  // namespace ock

#endif  // ACC_LINKS_OPENSSL_API_DL_H
