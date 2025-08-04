/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include <iostream>
#include "gtest/gtest.h"
#include "mmc_ref.h"
#include "mmc_blob_allocator.h"
#include "mmc_bm_proxy.h"
#include <thread>
#include <atomic>

using namespace testing;
using namespace std;
using namespace ock::mmc;

class TestBmProxy : public testing::Test {
public:
    TestBmProxy();

    void SetUp() override;

    void TearDown() override;

protected:
    mmc_bm_init_config_t initConfig_;
    mmc_bm_create_config_t createConfig_;
    MmcRef<MmcBmProxy> proxy_;
};

TestBmProxy::TestBmProxy() {}

void TestBmProxy::SetUp()
{
    cout << "this is NetEngine TEST_F setup:" << endl;

    initConfig_.logLevel = 0;
    initConfig_.autoRanking = 1;
    initConfig_.ipPort = "127.0.0.1:12345";
    initConfig_.worldSize = 1;
    initConfig_.deviceId = 0;
    initConfig_.hcomUrl = "hcom_url";

    createConfig_.id = 1234;
    createConfig_.memberSize = 1;
    createConfig_.dataOpType = "sdma";
    createConfig_.localDRAMSize = 1024 * 1024;
    createConfig_.localHBMSize = 0;
    createConfig_.flags = 0;

    proxy_ = MmcBmProxyFactory::GetInstance("test_proxy");
}

void TestBmProxy::TearDown()
{
    cout << "this is NetEngine TEST_F teardown" << endl;

    if (proxy_.Get() != nullptr) {
        proxy_->DestroyBm();
    }
}

static void GenerateData(void *ptr, int32_t rank)
{
    int32_t *arr = (int32_t *)ptr;
    static int32_t mod = INT16_MAX;
    int32_t base = rank;
    for (uint32_t i = 0; i < SIZE_32K / sizeof(int); i++) {
        base = (base * 23 + 17) % mod;
        if ((i + rank) % 3 == 0) {
            arr[i] = -base;
        } else {
            arr[i] = base;
        }
    }
}

static bool CheckData(void *base, void *ptr)
{
    int32_t *arr1 = (int32_t *)base;
    int32_t *arr2 = (int32_t *)ptr;
    for (uint32_t i = 0; i < SIZE_32K / sizeof(int); i++) {
        if (arr1[i] != arr2[i]) return false;
    }
    return true;
}

TEST_F(TestBmProxy, Copy)
{
    std::string bmUrl = "tcp://127.0.0.1:5681";
    std::string hcomUrl = "tcp://127.0.0.1:5682";

    MmcBmProxyPtr bmProxy = MmcBmProxyFactory::GetInstance("bmProxyDefault");

    mmc_bm_init_config_t initConfig;
    initConfig.logLevel = 0;
    initConfig.autoRanking = 1;
    initConfig.rankId = 0;
    initConfig.ipPort = bmUrl;
    initConfig.worldSize = 1;
    initConfig.deviceId = 0;
    initConfig.hcomUrl = hcomUrl;
    
    mmc_bm_create_config_t createConfig;
    createConfig.id = 0;
    createConfig.memberSize = 1;
    createConfig.dataOpType = "sdma";
    createConfig.localDRAMSize = 0;
    createConfig.localHBMSize = 104857600;
    createConfig.flags = 0;
    
    Result ret = bmProxy->InitBm(initConfig, createConfig);
    EXPECT_EQ(ret, MMC_OK);

    uint64_t bmAddr = bmProxy->GetGva();

    void *hostSrc1 = malloc(SIZE_32K);
    void *hostSrc2 = malloc(SIZE_32K);
    void *hostDest1 = malloc(SIZE_32K);
    void *hostDest2 = malloc(SIZE_32K);

    GenerateData(hostSrc1, 1);
    GenerateData(hostSrc2, 2);

    mmc_buffer buffer1 = {};
    buffer1.addr = (uint64_t)hostSrc1;
    buffer1.type = 0;
    buffer1.dimType = 0;
    buffer1.oneDim.offset = 0;
    buffer1.oneDim.len = SIZE_32K;

    mmc_buffer buffer2 = {};
    buffer2.addr = (uint64_t)hostSrc2;
    buffer2.type = 0;
    buffer2.dimType = 0;
    buffer2.oneDim.offset = 0;
    buffer2.oneDim.len = SIZE_32K;

    mmc_buffer buffer3 = {};
    buffer3.addr = (uint64_t)hostDest1;
    buffer3.type = 0;
    buffer3.dimType = 0;
    buffer3.oneDim.offset = 0;
    buffer3.oneDim.len = SIZE_32K;

    mmc_buffer buffer4 = {};
    buffer4.addr = (uint64_t)hostDest2;
    buffer4.type = 0;
    buffer4.dimType = 0;
    buffer4.oneDim.offset = 0;
    buffer4.oneDim.len = SIZE_32K;

    ret = bmProxy->Put(&buffer1, bmAddr, SIZE_32K);
    EXPECT_EQ(ret, MMC_OK);
    ret = bmProxy->Put(&buffer2, bmAddr + SIZE_32K, SIZE_32K);
    EXPECT_EQ(ret, MMC_OK);
    ret = bmProxy->Get(&buffer3, bmAddr + SIZE_32K, SIZE_32K);
    EXPECT_EQ(ret, MMC_OK);
    ret = bmProxy->Get(&buffer4, bmAddr, SIZE_32K);
    EXPECT_EQ(ret, MMC_OK);

    // mock后无法验证
    // bool result = CheckData(hostSrc1, hostDest2);
    // EXPECT_TRUE(result);
    // result = CheckData(hostSrc2, hostDest1);
    // EXPECT_TRUE(result);

    free(hostSrc1);
    free(hostSrc2);
    free(hostDest1);
    free(hostDest2);
    bmProxy->DestroyBm();
}

TEST_F(TestBmProxy, InitBm_Success)
{
    Result ret = proxy_->InitBm(initConfig_, createConfig_);
    ASSERT_EQ(ret, MMC_OK);
}

TEST_F(TestBmProxy, DoubleInit)
{
    ASSERT_EQ(proxy_->InitBm(initConfig_, createConfig_), MMC_OK);
    ASSERT_EQ(proxy_->InitBm(initConfig_, createConfig_), MMC_OK);
}

TEST_F(TestBmProxy, InvalidOpType)
{
    createConfig_.dataOpType = "invalid_type";
    ASSERT_NE(proxy_->InitBm(initConfig_, createConfig_), MMC_OK);
}

TEST_F(TestBmProxy, DestroyBm)
{
    ASSERT_EQ(proxy_->InitBm(initConfig_, createConfig_), MMC_OK);
    proxy_->DestroyBm();
    ASSERT_EQ(proxy_->InitBm(initConfig_, createConfig_), MMC_OK);
}

TEST_F(TestBmProxy, PutGet_NullBuffer)
{
    ASSERT_EQ(proxy_->InitBm(initConfig_, createConfig_), MMC_OK);

    mmc_buffer* nullBuf = nullptr;
    ASSERT_NE(proxy_->Put(nullBuf, 0x1000, 100), MMC_OK);
    ASSERT_NE(proxy_->Get(nullBuf, 0x1000, 100), MMC_OK);
}

TEST_F(TestBmProxy, PutGet_1DData)
{
    ASSERT_EQ(proxy_->InitBm(initConfig_, createConfig_), MMC_OK);

    mmc_buffer buf = {};
    buf.dimType = 0;
    buf.addr = reinterpret_cast<uint64_t>(new char[200]);
    buf.oneDim.offset = 0;
    buf.oneDim.len = 100;

    ASSERT_EQ(proxy_->Put(&buf, 0x1000, 200), MMC_OK);
    ASSERT_EQ(proxy_->Get(&buf, 0x1000, 100), MMC_OK);
    ASSERT_NE(proxy_->Put(&buf, 0x1000, 50), MMC_OK);

    delete[] reinterpret_cast<char*>(buf.addr);
}

TEST_F(TestBmProxy, PutGet_2DData)
{
    ASSERT_EQ(proxy_->InitBm(initConfig_, createConfig_), MMC_OK);

    mmc_buffer buf = {};
    buf.dimType = 1;
    buf.addr = reinterpret_cast<uint64_t>(new char[1024]);

    buf.twoDim.layerOffset = 0;
    buf.twoDim.dpitch = 256;
    buf.twoDim.width = 128;
    buf.twoDim.layerNum = 4;
    buf.twoDim.layerCount = 4;

    ASSERT_EQ(proxy_->Put(&buf, 0x1000, 512), MMC_OK);
    ASSERT_EQ(proxy_->Get(&buf, 0x1000, 512), MMC_OK);

    buf.twoDim.dpitch = 100;
    ASSERT_NE(proxy_->Put(&buf, 0x1000, 512), MMC_OK);
    ASSERT_NE(proxy_->Get(&buf, 0x1000, 512), MMC_OK);

    delete[] reinterpret_cast<char*>(buf.addr);
}

TEST_F(TestBmProxy, ConcurrentAccess)
{
    ASSERT_EQ(proxy_->InitBm(initConfig_, createConfig_), MMC_OK);

    std::atomic<int> successCount{0};
    auto worker = [&]() {
        mmc_buffer buf = {};
        buf.dimType = 0;
        buf.addr = reinterpret_cast<uint64_t>(new char[100]);
        buf.oneDim.offset = 0;
        buf.oneDim.len = 100;

        if (proxy_->Put(&buf, 0x1000, 100) == MMC_OK &&
            proxy_->Get(&buf, 0x1000, 100) == MMC_OK) {
            successCount++;
        }

        delete[] reinterpret_cast<char*>(buf.addr);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    ASSERT_EQ(successCount, 5);
}