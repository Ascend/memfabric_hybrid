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
        return OPENSSLAPIDL::initSsl(opts, settings);
    }

    static inline int OpensslInitCrypto(uint64_t opts, const OPENSSL_INIT_SETTINGS *settings)
    {
        return OPENSSLAPIDL::initCypto(opts, settings);
    }

    static inline const SSL_METHOD *TlsClientMethod()
    {
        return OPENSSLAPIDL::tlsClientMethod();
    }

    static inline const SSL_METHOD *TlsMethod()
    {
        return OPENSSLAPIDL::tlsMethod();
    }

    static inline const SSL_METHOD *TlsServerMethod()
    {
        return OPENSSLAPIDL::tlsServerMethod();
    }

    static inline int SslShutdown(SSL *s)
    {
        return OPENSSLAPIDL::sslShutdown(s);
    }

    static inline int SslSetFd(SSL *s, int fd)
    {
        return OPENSSLAPIDL::sslSetFd(s, fd);
    }

    static inline SSL *SslNew(SSL_CTX *ctx)
    {
        return OPENSSLAPIDL::sslNew(ctx);
    }

    static inline void SslFree(SSL *s)
    {
        OPENSSLAPIDL::sslFree(s);
    }

    static SSL_CTX *SslCtxNew(const SSL_METHOD *method)
    {
        return OPENSSLAPIDL::sslCtxNew(method);
    }

    static inline void SslCtxFree(SSL_CTX *ctx)
    {
        OPENSSLAPIDL::sslCtxFree(ctx);
    }

    static inline int SslWrite(SSL *s, const void *buf, int num)
    {
        return OPENSSLAPIDL::sslWrite(s, buf, num);
    }

    static inline int SslRead(SSL *s, void *buf, int num)
    {
        return OPENSSLAPIDL::sslRead(s, buf, num);
    }

    static inline int SslConnect(SSL *s)
    {
        return OPENSSLAPIDL::sslConnect(s);
    }

    static inline int SslConnectState(SSL *s)
    {
        return OPENSSLAPIDL::sslConnectState(s);
    }

    static inline int SslAccept(SSL *s)
    {
        return OPENSSLAPIDL::sslAccept(s);
    }

    static inline int SslAcceptState(SSL *s)
    {
        return OPENSSLAPIDL::sslAcceptState(s);
    }

    static inline int SslGetShutdown(SSL *s)
    {
        return OPENSSLAPIDL::sslGetShutdown(s);
    }

    static inline int SslGetError(const SSL *s, int retCode)
    {
        return OPENSSLAPIDL::sslGetError(s, retCode);
    }

    static inline int SslWriteEx(SSL *s, const void *buf, size_t num, size_t *written)
    {
        return OPENSSLAPIDL::sslWriteEx(s, buf, num, written);
    }

    static inline int SslReadEx(SSL *s, void *buf, size_t num, size_t *readbytes)
    {
        return OPENSSLAPIDL::sslReadEx(s, buf, num, readbytes);
    }

    static inline int SslCtxSetCipherSuites(SSL_CTX *ctx, const char *str)
    {
        return OPENSSLAPIDL::setCipherSuites(ctx, str);
    }

    static inline long SslCtxCtrl(SSL_CTX *ctx, int cmd, long larg, void *parg)
    {
        return OPENSSLAPIDL::sslCtxCtrl(ctx, cmd, larg, parg);
    }

    static inline const char *SslGetVersion(const SSL *ssl)
    {
        return OPENSSLAPIDL::sslGetVersion(ssl);
    }

    static inline int SslIsServer(SSL *ssl)
    {
        return OPENSSLAPIDL::sslIsServer(ssl);
    }

    static inline void SslCtxSetVerify(SSL_CTX *ctx, int mode, int (*cb)(int, X509_STORE_CTX *))
    {
        OPENSSLAPIDL::sslCtxSetVerify(ctx, mode, cb);
    }

    static inline int SslCtxUsePrivateKeyFile(SSL_CTX *ctx, const char *file, int type)
    {
        return OPENSSLAPIDL::usePrivKeyFile(ctx, file, type);
    }

    static inline int SslCtxUseCertificateFile(SSL_CTX *ctx, const char *file, int type)
    {
        return OPENSSLAPIDL::useCertFile(ctx, file, type);
    }

    static inline X509 *PemReadX509(FILE *fp, X509 **x, PEM_PASSWORD_CB *cb, void *u)
    {
        return OPENSSLAPIDL::pemReadX509(fp, x, cb, u);
    }

    static inline void X509Free(X509 *cert)
    {
        OPENSSLAPIDL::x509Free(cert);
    }

    static inline int Asn1Time2Tm(const ASN1_TIME *s, struct tm *tm)
    {
        return OPENSSLAPIDL::asn1Time2Tm(s, tm);
    }

    static inline void SslCtxSetDefaultPasswdCbUserdata(SSL_CTX *ctx, void *u)
    {
        OPENSSLAPIDL::setDefaultPasswdCbUserdata(ctx, u);
    }

    static inline void SslCtxSetCertVerifyCallback(SSL_CTX *ctx, int (*cb)(X509_STORE_CTX *, void *), void *arg)
    {
        OPENSSLAPIDL::setCertVerifyCallback(ctx, cb, arg);
    }

    static inline int SslCtxLoadVerifyLocations(SSL_CTX *ctx, const char *cafile, const char *capath)
    {
        return OPENSSLAPIDL::loadVerifyLocations(ctx, cafile, capath);
    }

    static inline int SslCtxCheckPrivateKey(const SSL_CTX *ctx)
    {
        return OPENSSLAPIDL::checkPrivateKey(ctx);
    }

    static inline X509 *SslGetPeerCertificate(const SSL *ssl)
    {
        return OPENSSLAPIDL::sslGetPeerCertificate(ssl);
    }

    static inline X509 *SslCtxGet0Certificate(const SSL_CTX *ctx)
    {
        return OPENSSLAPIDL::ssLCtxGet0Certificate(ctx);
    }

    static inline long SslGetVerifyResult(const SSL *ssl)
    {
        return OPENSSLAPIDL::sslGetVerifyResult(ssl);
    }

    static inline const EVP_CIPHER *EvpAes128Gcm()
    {
        return OPENSSLAPIDL::evpAes128Gcm();
    }

    static inline const EVP_CIPHER *EvpAes256Gcm()
    {
        return OPENSSLAPIDL::evpAes256Gcm();
    }

    static inline EVP_CIPHER_CTX *EvpCipherCtxNew()
    {
        return OPENSSLAPIDL::evpCipherCtxNew();
    }

    static inline void EvpCipherCtxFree(EVP_CIPHER_CTX *ctx)
    {
        OPENSSLAPIDL::evpCipherCtxFree(ctx);
    }

    static inline int EvpCipherCtxCtrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr)
    {
        return OPENSSLAPIDL::evpCipherCtxCtrl(ctx, type, arg, ptr);
    }

    static inline int EvpEncryptInitEx(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher, ENGINE *impl,
        const unsigned char *key, const unsigned char *iv)
    {
        return OPENSSLAPIDL::evpEncryptInitEx(ctx, cipher, impl, key, iv);
    }

    static inline int EvpEncryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in,
        int inl)
    {
        return OPENSSLAPIDL::evpEncryptUpdate(ctx, out, outl, in, inl);
    }

    static inline int EvpEncryptFinalEx(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
    {
        return OPENSSLAPIDL::evpEncryptFinalEx(ctx, out, outl);
    }

    static inline int EvpDecryptInitEx(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher, ENGINE *impl,
        const unsigned char *key, const unsigned char *iv)
    {
        return OPENSSLAPIDL::evpDecryptInitEx(ctx, cipher, impl, key, iv);
    }

    static inline int EvpDecryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in,
        int inl)
    {
        return OPENSSLAPIDL::evpDecryptUpdate(ctx, out, outl, in, inl);
    }

    static inline int EvpDecryptFinalEx(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
    {
        return OPENSSLAPIDL::evpDecryptFinalEx(ctx, out, outl);
    }

    static inline int RandPoll()
    {
        return OPENSSLAPIDL::randPoll();
    }

    static inline int RandStatus()
    {
        return OPENSSLAPIDL::randStatus();
    }

    static inline int RandPrivBytes(unsigned char *buf, int num)
    {
        return OPENSSLAPIDL::randPrivBytes(buf, num);
    }

    static inline int X509VerifyCert(X509_STORE_CTX *ctx)
    {
        return OPENSSLAPIDL::x509VerifyCert(ctx);
    }

    static inline const char *X509VerifyCertErrorString(long n)
    {
        return OPENSSLAPIDL::x509VerifyCertErrorString(n);
    }

    static inline int X509StoreCtxGetError(X509_STORE_CTX *ctx)
    {
        return OPENSSLAPIDL::x509StoreCtxGetError(ctx);
    }

    static inline X509_CRL *PemReadBioX509Crl(BIO *bp, X509_CRL **x, PEM_PASSWORD_CB *cb, void *u)
    {
        return OPENSSLAPIDL::pemReadBioX509Crl(bp, x, cb, u);
    }

    static inline const BIO_METHOD *BioSFile(void)
    {
        return OPENSSLAPIDL::bioSFile();
    }

    static inline BIO *BioNew(const BIO_METHOD *bioMethod)
    {
        return OPENSSLAPIDL::bioNew(bioMethod);
    }

    static inline int BioCtrl(BIO *bp, int cmd, long larg, void *parg)
    {
        return OPENSSLAPIDL::bioCtrl(bp, cmd, larg, parg);
    }

    static inline void BioFree(BIO *b)
    {
        return OPENSSLAPIDL::bioFree(b);
    }

    static inline X509_STORE *X509StoreCtxGet0Store(const X509_STORE_CTX *ctx)
    {
        return OPENSSLAPIDL::x509StoreCtxGet0Store(ctx);
    }

    static inline void X509StoreCtxSetFlags(X509_STORE_CTX *ctx, unsigned long flags)
    {
        return OPENSSLAPIDL::x509StoreCtxSetFlags(ctx, flags);
    }

    static inline int X509StoreAddCrl(X509_STORE *xs, X509_CRL *x)
    {
        return OPENSSLAPIDL::x509StoreAddCrl(xs, x);
    }

    static inline void X509CrlFree(X509_CRL *x)
    {
        return OPENSSLAPIDL::x509CrlFree(x);
    }

    static inline int X509CmpCurrentTime(const ASN1_TIME *s)
    {
        return OPENSSLAPIDL::x509CmpCurrentTime(s);
    }

    static inline const ASN1_TIME *X509CrlGet0NextUpdate(const X509_CRL *crl)
    {
        return OPENSSLAPIDL::x509CrlGet0NextUpdate(crl);
    }

    static inline ASN1_TIME *X509GetNotAfter(const X509 *x)
    {
        return OPENSSLAPIDL::x509GetNotAfter(x);
    }

    static inline ASN1_TIME *X509GetNotBefore(const X509 *x)
    {
        return OPENSSLAPIDL::x509GetNotBefore(x);
    }

    static inline EVP_PKEY *X509GetPubkey(X509 *x)
    {
        return OPENSSLAPIDL::x509GetPubkey(x);
    }

    static inline int EvpPkeyBits(const EVP_PKEY *pkey)
    {
        return OPENSSLAPIDL::evpPkeyBits(pkey);
    }

    static inline void EvpPkeyFree(EVP_PKEY *pkey)
    {
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
