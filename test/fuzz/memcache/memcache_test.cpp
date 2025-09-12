/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <secodefuzz/secodeFuzz.h>
#include "dt_fuzz.h"
#include "mmc_service.h"
#include "mmc_def.h"
#include "mmc.h"
#include "mmc_bm_proxy.h"
#include "acc_tcp_server.h"
#include "memcache_stubs.h"
#include "mmc_configuration.h"
#include "memcache_fuzz_common.h"

using namespace ock::mmc;
using namespace ock::acc;

namespace {
class TestMmcFuzzCommon : public TestMmcFuzzBase {
public:
    static void SetUpTestSuite();
    static void TearDownTestSuite();
    void SetUp() override;
    void TearDown() override;
    static std::string mmcLocalConfigPath;
    static bool isConfigValid;
    static mmc_local_service_config_t localServiceConfigFromFile;
};

std::string TestMmcFuzzCommon::mmcLocalConfigPath;
bool TestMmcFuzzCommon::isConfigValid = false;
mmc_local_service_config_t TestMmcFuzzCommon::localServiceConfigFromFile;

void TestMmcFuzzCommon::SetUpTestSuite()
{
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(DT_RUNNING_TIME);

    // 有没有更好的方法？现在要求执行这个用例之前设置环境变量，否则接口返回失败。没法打桩，也没法对常量复制覆盖。
    const char * tempPath = std::getenv("MMC_LOCAL_CONFIG_PATH");
    if (tempPath == nullptr) {
        std::cerr << "[Error] Please set env param MMC_LOCAL_CONFIG_PATH to test! \n";
    }
    ASSERT_NE(tempPath, nullptr);
    mmcLocalConfigPath = tempPath;

    auto clientConfig = new ClientConfig();
    ASSERT_NE(clientConfig, nullptr);
    if (clientConfig->LoadFromFile(mmcLocalConfigPath) && clientConfig->ValidateConf().empty()) {
        isConfigValid = true;
        clientConfig->GetLocalServiceConfig(localServiceConfigFromFile);
        delete clientConfig;
        clientConfig = nullptr;
    }
    delete clientConfig;
}

void TestMmcFuzzCommon::TearDownTestSuite()
{
    GlobalMockObject::verify();
}

void TestMmcFuzzCommon::SetUp()
{
    MockSmemAll();
}

void TestMmcFuzzCommon::TearDown()
{
}

TEST_F(TestMmcFuzzCommon, TestMmcInit)
{
    char fuzzName[] = "TestMmcInit";

    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        std::copy_n(localServiceConfigFromFile.discoveryURL, std::strlen(localServiceConfigFromFile.discoveryURL) + 1,
                    metaServiceConfig.discoveryURL);
        metaServiceConfig.discoveryURL[std::strlen(localServiceConfigFromFile.discoveryURL)] = '\0';
        metaService = mmcs_meta_service_start(&metaServiceConfig);
        ASSERT_NE(metaService, nullptr);

        mmc_init_config conf{0};
        auto ret = mmc_init(conf);
        ASSERT_EQ(ret == 0, isConfigValid);

        ret = mmc_init(conf);
        ASSERT_EQ(ret == 0, true);  // 重复启动

        mmc_uninit();
        mmc_uninit();  // 重复关闭
        mmcs_meta_service_stop(metaService);
    }
    DT_FUZZ_END()
}

}