/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <secodefuzz/secodeFuzz.h>
#include "dt_fuzz.h"
#include "mmc_service.h"
#include "mmc_def.h"
#include "mmc_bm_proxy.h"
#include "mmc_local_service_default.h"
#include "mmc_msg_base.h"
#include "acc_tcp_server.h"
#include "memcache_stubs.h"
#include "mmc_config_const.h"

using namespace ock::mmc;
using namespace ock::acc;

namespace {
class TestMmcService : public testing::Test {
public:
    static void SetUpTestSuite();
    static void TearDownTestSuite();

protected:
    static mmc_meta_service_config_t confTemplate;
};

mmc_meta_service_config_t TestMmcService::confTemplate =  {
    .discoveryURL = "tcp://127.0.0.1:5559",
    .logLevel = 0,
    .logRotationFileSize = 2 * 1024 * 1024,
    .logRotationFileCount = 50,
    .evictThresholdHigh = 50,
    .evictThresholdLow = 40,
    .tlsConfig = mmc_tls_config {
        .tlsEnable = false,
    }
};

void TestMmcService::SetUpTestSuite()
{
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(DT_RUNNING_TIME);
}

void TestMmcService::TearDownTestSuite()
{
    GlobalMockObject::verify();
}

class TestMmcServiceMeta : public TestMmcService {
public:
    void SetUp();
    void TearDown();
};

void TestMmcServiceMeta::SetUp()
{
    MockSmemBm();
    MockAccTcpServer *mockTcpServer = new MockAccTcpServer;  // AccRef 自己在析构时会自动调用对这个的 delete
    AccRef<AccTcpServer> refMockTcpServer;
    refMockTcpServer.Set(mockTcpServer);
    MOCKER(&AccTcpServer::Create).stubs().will(returnValue(refMockTcpServer));
}

void TestMmcServiceMeta::TearDown()
{
}

class TestMmcServiceLocal : public TestMmcService {
public:
    void SetUp();
    void TearDown();

protected:
    NetEnginePtr mServer;
};

void TestMmcServiceLocal::SetUp()
{
    MockSmemBm();

    NetEngineOptions mServerOptions;
    mServerOptions.name = "test_engine";
    mServerOptions.ip = "127.0.0.1";
    mServerOptions.port = 5560;
    mServerOptions.threadCount = 2;
    mServerOptions.rankId = 0;
    mServerOptions.tlsOption.tlsEnable = false;
    mServerOptions.startListener = true;

    mServer = NetEngine::Create();
    RegisterMockHandles(mServer);
    ASSERT_EQ(mServer->Start(mServerOptions), 0);
}

void TestMmcServiceLocal::TearDown()
{
}

TEST_F(TestMmcServiceMeta, TestMmcMetaServiceStart)
{
    char fuzzName[] = "TestMmcMetaServiceStart";
    int logLevelEnum[] = {0, 1, 2, 3, 4};
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        std::string discoveryURL = "tcp://127.0.0.1:";
        uint32_t port = *(u32 *)DT_SetGetNumberRangeV3(0, 5559, 0, 65545);
        discoveryURL += std::to_string(port);
        int32_t logLevel = *(s32 *)DT_SetGetNumberEnumV3(1, 0, logLevelEnum, 5);
        uint16_t evictThresholdHigh = *(u16 *)DT_SetGetNumberRangeV3(2, 50, MIN_EVICT_THRESHOLD, MAX_EVICT_THRESHOLD);
        uint16_t evictThresholdLow = *(u16 *)DT_SetGetNumberRangeV3(3, 40, MIN_EVICT_THRESHOLD, MAX_EVICT_THRESHOLD);
        bool tlsEnable = bool(*(u8 *)DT_SetGetNumberRangeV3(4, 0, 0, 1));
        bool isRetNullptr = logLevel < 0 || logLevel > 3;
        isRetNullptr |= evictThresholdHigh <= evictThresholdLow;
        isRetNullptr |= port <= 1024 || port >= 65535;

        mmc_meta_service_config_t conf {
            .logLevel = logLevel,
            .logRotationFileSize = 2 * 1024 * 1024,
            .logRotationFileCount = 50,
            .evictThresholdHigh = evictThresholdHigh,
            .evictThresholdLow = evictThresholdLow,
            .tlsConfig = mmc_tls_config {
                .tlsEnable = tlsEnable,
            }
        };
        std::copy_n(discoveryURL.c_str(), discoveryURL.length() + 1, conf.discoveryURL);
        conf.discoveryURL[discoveryURL.length()] = '\0';
        auto ret = mmcs_meta_service_start(&conf);
        ASSERT_EQ(isRetNullptr, ret == nullptr);
        mmcs_meta_service_stop(ret);
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcServiceMeta, TestMmcMetaServiceStop)
{
    char fuzzName[] = "TestMmcMetaServiceStop";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        auto ret = mmcs_meta_service_start(&confTemplate);
        ASSERT_NE(nullptr, ret);
        mmcs_meta_service_stop(ret);
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcServiceLocal, TestMmcLocalServiceStart)
{
    char fuzzName[] = "TestMmcLocalServiceStart";
    char *dataOpTypes[] = {(char *)"sdma\0", (char *)"roce\0", (char *)"tcp\0", (char *)"notExist\0"};
    int logLevelEnum[] = {0, 1, 2, 3, 4};
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        uint32_t deviceId = *(u32 *)DT_SetGetNumberRangeV3(0, 40, MIN_DEVICE_ID, MAX_DEVICE_ID);
        uint32_t rankId = *(u32 *)DT_SetGetNumberRangeV3(1, 40, MIN_BM_RANK_ID, MAX_BM_RANK_ID);
        uint32_t worldSize = *(u32 *)DT_SetGetNumberRangeV3(2, 40, MIN_WORLD_SIZE, MAX_WORLD_SIZE);
        const char *dataOpType = DT_SetGetStringEnumV3(3, 5, 10, dataOpTypes[0], dataOpTypes, 4);
        uint64_t localDRAMSizeHigh = *(u8 *)DT_SetGetU8V3(4, 0);
        uint64_t localDRAMSizeLow = *(u32 *)DT_SetGetU32V3(5, 1024);
        uint64_t localDRAMSize = (localDRAMSizeHigh << 32) + localDRAMSizeLow; // max 1024 ^ 4
        uint64_t localHBMSizeHigh = *(u8 *)DT_SetGetU8V3(6, 0);
        uint64_t localHBMSizeLow = *(u32 *)DT_SetGetU32V3(7, 0);
        uint64_t localHBMSize = (localHBMSizeHigh << 32) + localHBMSizeLow; // max 1024 ^ 4
        int32_t logLevel = *(s32 *)DT_SetGetNumberEnumV3(8, 0, logLevelEnum, 5);
        int32_t autoRanking = *(s32 *)DT_SetGetS32V3(9, 0);
        uint32_t createId = *(u32 *)DT_SetGetU32V3(10, 0);
        bool isRetNullptr = logLevel < 0 || logLevel > 3;
        isRetNullptr |= !(bool(localDRAMSize) ^ bool(localHBMSize));
        isRetNullptr |= strcmp(dataOpType, dataOpTypes[3]) == 0;
        std::cout << "logLevel: " << logLevel << ", localDRAMSize: " << localDRAMSize << ", localHBMSize: " << localHBMSize << ", dataOpType: " << dataOpType << std::endl;

        mmc_local_service_config_t conf {
            .discoveryURL = "tcp://127.0.0.1:5560",
            .deviceId = deviceId,
            .rankId = rankId,
            .worldSize = worldSize,
            .bmIpPort = "",
            .bmHcomUrl = "",
            .autoRanking = autoRanking,
            .createId = createId,
            .dataOpType = dataOpType,
            .localDRAMSize = localDRAMSize,
            .localHBMSize = localHBMSize,
            .flags = 0,
            .tlsConfig = {
                .tlsEnable = false
            },
            .logLevel = logLevel,
            .logFunc = nullptr
        };
        auto ret = mmcs_local_service_start(&conf);
        ASSERT_EQ(isRetNullptr, ret == nullptr);
        mmcs_local_service_stop(ret);
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcServiceLocal, TestMmcLocalServiceStop)
{
    char fuzzName[] = "TestMmcLocalServiceStop";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        mmc_local_service_config_t conf {
            .discoveryURL = "tcp://127.0.0.1:5560",
            .deviceId = 0,
            .rankId = 0,
            .worldSize = 1,
            .dataOpType = "tcp",
            .localDRAMSize = 0,
            .localHBMSize = 1,
            .logLevel = 0
        };
        auto ret = mmcs_local_service_start(&conf);
        mmcs_local_service_stop(ret);
    }
    DT_FUZZ_END()
}

}