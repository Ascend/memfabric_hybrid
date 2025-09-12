/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <secodefuzz/secodeFuzz.h>
#include "dt_fuzz.h"
#include "mmc_client.h"
#include "mmc_service.h"
#include "mmc_def.h"
#include "mmc_bm_proxy.h"
#include "mmc_msg_base.h"
#include "acc_tcp_server.h"
#include "memcache_stubs.h"
#include "mmc_config_const.h"
#include "memcache_fuzz_common.h"

using namespace ock::mmc;
using namespace ock::acc;

namespace {
class TestMmcFuzzServiceMeta : public TestMmcFuzzBase {
public:
    static void SetUpTestSuite();
    static void TearDownTestSuite();
    void SetUp() override;
    void TearDown() override;
};

void TestMmcFuzzServiceMeta::SetUpTestSuite()
{
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(DT_RUNNING_TIME);
}

void TestMmcFuzzServiceMeta::TearDownTestSuite()
{
    GlobalMockObject::verify();
}

void TestMmcFuzzServiceMeta::SetUp()
{
    MockSmemAll();
}

void TestMmcFuzzServiceMeta::TearDown()
{
}

TEST_F(TestMmcFuzzServiceMeta, TestMmcMetaServiceStart)
{
    char fuzzName[] = "TestMmcMetaServiceStart";
    int logLevelEnum[] = {0, 1, 2, 3, 4};

    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        mmc_meta_service_config_t metaServiceConfig(this->metaServiceConfig);
        metaServiceConfig.logLevel = *(s32 *)DT_SetGetNumberEnum(&g_Element[0], 0, logLevelEnum, 5);
        metaServiceConfig.evictThresholdHigh = *(u16 *)DT_SetGetNumberRange(&g_Element[1], 50, 0, MAX_EVICT_THRESHOLD);
        metaServiceConfig.evictThresholdLow = *(u16 *)DT_SetGetNumberRange(&g_Element[2], 40, 0, MAX_EVICT_THRESHOLD);

        bool isRetNullptr = metaServiceConfig.logLevel < 0 || metaServiceConfig.logLevel > 3;
        isRetNullptr |= metaServiceConfig.evictThresholdHigh <= metaServiceConfig.evictThresholdLow;
        isRetNullptr |= metaServiceConfig.evictThresholdLow < MIN_EVICT_THRESHOLD;
        isRetNullptr |= metaServiceConfig.evictThresholdHigh > MAX_EVICT_THRESHOLD;

        auto ret = mmcs_meta_service_start(&metaServiceConfig);
        ASSERT_EQ(isRetNullptr, ret == nullptr);
        mmcs_meta_service_stop(ret);
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcFuzzServiceMeta, TestMmcMetaServiceStop)
{
    char fuzzName[] = "TestMmcMetaServiceStop";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        auto ret = mmcs_meta_service_start(&metaServiceConfig);
        ASSERT_NE(nullptr, ret);
        mmcs_meta_service_stop(ret);
    }
    DT_FUZZ_END()
}


class TestMmcFuzzServiceLocal : public TestMmcFuzzBase {
public:
    static void SetUpTestSuite();
    static void TearDownTestSuite();
    void SetUp() override;
    void TearDown() override;
};

void TestMmcFuzzServiceLocal::SetUpTestSuite()
{
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(DT_RUNNING_TIME);
}

void TestMmcFuzzServiceLocal::TearDownTestSuite()
{
    GlobalMockObject::verify();
}

void TestMmcFuzzServiceLocal::SetUp()
{
    MockSmemAll();
}

void TestMmcFuzzServiceLocal::TearDown()
{
}

TEST_F(TestMmcFuzzServiceLocal, TestMmcLocalServiceStart)
{
    char fuzzName[] = "TestMmcLocalServiceStart";
    char *dataOpTypes[] = {(char *)"sdma\0", (char *)"roce\0", (char *)"tcp\0", (char *)"notExist\0"};
    int logLevelEnum[] = {0, 1, 2, 3, 4};
    const char *urls[] = {
        localServiceConfig.discoveryURL,
        "",
        "tcp:",
        "tcp://127.0.0.1:",
        "tcp://299.99.99.99:1234",
        "tcp://299.99.99.99:123456"
    };
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        mmc_local_service_config_t localServiceConfig(this->localServiceConfig);
        localServiceConfig.deviceId = *(u32 *)DT_SetGetNumberRange(&g_Element[0], 40, MIN_DEVICE_ID, MAX_DEVICE_ID);
        localServiceConfig.worldSize = *(u32 *)DT_SetGetNumberRange(&g_Element[1], 40, MIN_WORLD_SIZE, MAX_WORLD_SIZE);
        localServiceConfig.dataOpType = DT_SetGetStringEnum(&g_Element[2], 5, 10, dataOpTypes[0], dataOpTypes, 4);
        localServiceConfig.logLevel = *(s32 *)DT_SetGetNumberEnum(&g_Element[3], 0, logLevelEnum, 5);
        localServiceConfig.createId = *(u32 *)DT_SetGetU32(&g_Element[4], 0);
        localServiceConfig.localDRAMSize = *(u64 *)DT_SetGetU64(&g_Element[5], 0);
        localServiceConfig.localHBMSize = *(u64 *)DT_SetGetU64(&g_Element[6], 0);
        int urlIdx = *(u32 *)DT_SetGetNumberRange(&g_Element[7], 0, 0, 5);
        std::copy_n(urls[urlIdx], std::strlen(urls[urlIdx]), localServiceConfig.discoveryURL);
        localServiceConfig.discoveryURL[std::strlen(urls[urlIdx])] = '\0';

        bool isRetNullptr = localServiceConfig.logLevel < 0 || localServiceConfig.logLevel > 3;
        isRetNullptr |= strcmp(localServiceConfig.dataOpType.c_str(), dataOpTypes[3]) == 0;
        isRetNullptr |= urlIdx != 0;

        ASSERT_NE(metaService = mmcs_meta_service_start(&metaServiceConfig), nullptr);
        localService = mmcs_local_service_start(&localServiceConfig);
        ASSERT_EQ(localService == nullptr, isRetNullptr);

        if (!isRetNullptr) {  // skip time wait
            mmcc_init(&clientConfig);
        }
        mmc_data_info infoQuery{.valid = false};
        mmc_buffer buf{.addr = 1, .type = 0, .dimType = 0, .oneDim = {.offset = 0, .len = 0}};
        mmc_put_options opt{0, NATIVE_AFFINITY};
        ASSERT_EQ(mmcc_put("test", &buf, opt, 0) != 0, isRetNullptr);
        ASSERT_EQ(mmcc_get("test", &buf, 0) != 0, isRetNullptr);
        ASSERT_EQ(mmcc_exist("test", 0) != 0, isRetNullptr);
        ASSERT_EQ(mmcc_query("test", &infoQuery, 0) != 0, isRetNullptr);
        ASSERT_EQ(mmcc_remove("test", 0) != 0, isRetNullptr);

        mmcs_local_service_stop(localService);
        mmcc_uninit();
        mmcs_meta_service_stop(metaService);
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcFuzzServiceLocal, TestMmcLocalServiceStop)
{
    char fuzzName[] = "TestMmcLocalServiceStop";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        ASSERT_NE(metaService = mmcs_meta_service_start(&metaServiceConfig), nullptr);
        ASSERT_NE(localService = mmcs_local_service_start(&localServiceConfig), nullptr);
        mmcs_local_service_stop(localService);
        mmcs_meta_service_stop(metaService);
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcFuzzServiceLocal, TestMmcLocalServiceTimeout)
{
    char fuzzName[] = "TestMmcLocalServiceTimeout";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        ASSERT_EQ(localService = mmcs_local_service_start(&localServiceConfig), nullptr);
        mmcs_local_service_stop(localService);
    }
    DT_FUZZ_END()
}

}