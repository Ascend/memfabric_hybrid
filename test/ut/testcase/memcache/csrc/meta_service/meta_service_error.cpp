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

class TestMmcServiceError : public testing::Test {
public:
    TestMmcServiceError();

    void SetUp() override;

    void TearDown() override;

protected:
};
TestMmcServiceError::TestMmcServiceError() {}

void TestMmcServiceError::SetUp()
{
    cout << "this is NetEngine TEST_F setup:";
}

void TestMmcServiceError::TearDown()
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

TEST_F(TestMmcServiceError, metaService)
{
    std::string metaUrl = "tcp://127.0.0.1:5868";
    std::string bmUrl = "tcp://127.0.0.1:5881";
    std::string hcomUrl = "tcp://127.0.0.1:5882";
    std::string localUrl = "";
    mmc_meta_service_config_t metaServiceConfig;
    metaServiceConfig.logLevel = 0;
    metaServiceConfig.logRotationFileSize = 2 * 1024 * 1024;
    metaServiceConfig.logRotationFileCount = 20;
    metaServiceConfig.evictThresholdHigh = 70;
    metaServiceConfig.evictThresholdLow = 60;
    metaServiceConfig.metaRebuildEnable = true;
    metaServiceConfig.tlsConfig.tlsEnable = false;
    UrlStringToChar(metaUrl, metaServiceConfig.discoveryURL);
    mmc_meta_service_t meta_service = mmcs_meta_service_start(&metaServiceConfig);
    ASSERT_TRUE(meta_service != nullptr);

    mmc_local_service_config_t localServiceConfig = {"", 0, 0, 1, bmUrl, hcomUrl, 0, "sdma", 0, 104857600, 0};
    localServiceConfig.logLevel = 0;
    localServiceConfig.tlsConfig.tlsEnable = false;
    UrlStringToChar(metaUrl, localServiceConfig.discoveryURL);
    mmc_meta_service_t local_service = mmcs_local_service_start(&localServiceConfig);
    ASSERT_TRUE(local_service != nullptr);

    mmc_client_config_t clientConfig;
    clientConfig.logLevel = 0;
    clientConfig.tlsConfig.tlsEnable = false;
    clientConfig.rankId = 0;
    UrlStringToChar(metaUrl, clientConfig.discoveryURL);
    int32_t ret = mmcc_init(&clientConfig);
    ASSERT_TRUE(ret == 0);
    mmcs_meta_service_stop(meta_service);

    sleep(3);

    meta_service = mmcs_meta_service_start(&metaServiceConfig);
    ASSERT_TRUE(meta_service != nullptr);

    sleep(3);

    std::string test = "test";

    void *hostSrc = malloc(SIZE_32K);
    void *hostDest = malloc(SIZE_32K);

    GenerateData(hostSrc, 1);

    mmc_buffer buffer;
    buffer.addr = (uint64_t)hostSrc;
    buffer.type = 0;
    buffer.dimType = 0;
    buffer.oneDim.offset = 0;
    buffer.oneDim.len = SIZE_32K;

    mmc_put_options options{0, NATIVE_AFFINITY};
    ret = mmcc_put(test.c_str(), &buffer, options, 0);
    ASSERT_TRUE(ret == 0);

    mmc_buffer readBuffer;
    readBuffer.addr = (uint64_t)hostDest;
    readBuffer.type = 0;
    readBuffer.dimType = 0;
    readBuffer.oneDim.offset = 0;
    readBuffer.oneDim.len = SIZE_32K;

    ret = mmcc_get(test.c_str(), &readBuffer, 0);
    ASSERT_TRUE(ret == 0);

    // bool result = CheckData(hostSrc, hostDest);
    // EXPECT_TRUE(result);

    mmc_location_t location = mmcc_get_location(test.c_str(), 0);
    ASSERT_TRUE(location.xx == 0);

    ret = mmcc_remove(test.c_str(), 0);
    ASSERT_TRUE(ret == 0);

    const char* keys[] = {"test1", "test2"};
    uint32_t keys_count = sizeof(keys) / sizeof(keys[0]);
    void* hostSrcs[keys_count];
    void* hostDests[keys_count];
    mmc_buffer bufs[keys_count];

    for (uint32_t i = 0; i < keys_count; ++i) {
        hostSrcs[i] = malloc(SIZE_32K);
        hostDests[i] = malloc(SIZE_32K);
        GenerateData(hostSrcs[i], 1);

        bufs[i].addr = (uint64_t)hostSrcs[i];
        bufs[i].type = 0;
        bufs[i].dimType = 0;
        bufs[i].oneDim.offset = 0;
        bufs[i].oneDim.len = SIZE_32K;
    }
    std::vector<int> results(keys_count, -1);
    ret = mmcc_batch_put(keys, keys_count, bufs, options, 0, results.data());
    ASSERT_TRUE(ret == 0);

    for (uint32_t i = 0; i < keys_count; ++i) {
        mmc_buffer readBuffer;
        readBuffer.addr = (uint64_t)hostDests[i];
        readBuffer.type = 0;
        readBuffer.dimType = 0;
        readBuffer.oneDim.offset = 0;
        readBuffer.oneDim.len = SIZE_32K;

        ret = mmcc_get(keys[i], &readBuffer, 0);
        ASSERT_TRUE(ret == 0);

        // EXPECT_TRUE(CheckData(hostSrcs[i], hostDests[i]));
    }

    for (uint32_t i = 0; i < keys_count; ++i) {
        ret = mmcc_remove(keys[i], 0);
        ASSERT_TRUE(ret == 0);
    }

    for (uint32_t i = 0; i < keys_count; ++i) {
        free(hostSrcs[i]);
        free(hostDests[i]);
    }
    sleep(3);
    free(hostSrc);
    free(hostDest);
    mmcs_local_service_stop(local_service);
    mmcc_uninit();
    mmcs_meta_service_stop(meta_service);
}

#define  MF_SIZE 1048576000

TEST_F(TestMmcServiceError, metaServiceRebuild)
{
    std::string metaUrl = "tcp://127.0.0.1:5868";
    std::string bmUrl = "tcp://127.0.0.1:5881";
    std::string hcomUrl = "tcp://127.0.0.1:5882";
    std::string localUrl = "";
    mmc_meta_service_config_t metaServiceConfig;
    metaServiceConfig.logLevel = 0;
    metaServiceConfig.logRotationFileSize = 2 * 1024 * 1024;
    metaServiceConfig.logRotationFileCount = 20;
    metaServiceConfig.evictThresholdHigh = 70;
    metaServiceConfig.evictThresholdLow = 60;
    metaServiceConfig.metaRebuildEnable = true;
    metaServiceConfig.tlsConfig.tlsEnable = false;
    UrlStringToChar(metaUrl, metaServiceConfig.discoveryURL);
    mmc_meta_service_t meta_service = mmcs_meta_service_start(&metaServiceConfig);
    ASSERT_TRUE(meta_service != nullptr);

    mmc_local_service_config_t localServiceConfig = {"", 0, 0, 1, bmUrl, hcomUrl, 0, "sdma", 0, MF_SIZE, 0};
    localServiceConfig.logLevel = 0;
    localServiceConfig.tlsConfig.tlsEnable = false;
    UrlStringToChar(metaUrl, localServiceConfig.discoveryURL);
    mmc_meta_service_t local_service = mmcs_local_service_start(&localServiceConfig);
    ASSERT_TRUE(local_service != nullptr);

    mmc_client_config_t clientConfig;
    clientConfig.logLevel = 0;
    clientConfig.tlsConfig.tlsEnable = false;
    clientConfig.rankId = 0;
    UrlStringToChar(metaUrl, clientConfig.discoveryURL);
    int32_t ret = mmcc_init(&clientConfig);
    ASSERT_TRUE(ret == 0);

    // 插入数据
    std::string test = "test";

    void *hostSrc = malloc(SIZE_32K);
    void *hostDest = malloc(SIZE_32K);

    GenerateData(hostSrc, 1);

    mmc_buffer buffer;
    buffer.addr = (uint64_t)hostSrc;
    buffer.type = 0;
    buffer.dimType = 0;
    buffer.oneDim.offset = 0;
    buffer.oneDim.len = SIZE_32K;

    mmc_put_options options{0, NATIVE_AFFINITY};
    ret = mmcc_put(test.c_str(), &buffer, options, 0);
    EXPECT_TRUE(ret == 0);

    mmc_buffer readBuffer;
    readBuffer.addr = (uint64_t)hostDest;
    readBuffer.type = 0;
    readBuffer.dimType = 0;
    readBuffer.oneDim.offset = 0;
    readBuffer.oneDim.len = SIZE_32K;

    ret = mmcc_get(test.c_str(), &readBuffer, 0);
    EXPECT_TRUE(ret == 0);

    // bool result = CheckData(hostSrc, hostDest);
    // EXPECT_TRUE(result);

    mmc_location_t location = mmcc_get_location(test.c_str(), 0);
    EXPECT_TRUE(location.xx == 0);

    uint32_t keys_count = MF_SIZE / SIZE_32K / 2;
    const char **keys = static_cast<const char**>(malloc(keys_count * sizeof(char*)));
    for (uint32_t i = 0; i < keys_count; i++) {
        keys[i] = static_cast<char*>(malloc(16));
        sprintf(const_cast<char *>(keys[i]), "test_%d", i);
    }
    void** hostSrcs = new void * [keys_count];
    void** hostDests = new void * [keys_count];
    auto *bufs = new mmc_buffer [keys_count];

    for (uint32_t i = 0; i < keys_count; ++i) {
        hostSrcs[i] = malloc(SIZE_32K);
        hostDests[i] = malloc(SIZE_32K);
        GenerateData(hostSrcs[i], 1);

        bufs[i].addr = (uint64_t)hostSrcs[i];
        bufs[i].type = 0;
        bufs[i].dimType = 0;
        bufs[i].oneDim.offset = 0;
        bufs[i].oneDim.len = SIZE_32K;
    }
    std::vector<int> results(keys_count, -1);
    ret = mmcc_batch_put(keys, keys_count, bufs, options, 0, results.data());
    EXPECT_TRUE(ret == 0);

    for (uint32_t i = 0; i < keys_count; ++i) {
        mmc_buffer readBuffer;
        readBuffer.addr = (uint64_t)hostDests[i];
        readBuffer.type = 0;
        readBuffer.dimType = 0;
        readBuffer.oneDim.offset = 0;
        readBuffer.oneDim.len = SIZE_32K;

        ret = mmcc_get(keys[i], &readBuffer, 0);
        EXPECT_TRUE(ret == 0);

        // EXPECT_TRUE(CheckData(hostSrcs[i], hostDests[i]));
    }

    ret = mmcc_remove(keys[0], 0);
    EXPECT_TRUE(ret == 0);
    sleep(3);

    mmcs_meta_service_stop(meta_service);
    sleep(3);

    meta_service = mmcs_meta_service_start(&metaServiceConfig);
    ASSERT_TRUE(meta_service != nullptr);
    sleep(3);

    ret = mmcc_get(test.c_str(), &readBuffer, 0);
    EXPECT_TRUE(ret == 0);

    // bool result = CheckData(hostSrc, hostDest);
    // EXPECT_TRUE(result);

    location = mmcc_get_location(test.c_str(), 0);
    EXPECT_TRUE(location.xx == 0);

    ret = mmcc_remove(test.c_str(), 0);
    EXPECT_TRUE(ret == 0);

    for (uint32_t i = 0; i < keys_count; ++i) {
        mmc_buffer readBuffer;
        readBuffer.addr = (uint64_t)hostDests[i];
        readBuffer.type = 0;
        readBuffer.dimType = 0;
        readBuffer.oneDim.offset = 0;
        readBuffer.oneDim.len = SIZE_32K;

        ret = mmcc_get(keys[i], &readBuffer, 0);
        if (i == 0) {
            EXPECT_TRUE(ret == -1);
        } else {
            EXPECT_TRUE(ret == 0);
        }

        // EXPECT_TRUE(CheckData(hostSrcs[i], hostDests[i]));
    }

    ret = mmcc_remove(keys[1], 0);
    EXPECT_TRUE(ret == 0);

    for (uint32_t i = 0; i < keys_count; ++i) {
        free(hostSrcs[i]);
        free(hostDests[i]);
    }
    sleep(3);
    free(hostSrc);
    free(hostDest);
    mmcs_local_service_stop(local_service);
    mmcc_uninit();
    mmcs_meta_service_stop(meta_service);
}
