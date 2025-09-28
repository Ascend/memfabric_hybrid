/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#ifndef SMEM_SMEM_TCP_CONFIG_STORE_SSL_HELPER_H
#define SMEM_SMEM_TCP_CONFIG_STORE_SSL_HELPER_H
#include "acc_def.h"
#include "acc_tcp_server.h"
#include "config_store_log.h"
#include "mf_tls_def.h"
#include "mf_tls_util.h"
#include "smem_types.h"

namespace ock {
namespace smem {

inline int PrepareTlsForAccTcpServer(const acc::AccTcpServerPtr &server, tls_config config)
{
    if (server == nullptr) {
        STORE_LOG_ERROR("Invalid input");
        return SM_ERROR;
    }

    if (server->LoadDynamicLib(config.packagePath) != 0) {
        STORE_LOG_ERROR("Failed to load openssl dynamic library");
        return SM_ERROR;
    }

    if (std::string(config.keyPassPath).empty()) {
        STORE_LOG_WARN("No private key password provided, using unencrypted private key");
        return SM_OK;
    }

    if (std::string(config.decrypterLibPath).empty()) {
        STORE_LOG_WARN("No decrypter provided, using default decrypter handler");
        server->RegisterDecryptHandler(mf::MfTlsUtil::DefaultDecrypter);
        return SM_OK;
    }

    const auto decrypter = mf::MfTlsUtil::LoadDecryptFunction(config.decrypterLibPath);
    if (decrypter == nullptr) {
        STORE_LOG_ERROR("failed to load customized decrypt function");
        return SM_ERROR;
    }
    server->RegisterDecryptHandler(decrypter);
    return SM_OK;
}

inline acc::AccTlsOption GetAccTlsOption(const tls_config& config)
{
    acc::AccTlsOption option{};
    option.enableTls = config.tlsEnable;
    option.tlsTopPath = "/";
    option.tlsCaPath = "/";
    option.tlsCrlPath = "/";
    option.tlsCert = config.certPath;
    option.tlsPk = config.keyPath;
    option.tlsPkPwd = config.keyPassPath;
    const std::string caFile = config.caPath;
    if (!caFile.empty()) {
        option.tlsCaFile.insert(caFile);
    }
    const std::string crlFile = config.crlPath;
    if (!crlFile.empty()) {
        option.tlsCrlFile.insert(crlFile);
    }
    return option;
}

}  // namespace smem
}  // namespace ock

#endif