/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include <limits.h>
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <secodefuzz/secodeFuzz.h>
#include "dt_fuzz.h"
#include "mmc_service.h"
#include "mmc_def.h"
#include "mmc.h"
#include "mmc_bm_proxy.h"
#include "mmc_local_service_default.h"
#include "acc_tcp_server.h"
#include "memcache_stubs.h"
#include "mmc_configuration.h"

using namespace ock::mmc;
using namespace ock::acc;

namespace {
class TestMmc : public testing::Test {
public:
    static void SetUpTestSuite();
    static void TearDownTestSuite();
    void SetUp();
    void TearDown();
};

void TestMmc::SetUpTestSuite()
{
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(DT_RUNNING_TIME);
}

void TestMmc::TearDownTestSuite()
{
    GlobalMockObject::verify();
}

void TestMmc::SetUp()
{
    MockSmemBm();
}

void TestMmc::TearDown()
{
}

TEST_F(TestMmc, TestMmcInit)
{
    char fuzzName[] = "TestMmcInit";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        // 有没有更好的方法？现在要求执行这个用例之前设置环境变量，否则接口返回失败。
        // 没法打桩，也没法对常量复制覆盖。
        const char * mmcLocalConfigPath = std::getenv("MMC_LOCAL_CONFIG_PATH");
        bool shouldInitSuccess = false;
        mmc_meta_service_config_t metaServiceConfig =  {
            .logLevel = 0,
            .logRotationFileSize = 2 * 1024 * 1024,
            .logRotationFileCount = 50,
            .evictThresholdHigh = 50,
            .evictThresholdLow = 40,
            .tlsConfig = mmc_tls_config {
                .tlsEnable = false,
            }
        };
        mmc_meta_service_t mmcMetaService = nullptr;
        mmc_local_service_config_t localServiceConfig;
        auto clientConfig = new ClientConfig();
        ASSERT_NE(clientConfig, nullptr);

        if (mmcLocalConfigPath != nullptr) {
            if (clientConfig->LoadFromFile(mmcLocalConfigPath) && clientConfig->ValidateConf().empty()) {
                shouldInitSuccess = true;
                clientConfig->GetLocalServiceConfig(localServiceConfig);
                std::copy_n(localServiceConfig.discoveryURL, std::strlen(localServiceConfig.discoveryURL) + 1,
                            metaServiceConfig.discoveryURL);
                metaServiceConfig.discoveryURL[std::strlen(localServiceConfig.discoveryURL)] = '\0';
                mmcMetaService = mmcs_meta_service_start(&metaServiceConfig);
                delete clientConfig;
                clientConfig = nullptr;
                ASSERT_NE(mmcMetaService, nullptr);
            }
        }

        delete clientConfig;

        auto ret = mmc_init();
        std::cout << "ret:   " << ret << ", shouldInitSuccess: " << shouldInitSuccess << std::endl;
        ASSERT_EQ(ret == 0, shouldInitSuccess);
        mmc_uninit();

        if (mmcMetaService != nullptr) {
            mmcs_meta_service_stop(mmcMetaService);
        }
    }
    DT_FUZZ_END()
}

}