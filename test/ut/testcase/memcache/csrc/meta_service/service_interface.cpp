/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include <iostream>
#include "gtest/gtest.h"
#include "mmc_service.h"
#include "mmc_client.h"
#include "mmc_blob_allocator.h"

using namespace testing;
using namespace std;

class TestMmcServiceInterface : public testing::Test {
public:
    TestMmcServiceInterface();

    void SetUp() override;

    void TearDown() override;

protected:
};
TestMmcServiceInterface::TestMmcServiceInterface() {}

void TestMmcServiceInterface::SetUp()
{
    cout << "this is NetEngine TEST_F setup:";
}

void TestMmcServiceInterface::TearDown()
{
    cout << "this is NetEngine TEST_F teardown";
}

static void UrlStringToChar(std::string &urlString, char *urlChar)
{
    for (uint32_t i = 0; i < urlString.length(); i++) {
        urlChar[i] = urlString.at(i);
    }
    urlChar[urlString.length()] = '\0';
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

TEST_F(TestMmcServiceInterface, metaServiceStart)
{
    std::string metaUrl = "tcp://127.0.0.1:5868";
    std::string bmUrl = "tcp://127.0.0.1:5881";
    std::string localUrl = "";
    mmc_meta_service_config_t metaServiceConfig;
    UrlStringToChar(metaUrl, metaServiceConfig.discoveryURL);
    metaServiceConfig.worldSize = 1;
    mmc_meta_service_t meta_service = mmcs_meta_service_start(&metaServiceConfig);
    ASSERT_TRUE(meta_service != nullptr);

    mmc_local_service_config_t localServiceConfig = {"", 0, 0, 1, bmUrl, 0, 0, "sdma", 0, 104857600, 0};
    UrlStringToChar(metaUrl, localServiceConfig.discoveryURL);
    mmc_meta_service_t local_service = mmcs_local_service_start(&localServiceConfig);
    ASSERT_TRUE(local_service != nullptr);

    mmc_client_config_t clientConfig;
    clientConfig.rankId = 0;
    UrlStringToChar(metaUrl, clientConfig.discoveryURL);
    int32_t ret = mmcc_init(&clientConfig);
    ASSERT_TRUE(ret == 0);
    std::string test = "test";

    void *hostSrc = malloc(SIZE_32K);
    void *hostDest = malloc(SIZE_32K);

    GenerateData(hostSrc, 1);

    mmc_buffer buffer;
    buffer.addr = (uint64_t)hostSrc;
    buffer.type = 0;
    buffer.dram.offset = 0;
    buffer.dram.len = SIZE_32K;

    mmc_put_options options{0, NATIVE_AFFINITY};
    ret = mmcc_put(test.c_str(), &buffer, options, 0);
    ASSERT_TRUE(ret == 0);

    mmc_buffer readBuffer;
    readBuffer.addr = (uint64_t)hostDest;
    readBuffer.type = 0;
    readBuffer.dram.offset = 0;
    readBuffer.dram.len = SIZE_32K;

    ret = mmcc_get(test.c_str(), &readBuffer, 0);
    ASSERT_TRUE(ret == 0);

    bool result = CheckData(hostSrc, hostDest);
    EXPECT_TRUE(result);

    mmc_location_t location = mmcc_get_location(test.c_str(), 0);
    ASSERT_TRUE(location.xx == 0);

    ret = mmcc_remove(test.c_str(), 0);
    ASSERT_TRUE(ret == 0);
    free(hostSrc);
    free(hostDest);

    const int BATCH_SIZE = 3;
    std::vector<std::string> keys = {"batch_test1", "batch_test2", "batch_test3"};
    std::vector<void *> batchHostSrc(BATCH_SIZE);
    std::vector<void *> batchHostDest(BATCH_SIZE);
    std::vector<mmc_buffer> batchBuffers(BATCH_SIZE);
    std::vector<mmc_buffer> batchReadBuffers(BATCH_SIZE);

    for (int i = 0; i < BATCH_SIZE; ++i) {
        batchHostSrc[i] = malloc(SIZE_32K);
        batchHostDest[i] = malloc(SIZE_32K);
        GenerateData(batchHostSrc[i], i + 1);

        batchBuffers[i].addr = (uint64_t)batchHostSrc[i];
        batchBuffers[i].type = 0;
        batchBuffers[i].dram.offset = 0;
        batchBuffers[i].dram.len = SIZE_32K;

        batchReadBuffers[i].addr = (uint64_t)batchHostDest[i];
        batchReadBuffers[i].type = 0;
        batchReadBuffers[i].dram.offset = 0;
        batchReadBuffers[i].dram.len = SIZE_32K;
    }

    ret = mmcc_batch_put(keys, batchBuffers, options, 0);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < BATCH_SIZE; ++i) {
        ret = mmcc_get(keys[i].c_str(), &batchReadBuffers[i], 0);
        EXPECT_EQ(ret, 0);
        bool batchResult = CheckData(batchHostSrc[i], batchHostDest[i]);
        EXPECT_TRUE(batchResult);

        mmc_location_t batchLocation = mmcc_get_location(keys[i].c_str(), 0);
        EXPECT_EQ(batchLocation.xx, 0);

        ret = mmcc_remove(keys[i].c_str(), 0);
        EXPECT_EQ(ret, 0);
    }

    for (int i = 0; i < BATCH_SIZE; ++i) {
        free(batchHostSrc[i]);
        free(batchHostDest[i]);
    }

    mmcs_local_service_stop(local_service);
    mmcs_meta_service_stop(meta_service);
}
