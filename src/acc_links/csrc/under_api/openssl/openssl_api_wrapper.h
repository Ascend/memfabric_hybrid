/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef ACC_LINKS_OPENSSL_API_WRAPPER_H
#define ACC_LINKS_OPENSSL_API_WRAPPER_H

#include "openssl_api_dl.h"

namespace ock {
namespace acc {
class OpenSslApiWrapper {
public:
    static const uint32_t SSL_VERIFY_NONE = 0U;
    static const uint32_t SSL_VERIFY_PEER = 1U;
    static const uint32_t SSL_VERIFY_FAIL_IF_NO_PEER_CERT = 2U;
    static const uint32_t SSL_FILETYPE_PEM = 1U;
    static const uint32_t EVP_CTRL_AEAD_SET_IVLEN = 9U;
    static const uint32_t EVP_CTRL_AEAD_GET_TAG = 16U;
    static const uint32_t EVP_CTRL_AEAD_SET_TAG = 17U;
    static const uint32_t OPENSSL_INIT_LOAD_SSL_STRINGS = 2097152U;
    static const uint32_t OPENSSL_INIT_LOAD_CRYPTO_STRINGS = 2U;
    static const uint32_t SSL_CTRL_SET_MIN_PROTO_VERSION = 123U;
    static const uint32_t TLS1_3_VERSION = 772U;
    static const uint32_t SSL_ERROR_WANT_READ = 2U;
    static const uint32_t SSL_ERROR_WANT_WRITE = 3U;
    static const uint32_t SSL_ERROR_ZERO_RETURN = 6U;

    static const uint32_t SSL_SENT_SHUTDOWN = 1U;
    static const uint32_t SSL_RECEIVED_SHUTDOWN = 2U;

    static const uint32_t BIO_C_SET_FILENAME = 108U;
    static const uint32_t BIO_CLOSE = 1U;
    static const uint32_t BIO_FP_READ = 2U;
    static const uint32_t X509_V_FLAG_CRL_CHECK = 4U;
    static const uint32_t X509_V_FLAG_CRL_CHECK_ALL = 8U;

    static int OpensslInitSsl(uint64_t opts, const OPENSSL_INIT_SETTINGS *settings)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::initSsl != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::initSsl(opts, settings);
    }

    static inline int OpensslInitCrypto(uint64_t opts, const OPENSSL_INIT_SETTINGS *settings)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::initCypto != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::initCypto(opts, settings);
    }

    static inline const SSL_METHOD *TlsClientMethod()
    {
        VALIDATE_RETURN(OPENSSLAPIDL::tlsClientMethod != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::tlsClientMethod();
    }

    static inline const SSL_METHOD *TlsMethod()
    {
        VALIDATE_RETURN(OPENSSLAPIDL::tlsMethod != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::tlsMethod();
    }

    static inline const SSL_METHOD *TlsServerMethod()
    {
        VALIDATE_RETURN(OPENSSLAPIDL::tlsServerMethod != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::tlsServerMethod();
    }

    static inline int SslShutdown(SSL *s)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslShutdown != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::sslShutdown(s);
    }

    static inline int SslSetFd(SSL *s, int fd)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslSetFd != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::sslSetFd(s, fd);
    }

    static inline SSL *SslNew(SSL_CTX *ctx)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslNew != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::sslNew(ctx);
    }

    static inline void SslFree(SSL *s)
    {
        VALIDATE_RETURN_VOID(OPENSSLAPIDL::sslFree != nullptr, "openssl handler not loaded");
        OPENSSLAPIDL::sslFree(s);
    }

    static SSL_CTX *SslCtxNew(const SSL_METHOD *method)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslCtxNew != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::sslCtxNew(method);
    }

    static inline void SslCtxFree(SSL_CTX *ctx)
    {
        VALIDATE_RETURN_VOID(OPENSSLAPIDL::sslCtxFree != nullptr, "openssl handler not loaded");
        OPENSSLAPIDL::sslCtxFree(ctx);
    }

    static inline int SslWrite(SSL *s, const void *buf, int num)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslWrite != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::sslWrite(s, buf, num);
    }

    static inline int SslRead(SSL *s, void *buf, int num)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslRead != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::sslRead(s, buf, num);
    }

    static inline int SslConnect(SSL *s)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslConnect != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::sslConnect(s);
    }

    static inline int SslConnectState(SSL *s)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslConnectState != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::sslConnectState(s);
    }

    static inline int SslAccept(SSL *s)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslAccept != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::sslAccept(s);
    }

    static inline int SslAcceptState(SSL *s)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslAcceptState != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::sslAcceptState(s);
    }

    static inline int SslGetShutdown(SSL *s)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslGetShutdown != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::sslGetShutdown(s);
    }

    static inline int SslGetError(const SSL *s, int retCode)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslGetError != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::sslGetError(s, retCode);
    }

    static inline int SslWriteEx(SSL *s, const void *buf, size_t num, size_t *written)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslWriteEx != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::sslWriteEx(s, buf, num, written);
    }

    static inline int SslReadEx(SSL *s, void *buf, size_t num, size_t *readbytes)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslReadEx != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::sslReadEx(s, buf, num, readbytes);
    }

    static inline int SslCtxSetCipherSuites(SSL_CTX *ctx, const char *str)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::setCipherSuites != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::setCipherSuites(ctx, str);
    }

    static inline long SslCtxCtrl(SSL_CTX *ctx, int cmd, long larg, void *parg)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslCtxCtrl != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::sslCtxCtrl(ctx, cmd, larg, parg);
    }

    static inline const char *SslGetVersion(const SSL *ssl)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslGetVersion != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::sslGetVersion(ssl);
    }

    static inline int SslIsServer(SSL *ssl)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslIsServer != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::sslIsServer(ssl);
    }

    static inline void SslCtxSetVerify(SSL_CTX *ctx, int mode, int (*cb)(int, X509_STORE_CTX *))
    {
        VALIDATE_RETURN_VOID(OPENSSLAPIDL::sslCtxSetVerify != nullptr, "openssl handler not loaded");
        OPENSSLAPIDL::sslCtxSetVerify(ctx, mode, cb);
    }

    static inline int SslCtxUsePrivateKey(SSL_CTX *ctx, EVP_PKEY *pkey)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::usePrivKey != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::usePrivKey(ctx, pkey);
    }

    static inline int SslCtxUsePrivateKeyFile(SSL_CTX *ctx, const char *file, int type)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::usePrivKeyFile != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::usePrivKeyFile(ctx, file, type);
    }

    static inline int SslCtxUseCertificateFile(SSL_CTX *ctx, const char *file, int type)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::useCertFile != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::useCertFile(ctx, file, type);
    }

    static inline X509 *PemReadX509(FILE *fp, X509 **x, PEM_PASSWORD_CB *cb, void *u)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::pemReadX509 != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::pemReadX509(fp, x, cb, u);
    }

    static inline void X509Free(X509 *cert)
    {
        VALIDATE_RETURN_VOID(OPENSSLAPIDL::x509Free != nullptr, "openssl handler not loaded");
        OPENSSLAPIDL::x509Free(cert);
    }

    static inline int Asn1Time2Tm(const ASN1_TIME *s, struct tm *tm)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::asn1Time2Tm != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::asn1Time2Tm(s, tm);
    }

    static inline void SslCtxSetDefaultPasswdCbUserdata(SSL_CTX *ctx, void *u)
    {
        VALIDATE_RETURN_VOID(OPENSSLAPIDL::setDefaultPasswdCbUserdata != nullptr, "openssl handler not loaded");
        OPENSSLAPIDL::setDefaultPasswdCbUserdata(ctx, u);
    }

    static inline void SslCtxSetCertVerifyCallback(SSL_CTX *ctx, int (*cb)(X509_STORE_CTX *, void *), void *arg)
    {
        VALIDATE_RETURN_VOID(OPENSSLAPIDL::setCertVerifyCallback != nullptr, "openssl handler not loaded");
        OPENSSLAPIDL::setCertVerifyCallback(ctx, cb, arg);
    }

    static inline int SslCtxLoadVerifyLocations(SSL_CTX *ctx, const char *cafile, const char *capath)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::loadVerifyLocations != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::loadVerifyLocations(ctx, cafile, capath);
    }

    static inline int SslCtxCheckPrivateKey(const SSL_CTX *ctx)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::checkPrivateKey != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::checkPrivateKey(ctx);
    }

    static inline X509 *SslGetPeerCertificate(const SSL *ssl)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslGetPeerCertificate != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::sslGetPeerCertificate(ssl);
    }

    static inline X509 *SslCtxGet0Certificate(const SSL_CTX *ctx)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::ssLCtxGet0Certificate != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::ssLCtxGet0Certificate(ctx);
    }

    static inline long SslGetVerifyResult(const SSL *ssl)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::sslGetVerifyResult != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::sslGetVerifyResult(ssl);
    }

    static inline const EVP_CIPHER *EvpAes128Gcm()
    {
        VALIDATE_RETURN(OPENSSLAPIDL::evpAes128Gcm != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::evpAes128Gcm();
    }

    static inline const EVP_CIPHER *EvpAes256Gcm()
    {
        VALIDATE_RETURN(OPENSSLAPIDL::evpAes256Gcm != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::evpAes256Gcm();
    }

    static inline EVP_CIPHER_CTX *EvpCipherCtxNew()
    {
        VALIDATE_RETURN(OPENSSLAPIDL::evpCipherCtxNew != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::evpCipherCtxNew();
    }

    static inline void EvpCipherCtxFree(EVP_CIPHER_CTX *ctx)
    {
        VALIDATE_RETURN_VOID(OPENSSLAPIDL::evpCipherCtxFree != nullptr, "openssl handler not loaded");
        OPENSSLAPIDL::evpCipherCtxFree(ctx);
    }

    static inline int EvpCipherCtxCtrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::evpCipherCtxCtrl != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::evpCipherCtxCtrl(ctx, type, arg, ptr);
    }

    static inline int EvpEncryptInitEx(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher, ENGINE *impl,
        const unsigned char *key, const unsigned char *iv)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::evpEncryptInitEx != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::evpEncryptInitEx(ctx, cipher, impl, key, iv);
    }

    static inline int EvpEncryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in,
        int inl)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::evpEncryptUpdate != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::evpEncryptUpdate(ctx, out, outl, in, inl);
    }

    static inline int EvpEncryptFinalEx(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::evpEncryptFinalEx != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::evpEncryptFinalEx(ctx, out, outl);
    }

    static inline int EvpDecryptInitEx(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher, ENGINE *impl,
        const unsigned char *key, const unsigned char *iv)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::evpDecryptInitEx != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::evpDecryptInitEx(ctx, cipher, impl, key, iv);
    }

    static inline int EvpDecryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in,
        int inl)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::evpDecryptUpdate != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::evpDecryptUpdate(ctx, out, outl, in, inl);
    }

    static inline int EvpDecryptFinalEx(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::evpDecryptFinalEx != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::evpDecryptFinalEx(ctx, out, outl);
    }

    static inline int RandPoll()
    {
        VALIDATE_RETURN(OPENSSLAPIDL::randPoll != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::randPoll();
    }

    static inline int RandStatus()
    {
        VALIDATE_RETURN(OPENSSLAPIDL::randStatus != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::randStatus();
    }

    static inline int RandPrivBytes(unsigned char *buf, int num)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::randPrivBytes != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::randPrivBytes(buf, num);
    }

    static inline int X509VerifyCert(X509_STORE_CTX *ctx)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::x509VerifyCert != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::x509VerifyCert(ctx);
    }

    static inline const char *X509VerifyCertErrorString(long n)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::x509VerifyCertErrorString != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::x509VerifyCertErrorString(n);
    }

    static inline int X509StoreCtxGetError(X509_STORE_CTX *ctx)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::x509StoreCtxGetError != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::x509StoreCtxGetError(ctx);
    }

    static inline X509_CRL *PemReadBioX509Crl(BIO *bp, X509_CRL **x, PEM_PASSWORD_CB *cb, void *u)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::pemReadBioX509Crl != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::pemReadBioX509Crl(bp, x, cb, u);
    }

    static inline const BIO_METHOD *BioSFile()
    {
        VALIDATE_RETURN(OPENSSLAPIDL::bioSFile != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::bioSFile();
    }

    static inline EVP_PKEY *PemReadBioPk(BIO *bp, EVP_PKEY **x, PEM_PASSWORD_CB *cb, void *u)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::pemReadBioPk != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::pemReadBioPk(bp, x, cb, u);
    }

    static inline BIO *BioNew(const BIO_METHOD *bioMethod)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::bioNew != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::bioNew(bioMethod);
    }

    static inline BIO *BioNewMemBuf(const void *buf, int len)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::bioNewMemBuf != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::bioNewMemBuf(buf, len);
    }

    static inline long BioCtrl(BIO *bp, int cmd, long larg, void *parg)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::bioCtrl != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::bioCtrl(bp, cmd, larg, parg);
    }

    static inline void BioFree(BIO *b)
    {
        VALIDATE_RETURN_VOID(OPENSSLAPIDL::bioFree != nullptr, "openssl handler not loaded");
        return OPENSSLAPIDL::bioFree(b);
    }

    static inline X509_STORE *X509StoreCtxGet0Store(const X509_STORE_CTX *ctx)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::x509StoreCtxGet0Store != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::x509StoreCtxGet0Store(ctx);
    }

    static inline void X509StoreCtxSetFlags(X509_STORE_CTX *ctx, unsigned long flags)
    {
        VALIDATE_RETURN_VOID(OPENSSLAPIDL::x509StoreCtxSetFlags != nullptr, "openssl handler not loaded");
        return OPENSSLAPIDL::x509StoreCtxSetFlags(ctx, flags);
    }

    static inline int X509StoreAddCrl(X509_STORE *xs, X509_CRL *x)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::x509StoreAddCrl != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::x509StoreAddCrl(xs, x);
    }

    static inline void X509CrlFree(X509_CRL *x)
    {
        VALIDATE_RETURN_VOID(OPENSSLAPIDL::x509CrlFree != nullptr, "openssl handler not loaded");
        return OPENSSLAPIDL::x509CrlFree(x);
    }

    static inline int X509CmpCurrentTime(const ASN1_TIME *s)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::x509CmpCurrentTime != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::x509CmpCurrentTime(s);
    }

    static inline int X509CrlGet0ByCert(X509_CRL *crl, X509_REVOKED **ret, X509 *x)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::x509CrlGet0ByCert != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::x509CrlGet0ByCert(crl, ret, x);
    }

    static inline const ASN1_TIME *X509CrlGet0NextUpdate(const X509_CRL *crl)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::x509CrlGet0NextUpdate != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::x509CrlGet0NextUpdate(crl);
    }

    static inline ASN1_TIME *X509GetNotAfter(const X509 *x)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::x509GetNotAfter != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::x509GetNotAfter(x);
    }

    static inline ASN1_TIME *X509GetNotBefore(const X509 *x)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::x509GetNotBefore != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::x509GetNotBefore(x);
    }

    static inline EVP_PKEY *X509GetPubkey(X509 *x)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::x509GetPubkey != nullptr, "openssl handler not loaded", nullptr);
        return OPENSSLAPIDL::x509GetPubkey(x);
    }

    static inline int EvpPkeyBits(const EVP_PKEY *pkey)
    {
        VALIDATE_RETURN(OPENSSLAPIDL::evpPkeyBits != nullptr, "openssl handler not loaded", -1);
        return OPENSSLAPIDL::evpPkeyBits(pkey);
    }

    static inline void EvpPkeyFree(EVP_PKEY *pkey)
    {
        VALIDATE_RETURN_VOID(OPENSSLAPIDL::evpPkeyFree != nullptr, "openssl handler not loaded");
        return OPENSSLAPIDL::evpPkeyFree(pkey);
    }

    static inline int Load(const std::string &libPsth)
    {
        return OPENSSLAPIDL::LoadOpensslAPI(libPsth);
    }

    static inline void UnLoad()
    {
    }
};
}  // namespace acc
}  // namespace ock
#endif  // ACC_LINKS_OPENSSL_API_WRAPPER_H
