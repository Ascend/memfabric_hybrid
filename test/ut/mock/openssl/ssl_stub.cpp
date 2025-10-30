/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
*/
#include <cstdio>
#include <cstdint>

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

constexpr int RETURN_OK = 1;
constexpr int RETURN_ERROR = -1;

extern "C" {
int OPENSSL_init_ssl(uint64_t opts, const OPENSSL_INIT_SETTINGS *settings)
{
    return RETURN_OK;
}

int OPENSSL_init_crypto(uint64_t opts, const OPENSSL_INIT_SETTINGS *settings)
{
    return RETURN_OK;
}

void OPENSSL_cleanup()
{
}

const SSL_METHOD *TLS_client_method()
{
    return nullptr;
}

const SSL_METHOD *TLS_method()
{
    return reinterpret_cast<const SSL_METHOD *>(RETURN_OK);
}

const SSL_METHOD *TLS_server_method()
{
    return nullptr;
}

int SSL_shutdown(SSL *s)
{
    return RETURN_OK;
}

int SSL_set_fd(SSL *s, int fd)
{
    return RETURN_OK;
}

SSL *SSL_new(SSL_CTX *ctx)
{
    return (SSL *) RETURN_OK;
}

void SSL_free(SSL *s)
{
}

SSL_CTX *SSL_CTX_new(const SSL_METHOD *method)
{
    return (SSL_CTX *) RETURN_OK;
}

void SSL_CTX_free(SSL_CTX *ctx)
{
}

int SSL_write(SSL *s, const void *buf, int num)
{
    return RETURN_OK;
}

int SSL_read(SSL *s, void *buf, int num)
{
    return RETURN_OK;
}

int SSL_connect(SSL *s)
{
    return RETURN_OK;
}

int SSL_set_connect_state(SSL *s)
{
    return RETURN_OK;
}

int SSL_accept(SSL *s)
{
    return RETURN_OK;
}

int SSL_set_accept_state(SSL *s)
{
    return RETURN_OK;
}

int SSL_get_shutdown(SSL *s)
{
    return RETURN_OK;
}

int SSL_get_error(const SSL *s, int retCode)
{
    return retCode;
}

int SSL_write_ex(SSL *s, const void *buf, size_t num, size_t *written)
{
    return RETURN_OK;
}

int SSL_read_ex(SSL *s, void *buf, size_t num, size_t *readbytes)
{
    return RETURN_OK;
}

int SSL_CTX_set_ciphersuites(SSL_CTX *ctx, const char *str)
{
    return RETURN_OK;
}

const SSL_CIPHER *SSL_get_current_cipher(const SSL *)
{
    return nullptr;
}

long SSL_CTX_ctrl(SSL_CTX *ctx, int cmd, long larg, void *parg)
{
    return RETURN_OK;
}

const char *SSL_get_version(const SSL *ssl)
{
    return nullptr;
}

int SSL_is_server(SSL *ssl)
{
    return RETURN_OK;
}

void SSL_CTX_set_verify(SSL_CTX *ctx, int mode, int (*cb)(int, X509_STORE_CTX *))
{
}

int SSL_CTX_use_PrivateKey(SSL_CTX *ctx, EVP_PKEY *pkey)
{
    return RETURN_OK;
}

int SSL_CTX_use_PrivateKey_file(SSL_CTX *ctx, const char *file, int type)
{
    return RETURN_OK;
}

int SSL_CTX_use_certificate_file(SSL_CTX *ctx, const char *file, int type)
{
    return RETURN_OK;
}

void SSL_CTX_set_default_passwd_cb_userdata(SSL_CTX *ctx, void *u)
{
}

void SSL_CTX_set_cert_verify_callback(SSL_CTX *ctx, int (*cb)(X509_STORE_CTX *, void *), void *arg)
{
}

int SSL_CTX_load_verify_locations(SSL_CTX *ctx, const char *cafile, const char *capath)
{
    return RETURN_OK;
}

int SSL_CTX_check_private_key(const SSL_CTX *ctx)
{
    return RETURN_OK;
}

X509 *SSL_get1_peer_certificate(const SSL *ssl)
{
    return (X509 *) RETURN_OK;
}

X509 *SSL_CTX_get0_certificate(const SSL_CTX *ctx)
{
    return (X509 *) RETURN_OK;
}

long SSL_get_verify_result(const SSL *ssl)
{
    return RETURN_OK;
}
}