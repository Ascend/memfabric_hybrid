/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#ifndef MEMCACHE_FUZZ_COMMON_H
#define MEMCACHE_FUZZ_COMMON_H

#include <gtest/gtest.h>
#include "mmc_def.h"
#include "mmc_meta_service_default.h"

using namespace ock::mmc;

class TestMmcFuzzBase : public testing::Test {
public:
    TestMmcFuzzBase() : localService{nullptr}, metaService{nullptr}
    {
        ock::mf::tls_config tlsConfig {
            .tlsEnable = false,
            .caPath = "certs/ca.cert.pem\0",
            .crlPath = "certs/ca_crl.pem\0",
            .certPath = "certs/server.cert.pem\0",
            .keyPath = "certs/server.private.key.pem\0",
            .keyPassPath = "certs/server.passphrase\0",
            .packagePath = "/opt/ock/security/libs/\0"
        };

        metaServiceConfig = mmc_meta_service_config_t {
            .discoveryURL = "tcp://127.0.0.1:5869\0",
            .haEnable = true,
            .logLevel = 0,
            .logPath = "",
            .logRotationFileSize = 2 * 1024 * 1024,
            .evictThresholdHigh = 70,
            .evictThresholdLow = 60,
            .tlsConfig = tlsConfig
        };

        localServiceConfig = mmc_local_service_config_t {
            .discoveryURL = "tcp://127.0.0.1:5869\0",
            .deviceId = 0,
            .rankId = 0,
            .worldSize = 1,
            .bmIpPort = "tcp://127.0.0.1:5882",
            .bmHcomUrl = "tcp://127.0.0.1:5883",
            .createId = 0,
            .dataOpType = "sdma",
            .localDRAMSize = 104857600,
            .localHBMSize = 104857600,
            .flags = 0,
            .tlsConfig = tlsConfig,
            .logLevel = 0,
            .logFunc = nullptr
        };

        clientConfig = mmc_client_config_t {
            .discoveryURL = "tcp://127.0.0.1:5869\0",
            .rankId = 0,
            .timeOut = 50,
            .logLevel = 0,
            .logFunc = nullptr,
            .tlsConfig = tlsConfig
        };
    }

protected:
    mmc_meta_service_t metaService;
    mmc_local_service_t localService;
    mmc_meta_service_config_t metaServiceConfig;
    mmc_local_service_config_t localServiceConfig;
    mmc_client_config_t clientConfig;
};

#endif // MEMCACHE_FUZZ_COMMON_H