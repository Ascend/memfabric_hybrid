/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include <iostream>
#include "gtest/gtest.h"
#include "mmc_ref.h"
#include "mmc_blob_allocator.h"
#include "mmc_bm_proxy.h"

using namespace testing;
using namespace std;
using namespace ock::mmc;

class TestBmProxy : public testing::Test {
public:
    TestBmProxy();

    void SetUp() override;

    void TearDown() override;

protected:
};
TestBmProxy::TestBmProxy() {}

void TestBmProxy::SetUp()
{
    cout << "this is NetEngine TEST_F setup:";
}

void TestBmProxy::TearDown()
{
    cout << "this is NetEngine TEST_F teardown";
}

static void GenerateData(void *ptr, int32_t rank)
{
    int32_t *arr = (int32_t *)ptr;
    static int32_t mod = INT16_MAX;
    int32_t base = rank;
    for (uint32_t i = 0; i < SIZE_32K / sizeof(int); i++) {
        base = (base * 23 + 17) % mod;
        if ((i + rank) % 3 == 0) {
            arr[i] = -base; // 构造三分之一的负数
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

TEST_F(TestBmProxy, Init)
{
    std::string bmUrl = "tcp://127.0.0.1:5681";
    std::string hcomUrl = "tcp://127.0.0.1:5682";

    MmcBmProxyPtr bmProxy = MmcBmProxyFactory::GetInstance("bmProxyDefault");

    mmc_bm_init_config_t initConfig = {0, 0, 1, bmUrl, hcomUrl, 0};
    mmc_bm_create_config_t createConfig = {0, 1, "sdma", 0, 104857600, 0};
    Result ret = bmProxy->InitBm(initConfig, createConfig);
    EXPECT_EQ(ret, MMC_OK);

    uint64_t bmAddr = bmProxy->GetGva();

    void *hostSrc1 = malloc(SIZE_32K);
    void *hostSrc2 = malloc(SIZE_32K);
    void *hostDest1 = malloc(SIZE_32K);
    void *hostDest2 = malloc(SIZE_32K);

    GenerateData(hostSrc1, 1);
    GenerateData(hostSrc2, 2);

    mmc_buffer buffer1;
    buffer1.addr = (uint64_t)hostSrc1;
    buffer1.type = 0;
    buffer1.dimType = 0;
    buffer1.oneDim.offset = 0;
    buffer1.oneDim.len = SIZE_32K;

    mmc_buffer buffer2;
    buffer2.addr = (uint64_t)hostSrc2;
    buffer2.type = 0;
    buffer2.dimType = 0;
    buffer2.oneDim.offset = 0;
    buffer2.oneDim.len = SIZE_32K;

    mmc_buffer buffer3;
    buffer3.addr = (uint64_t)hostDest1;
    buffer3.type = 0;
    buffer3.dimType = 0;
    buffer3.oneDim.offset = 0;
    buffer3.oneDim.len = SIZE_32K;

    mmc_buffer buffer4;
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

    bool result = CheckData(hostSrc1, hostDest2);
    EXPECT_TRUE(result);
    result = CheckData(hostSrc2, hostDest1);
    EXPECT_TRUE(result);

    free(hostSrc1);
    free(hostSrc2);
    free(hostDest1);
    free(hostDest2);
    bmProxy->DestoryBm();
}