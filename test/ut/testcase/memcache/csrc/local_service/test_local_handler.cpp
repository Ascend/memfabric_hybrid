/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "mmc_meta_manager.h"
#include "mmc_bm_proxy.h"
#include "mmc_ref.h"
#include "gtest/gtest.h"
#include <iostream>
#include <memory>

using namespace testing;
using namespace std;
using namespace ock::mmc;

class TestLocalHandler : public testing::Test {
public:
    TestLocalHandler();

    void SetUp() override;

    void TearDown() override;

protected:
};
TestLocalHandler::TestLocalHandler() {}

void TestLocalHandler::SetUp()
{
    cout << "this is Meta Manger TEST_F setup:" << std::endl;
}

void TestLocalHandler::TearDown()
{
    cout << "this is Meta Manager TEST_F teardown" << std::endl;
}

TEST_F(TestLocalHandler, Init)
{
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{100, 1000};

    uint64_t defaultTtl = 2000;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl, 70, 60);
    metaMng->Start();
    metaMng->Mount(loc, locInfo);
    ASSERT_TRUE(metaMng != nullptr);
    metaMng->Stop();
}

TEST_F(TestLocalHandler, Alloc)
{
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 1000000};

    uint64_t defaultTtl = 2000;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl, 70, 60);
    metaMng->Start();
    metaMng->Mount(loc, locInfo);

    AllocOptions allocReq{SIZE_32K, 1, 0, 0, 0};  // blobSize, numBlobs, mediaType, preferredRank, flags
    MmcMemObjMetaPtr objMeta;
    Result ret = metaMng->Alloc("test_string", allocReq, 1, objMeta);
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(objMeta->NumBlobs() == 1);
    ASSERT_TRUE(objMeta->Size() == SIZE_32K);

    metaMng->UpdateState("test_string", loc, 0, 1, MMC_WRITE_OK);

    ret = metaMng->Remove("test_string");
    ASSERT_TRUE(ret == MMC_OK);
    metaMng->Stop();
}

class TestMmcBmProxy : public testing::Test {
protected:
    void SetUp() override
    {
        proxy = MmcMakeRef<MmcBmProxy>("test_proxy");

        initConfig = {
            .deviceId = 0,
            .rankId = 0,
            .worldSize = 4,
            .ipPort = "127.0.0.1:5000",
            .hcomUrl = "tcp://127.0.0.1:5001",
            .autoRanking = 1,
            .logLevel = 0,
            .logFunc = nullptr
        };
        
        createConfig = {
            .id = 12345,
            .memberSize = 4,
            .dataOpType = "sdma",
            .localDRAMSize = 1024 * 1024,
            .localHBMSize = 1024 * 1024 * 2,
            .flags = 0
        };

        oneDimBuffer = {
            .addr = 0x1000,
            .type = 0,
            .dimType = 0,
            .oneDim = {.offset = 0, .len = 1024}
        };

        twoDimBuffer = {
            .addr = 0x1000,
            .type = 0,
            .dimType = 1,
            .twoDim = {
                .dpitch = 512,
                .layerOffset = 0,
                .width = 256,
                .layerNum = 4,
                .layerCount = 1
            }
        };
    }

    Result InitBmWithConfig()
    {
        return proxy->InitBm(initConfig, createConfig);
    }

    MmcRef<MmcBmProxy> proxy;
    mmc_bm_init_config_t initConfig;
    mmc_bm_create_config_t createConfig;
    mmc_buffer oneDimBuffer;
    mmc_buffer twoDimBuffer;
};

TEST_F(TestMmcBmProxy, InitBm_Success)
{
    Result ret = InitBmWithConfig();
    EXPECT_EQ(ret, MMC_OK);
}

TEST_F(TestMmcBmProxy, InitBm_AlreadyStarted)
{
    InitBmWithConfig();
    Result ret = InitBmWithConfig();
    EXPECT_EQ(ret, MMC_OK);
}

TEST_F(TestMmcBmProxy, InitBm_InvalidOpType)
{
    createConfig.dataOpType = "invalid_type";
    Result ret = InitBmWithConfig();
    EXPECT_EQ(ret, MMC_ERROR);
}

TEST_F(TestMmcBmProxy, DestroyBm_Success)
{
    InitBmWithConfig();
    proxy->DestoryBm();
    EXPECT_EQ(proxy->GetGva(), 0UL);
}

TEST_F(TestMmcBmProxy, DestroyBm_NotStarted)
{
    proxy->DestoryBm();
    EXPECT_EQ(proxy->GetGva(), 0UL);
}

TEST_F(TestMmcBmProxy, Put_OneDimSuccess)
{
    InitBmWithConfig();
    Result ret = proxy->Put(&oneDimBuffer, 0x2000, 1024);
    EXPECT_EQ(ret, MMC_OK);
}

TEST_F(TestMmcBmProxy, Put_OneDimSizeExceed)
{
    InitBmWithConfig();
    oneDimBuffer.oneDim.len = 2048;
    Result ret = proxy->Put(&oneDimBuffer, 0x2000, 1024);
    EXPECT_EQ(ret, MMC_ERROR);
}

TEST_F(TestMmcBmProxy, Put_TwoDimSuccess)
{
    InitBmWithConfig();
    Result ret = proxy->Put(&twoDimBuffer, 0x2000, 1024);
    EXPECT_EQ(ret, MMC_OK);
}

TEST_F(TestMmcBmProxy, Put_TwoDimDpitchTooSmall)
{
    InitBmWithConfig();
    twoDimBuffer.twoDim.dpitch = 256;
    twoDimBuffer.twoDim.width = 512;
    Result ret = proxy->Put(&twoDimBuffer, 0x2000, 1024);
    EXPECT_EQ(ret, MMC_ERROR);
}

TEST_F(TestMmcBmProxy, Put_InvalidBufferType)
{
    InitBmWithConfig();
    mmc_buffer invalidBuffer = {.dimType = 2};
    Result ret = proxy->Put(&invalidBuffer, 0x2000, 1024);
    EXPECT_EQ(ret, MMC_ERROR);
}

TEST_F(TestMmcBmProxy, Put_NullBuffer)
{
    InitBmWithConfig();
    Result ret = proxy->Put(nullptr, 0x2000, 1024);
    EXPECT_EQ(ret, MMC_ERROR);
}

TEST_F(TestMmcBmProxy, Get_OneDimSuccess)
{
    InitBmWithConfig();
    Result ret = proxy->Get(&oneDimBuffer, 0x2000, 1024);
    EXPECT_EQ(ret, MMC_OK);
}

TEST_F(TestMmcBmProxy, Get_OneDimSizeMismatch)
{
    InitBmWithConfig();
    oneDimBuffer.oneDim.len = 512;
    Result ret = proxy->Get(&oneDimBuffer, 0x2000, 1024);
    EXPECT_EQ(ret, MMC_ERROR);
}

TEST_F(TestMmcBmProxy, Get_TwoDimSuccess)
{
    InitBmWithConfig();
    Result ret = proxy->Get(&twoDimBuffer, 0x2000, 1024);
    EXPECT_EQ(ret, MMC_OK);
}

TEST_F(TestMmcBmProxy, Get_TwoDimSizeMismatch)
{
    InitBmWithConfig();
    twoDimBuffer.twoDim.layerNum = 8;
    Result ret = proxy->Get(&twoDimBuffer, 0x2000, 1024);
    EXPECT_EQ(ret, MMC_ERROR);
}

TEST_F(TestMmcBmProxy, Get_TwoDimDpitchTooSmall)
{
    InitBmWithConfig();
    twoDimBuffer.twoDim.dpitch = 256;
    twoDimBuffer.twoDim.width = 512;
    Result ret = proxy->Get(&twoDimBuffer, 0x2000, 1024);
    EXPECT_EQ(ret, MMC_ERROR);
}

TEST_F(TestMmcBmProxy, Get_InvalidBufferType)
{
    InitBmWithConfig();
    mmc_buffer invalidBuffer = {.dimType = 12};
    Result ret = proxy->Get(&invalidBuffer, 0x2000, 1024);
    EXPECT_EQ(ret, MMC_ERROR);
}

TEST_F(TestMmcBmProxy, Get_NullBuffer)
{
    InitBmWithConfig();
    Result ret = proxy->Get(nullptr, 0x2000, 1024);
    EXPECT_EQ(ret, MMC_ERROR);
}

TEST_F(TestMmcBmProxy, Get_NotInitialized)
{
    Result ret = proxy->Get(&oneDimBuffer, 0x2000, 1024);
    EXPECT_EQ(ret, MMC_ERROR);
}

TEST_F(TestMmcBmProxy, Put_NotInitialized)
{
    Result ret = proxy->Put(&oneDimBuffer, 0x2000, 1024);
    EXPECT_EQ(ret, MMC_ERROR);
}

TEST(MmcBmProxyFactory, GetInstance)
{
    auto proxy1 = MmcBmProxyFactory::GetInstance("proxy1");
    auto proxy2 = MmcBmProxyFactory::GetInstance("proxy2");
    auto proxy1_again = MmcBmProxyFactory::GetInstance("proxy1");
    
    EXPECT_NE(proxy1.Get(), nullptr);
    EXPECT_NE(proxy2.Get(), nullptr);
    EXPECT_EQ(proxy1.Get(), proxy1_again.Get());
}
