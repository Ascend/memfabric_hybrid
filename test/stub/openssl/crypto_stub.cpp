#include <cstdio>
#include <ctime>

/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
*/
constexpr int RETURN_OK = 1;
constexpr int RETURN_ERROR = -1;
constexpr int EVP_KEY_LENGTH = 4096;

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

extern "C" {
const EVP_CIPHER *EVP_aes_128_gcm()
{
    return nullptr;
}

const EVP_CIPHER *EVP_aes_256_gcm()
{
    return nullptr;
}

EVP_CIPHER_CTX *EVP_CIPHER_CTX_new()
{
    return nullptr;
}

void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX *ctx)
{
}

int EVP_CIPHER_CTX_ctrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr)
{
    return RETURN_OK;
}

int EVP_EncryptInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher, ENGINE *impl,
                       const unsigned char *key, const unsigned char *iv)
{
    return RETURN_OK;
}

int EVP_EncryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in,
                      int inl)
{
    return RETURN_OK;
}

int EVP_EncryptFinal_ex(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
{
    return RETURN_OK;
}

int EVP_DecryptInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher, ENGINE *impl,
                       const unsigned char *key, const unsigned char *iv)
{
    return RETURN_OK;
}

int EVP_DecryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in,
                      int inl)
{
    return RETURN_OK;
}

int EVP_DecryptFinal_ex(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
{
    return RETURN_OK;
}

int RAND_poll()
{
    return RETURN_OK;
}

int RAND_status()
{
    return RETURN_OK;
}

int RAND_bytes(unsigned char *buf, int num)
{
    return RETURN_OK;
}

int RAND_priv_bytes(unsigned char *buf, int num)
{
    return RETURN_OK;
}

void RAND_seed(void *buf, int num)
{
}

int X509_verify_cert(X509_STORE_CTX *ctx)
{
    return RETURN_OK;
}

const char *X509_verify_cert_error_string(long n)
{
    return nullptr;
}

int X509_STORE_CTX_get_error(X509_STORE_CTX *ctx)
{
    return RETURN_OK;
}

X509_CRL *PEM_read_bio_X509_CRL(BIO *bp, X509_CRL **x, PEM_PASSWORD_CB *cb, void *u)
{
    return nullptr;
}

BIO_METHOD *BIO_s_file(void)
{
    return nullptr;
}

BIO *BIO_new(const BIO_METHOD *bioMethod)
{
    return nullptr;
}

int BIO_ctrl(BIO *bp, int cmd, long larg, void *parg)
{
    return RETURN_OK;
}

void BIO_free(BIO *b)
{
}

X509_STORE *X509_STORE_CTX_get0_store(const X509_STORE_CTX *ctx)
{
    return nullptr;
}

void X509_STORE_CTX_set_flags(X509_STORE_CTX *ctx, unsigned long flags)
{
}

int X509_STORE_add_crl(X509_STORE *xs, X509_CRL *x)
{
    return RETURN_OK;
}

void X509_CRL_free(X509_CRL *x)
{
}

int X509_cmp_current_time(const ASN1_TIME *s)
{
    return 0;
}

ASN1_TIME *X509_CRL_get0_nextUpdate(const X509_CRL *crl)
{
    return (ASN1_TIME *) RETURN_OK;
}

ASN1_TIME *X509_getm_notAfter(const X509 *x)
{
    return (ASN1_TIME *) RETURN_OK;
}

ASN1_TIME *X509_getm_notBefore(const X509 *x)
{
    return (ASN1_TIME *) RETURN_OK;
}

EVP_PKEY *X509_get_pubkey(X509 *x)
{
    return nullptr;
}

int EVP_PKEY_get_bits(const EVP_PKEY *pkey)
{
    return EVP_KEY_LENGTH;
}

void EVP_PKEY_free(EVP_PKEY *pkey)
{
}

X509 *PEM_read_X509(FILE *fp, X509 **x, pem_password_cb *cb, void *u)
{
    return (X509 *) RETURN_OK;
}

X509 *X509_free(X509 *cert)
{
    return nullptr;
}

int ASN1_TIME_to_tm(const ASN1_TIME *s, struct tm *tm)
{
    time_t now = time(nullptr) + 60 * 60 * 24 * 30;
    *tm = *gmtime(&now);
    return RETURN_OK;
}
}