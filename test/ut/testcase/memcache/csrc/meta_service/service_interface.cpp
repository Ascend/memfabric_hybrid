/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include <iostream>
#include "gtest/gtest.h"
#include "mmc_def.h"
#include "mmc_service.h"
#include "mmc_client.h"
#include "mmc_mem_blob.h"
#include "mmc_blob_allocator.h"

using namespace testing;
using namespace std;

class TestMmcServiceInterface : public testing::Test {
public:
    TestMmcServiceInterface();

    void SetUp() override;

    void TearDown() override;

protected:
    std::shared_ptr<ock::mmc::MmcBlobAllocator> allocator;
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
    std::string hcomUrl = "tcp://127.0.0.1:5882";
    std::string localUrl = "";
    mmc_meta_service_config_t metaServiceConfig;
    metaServiceConfig.logLevel = 0;
    metaServiceConfig.logRotationFileSize = 2 * 1024 * 1024;
    metaServiceConfig.logRotationFileCount = 20;
    metaServiceConfig.tlsConfig.tlsEnable = false;
    metaServiceConfig.evictThresholdHigh = 70;
    metaServiceConfig.evictThresholdLow = 60;
    metaServiceConfig.metaRebuildEnable = true;
    UrlStringToChar(metaUrl, metaServiceConfig.discoveryURL);
    mmc_meta_service_t meta_service = mmcs_meta_service_start(&metaServiceConfig);
    ASSERT_TRUE(meta_service != nullptr);

    mmc_local_service_config_t localServiceConfig = {"", 0, 0, 1, bmUrl, hcomUrl, 0, 0, "sdma", 0, 104857600, 0};
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

        // bool result = CheckData(hostSrcs[i], hostDests[i]);
        // EXPECT_TRUE(result);
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

TEST_F(TestMmcServiceInterface, testClientInitUninit)
{
    mmc_client_config_t clientConfig{};
    int32_t ret = mmcc_init(&clientConfig);
    ASSERT_TRUE(ret == ock::mmc::MMC_INVALID_PARAM);

    ret = mmcc_init(nullptr);
    ASSERT_TRUE(ret == ock::mmc::MMC_INVALID_PARAM);
}

TEST_F(TestMmcServiceInterface, testPutInvalidParam)
{
    mmc_client_config_t clientConfig{};
    int32_t ret = mmcc_init(&clientConfig);
    ASSERT_TRUE(ret == ock::mmc::MMC_OK);

    mmc_buffer validBuf{ (uint64_t)malloc(1024), 0, 0, {0, 1024} };
    const char* emptyKey = "";

    ret = mmcc_put(nullptr, &validBuf, mmc_put_options{}, 0);
    ASSERT_EQ(ret, ock::mmc::MMC_INVALID_PARAM);

    ret = mmcc_put(emptyKey, &validBuf, mmc_put_options{}, 0);
    ASSERT_EQ(ret, ock::mmc::MMC_INVALID_PARAM);

    ret = mmcc_put("validKey", nullptr, mmc_put_options{}, 0);
    ASSERT_EQ(ret, ock::mmc::MMC_INVALID_PARAM);

    mmc_buffer invalidBuf{ 0, 0, 0, {0, 1024} };
    ret = mmcc_put("validKey", &invalidBuf, mmc_put_options{}, 0);
    ASSERT_EQ(ret, ock::mmc::MMC_INVALID_PARAM);
    
    free((void*)validBuf.addr);
}

TEST_F(TestMmcServiceInterface, testBatchOperationsEdgeCases)
{

    int32_t results[2];
    mmc_data_info info[2];

    int32_t ret = mmcc_batch_remove(nullptr, 0, results, 0);
    ASSERT_EQ(ret, ock::mmc::MMC_INVALID_PARAM);
    
    ret = mmcc_batch_exist(nullptr, 0, results, 0);
    ASSERT_EQ(ret, ock::mmc::MMC_INVALID_PARAM);

    const uint32_t oversize = MAX_BATCH_OP_COUNT + 1;
    ret = mmcc_batch_remove(nullptr, oversize, results, 0);
    ASSERT_EQ(ret, ock::mmc::MMC_INVALID_PARAM);
}

TEST_F(TestMmcServiceInterface, testBatchQueryInvalidKeys)
{
    const char* keys[] = {
        "validKey1",
        "",
        nullptr,
        "invalid*&^%key",
        "validKey2"
    };
    const uint32_t keyCount = sizeof(keys) / sizeof(keys[0]);
    mmc_data_info info[keyCount];

    bool exists1 = mmcc_exist(keys[1], 0) == ock::mmc::MMC_OK;
    ASSERT_FALSE(exists1);
    
    bool exists2 = mmcc_exist(keys[2], 0) == ock::mmc::MMC_OK;
    ASSERT_FALSE(exists2);
    
    bool exists3 = mmcc_exist(keys[3], 0) == ock::mmc::MMC_OK;
    ASSERT_FALSE(exists3);
}

TEST_F(TestMmcServiceInterface, testExistOperations)
{
    mmc_client_config_t clientConfig{};
    mmcc_init(&clientConfig);

    int32_t ret = mmcc_exist("non_existent_key", 0);
    ASSERT_EQ(ret, ock::mmc::MMC_LINK_NOT_FOUND);

    const char* keys[] = {"key1", "key2", "non_existent"};
    int32_t exists[3];
    ret = mmcc_batch_exist(keys, 3, exists, 0);
    ASSERT_EQ(ret, ock::mmc::MMC_LINK_NOT_FOUND);
    ASSERT_EQ(exists[2], ock::mmc::MMC_OK);
}

TEST_F(TestMmcServiceInterface, testBatchGetErrorHandling)
{
    mmc_client_config_t clientConfig{};
    int32_t ret = mmcc_init(&clientConfig);
    ASSERT_EQ(ret, ock::mmc::MMC_OK);
    std::vector<int> results1(2, -1);
    ret = mmcc_batch_get(nullptr, 2, nullptr, 0, results1.data());
    ASSERT_EQ(ret, ock::mmc::MMC_INVALID_PARAM);

    const char* keys[] = {"test_key"};
    mmc_buffer bufs[1];
    std::vector<int> results2(1, -1);
    ret = mmcc_batch_get(keys, 0, bufs, 0, results2.data());
    ASSERT_EQ(ret, ock::mmc::MMC_INVALID_PARAM);

    const uint32_t oversize = MAX_BATCH_OP_COUNT + 1;
    ret = mmcc_batch_get(keys, oversize, bufs, 0, results2.data());
    ASSERT_EQ(ret, ock::mmc::MMC_INVALID_PARAM);

    const char* validKeys[] = {"key1", "key2"};
    mmc_buffer invalidBufs[2] = {
        {.addr = (uint64_t)malloc(SIZE_32K), .type = 0, .dimType = 0, .oneDim = {0, SIZE_32K}},
        {.addr = 0, .type = 0, .dimType = 0, .oneDim = {0, SIZE_32K}}
    };

    void* data = malloc(SIZE_32K);
    GenerateData(data, 1);
    mmc_buffer writeBuf = {.addr = (uint64_t)data, .type = 0, .dimType = 0, .oneDim = {0, SIZE_32K}};
    mmc_put_options putOpts{0, NATIVE_AFFINITY};
    
    mmcc_put("key1", &writeBuf, putOpts, 0);
    mmcc_put("key2", &writeBuf, putOpts, 0);
    std::vector<int> results3(2, -1);
    ret = mmcc_batch_get(validKeys, 2, invalidBufs, 0, results3.data());
    ASSERT_EQ(ret, ock::mmc::MMC_LINK_NOT_FOUND);

    EXPECT_NE(invalidBufs[0].addr, 0ULL);
    EXPECT_EQ(invalidBufs[1].addr, 0ULL);

    free(data);
    free((void*)invalidBufs[0].addr);
    mmcc_remove("key1", 0);
    mmcc_remove("key2", 0);
}

TEST_F(TestMmcServiceInterface, testBatchGetWithPartialData)
{
    mmc_client_config_t clientConfig{};
    int32_t ret = mmcc_init(&clientConfig);
    ASSERT_EQ(ret, ock::mmc::MMC_OK);

    const char* keys[] = {"partial_key1", "partial_key2", "partial_key3"};
    const uint32_t keyCount = sizeof(keys) / sizeof(keys[0]);

    void* data1 = malloc(SIZE_32K);
    void* data3 = malloc(SIZE_32K);
    GenerateData(data1, 1);
    GenerateData(data3, 3);
    
    mmc_buffer writeBuf1 = {.addr = (uint64_t)data1, .type = 0, .dimType = 0, .oneDim = {0, SIZE_32K}};
    mmc_buffer writeBuf3 = {.addr = (uint64_t)data3, .type = 0, .dimType = 0, .oneDim = {0, SIZE_32K}};
    
    mmc_put_options putOpts{0, NATIVE_AFFINITY};
    mmcc_put(keys[0], &writeBuf1, putOpts, 0);
    mmcc_put(keys[2], &writeBuf3, putOpts, 0);

    mmc_buffer readBufs[keyCount];
    void* destData[keyCount];
    
    for (uint32_t i = 0; i < keyCount; i++) {
        destData[i] = malloc(SIZE_32K);
        memset(destData[i], 0, SIZE_32K);
        readBufs[i] = mmc_buffer{
            .addr = (uint64_t)destData[i],
            .type = 0,
            .dimType = 0,
            .oneDim = {0, SIZE_32K}
        };
    }
    std::vector<int> results3(keyCount, -1);
    ret = mmcc_batch_get(keys, keyCount, readBufs, 0, results3.data());
    ASSERT_EQ(ret, ock::mmc::MMC_LINK_NOT_FOUND);

    EXPECT_FALSE(CheckData(data1, destData[0]));
    EXPECT_FALSE(CheckData(data1, destData[1]));
    EXPECT_FALSE(CheckData(data3, destData[2]));

    EXPECT_NE(readBufs[0].addr, 0ULL);
    EXPECT_NE(readBufs[1].addr, 0ULL);
    EXPECT_NE(readBufs[2].addr, 0ULL);

    free(data1);
    free(data3);
    for (uint32_t i = 0; i < keyCount; i++) {
        free(destData[i]);
        mmcc_remove(keys[i], 0);
    }
}

class TestMmcBlobAllocator : public testing::Test {
protected:
    void SetUp() override
    {
        baseMem = new char[CAPACITY];
        allocator = new ock::mmc::MmcBlobAllocator(
            0,
            static_cast<ock::mmc::MediaType>(0),
            reinterpret_cast<uint64_t>(baseMem),
            CAPACITY
        );
        std::map<std::string, ock::mmc::MmcMemBlobDesc> blobMap;
        allocator->BuildFromBlobs(blobMap);
    }

    void TearDown() override
    {
        delete allocator;
        delete[] baseMem;
    }

    void CheckAlignment(const ock::mmc::MmcMemBlobPtr& blob)
    {
        ASSERT_NE(blob.Get(), nullptr);
        EXPECT_EQ(blob->Gva() % 4096, 0UL);
    }

    ock::mmc::MmcBlobAllocator* allocator;
    char* baseMem;
    static constexpr uint64_t CAPACITY = 4096 * 16;
};

constexpr uint64_t TestMmcBlobAllocator::CAPACITY;

TEST_F(TestMmcBlobAllocator, testBlobAllocatorEdgeCases)
{
    auto blob = allocator->Alloc(CAPACITY * 2);
    ASSERT_EQ(blob.Get(), nullptr);

    auto fullBlob = allocator->Alloc(CAPACITY);
    ASSERT_NE(fullBlob.Get(), nullptr);

    ASSERT_EQ(allocator->Release(fullBlob), ock::mmc::MMC_OK);
    ASSERT_EQ(allocator->Release(fullBlob), ock::mmc::MMC_ERROR);

    auto invalidBlob = ock::mmc::MmcMakeRef<ock::mmc::MmcMemBlob>(
        0, 0xDEADBEEF, 1024, ock::mmc::MediaType(0), ock::mmc::NONE);
    ASSERT_EQ(allocator->Release(invalidBlob), ock::mmc::MMC_ERROR);
}

TEST_F(TestMmcBlobAllocator, StopBehavior)
{
    allocator->Stop();

    EXPECT_FALSE(allocator->CanAlloc(1024));
    auto blob = allocator->Alloc(1024);
    EXPECT_EQ(blob.Get(), nullptr);
}

TEST_F(TestMmcBlobAllocator, CanAllocCoverage)
{
    EXPECT_TRUE(allocator->CanAlloc(1024));
    EXPECT_FALSE(allocator->CanAlloc(CAPACITY * 2));
}

TEST_F(TestMmcBlobAllocator, AllocCoverage)
{
    auto largeBlob = allocator->Alloc(CAPACITY * 2);
    EXPECT_EQ(largeBlob.Get(), nullptr);

    auto exactBlob = allocator->Alloc(4096);

    auto splitBlob = allocator->Alloc(2048);

    uint64_t blobAddr = splitBlob->Gva();
    uint64_t baseAddr = reinterpret_cast<uint64_t>(baseMem);
    EXPECT_GE(blobAddr, baseAddr);
    EXPECT_LE(blobAddr - baseAddr, CAPACITY);

    auto largeBlob2 = allocator->Alloc(CAPACITY - 1);
    EXPECT_EQ(largeBlob2.Get(), nullptr);
}

TEST_F(TestMmcBlobAllocator, ReleaseCoverage)
{
    EXPECT_EQ(allocator->Release(nullptr), ock::mmc::MMC_ERROR);

    auto invalidAddrBlob = ock::mmc::MmcMakeRef<ock::mmc::MmcMemBlob>(
        0,
        static_cast<uint64_t>(0xDEADBEEF),
        1024,
        static_cast<ock::mmc::MediaType>(0),
        ock::mmc::NONE
    );
    EXPECT_EQ(allocator->Release(invalidAddrBlob), ock::mmc::MMC_ERROR);

    auto validBlob = allocator->Alloc(1024);
    EXPECT_EQ(allocator->Release(validBlob), ock::mmc::MMC_OK);
    EXPECT_EQ(allocator->Release(validBlob), ock::mmc::MMC_ERROR);

    auto blob1 = allocator->Alloc(2048);
    auto blob2 = allocator->Alloc(2048);
    EXPECT_EQ(allocator->Release(blob1), ock::mmc::MMC_OK);
    EXPECT_EQ(allocator->Release(blob2), ock::mmc::MMC_OK);
    EXPECT_TRUE(allocator->CanAlloc(4096));

    auto blob3 = allocator->Alloc(4096);
    auto blob4 = allocator->Alloc(2048);
    EXPECT_EQ(allocator->Release(blob4), ock::mmc::MMC_OK);
    EXPECT_EQ(allocator->Release(blob3), ock::mmc::MMC_OK);

    auto blobA = allocator->Alloc(2048);
    auto blobB = allocator->Alloc(2048);
    auto blobC = allocator->Alloc(2048);
    EXPECT_EQ(allocator->Release(blobB), ock::mmc::MMC_OK);
    EXPECT_EQ(allocator->Release(blobA), ock::mmc::MMC_OK);
    EXPECT_EQ(allocator->Release(blobC), ock::mmc::MMC_OK);
    EXPECT_TRUE(allocator->CanAlloc(6144));
}

TEST_F(TestMmcBlobAllocator, AllocationAfterRelease)
{
    auto blob1 = allocator->Alloc(8192);

    auto blob2 = allocator->Alloc(4096);

    EXPECT_EQ(allocator->Release(blob1), ock::mmc::MMC_OK);
    EXPECT_EQ(allocator->Release(blob2), ock::mmc::MMC_OK);

    auto fullBlob = allocator->Alloc(CAPACITY - 1);
}
