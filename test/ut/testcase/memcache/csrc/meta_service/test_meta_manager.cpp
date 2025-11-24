/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "mmc_meta_manager.h"
#include "mmc_ref.h"
#include "gtest/gtest.h"
#include <chrono>
#include <iostream>
#include <thread>

using namespace testing;
using namespace std;
using namespace ock::mmc;

class TestMmcMetaManager : public testing::Test {
public:
    TestMmcMetaManager();

    void SetUp() override;

    void TearDown() override;

protected:
};
TestMmcMetaManager::TestMmcMetaManager() {}

void TestMmcMetaManager::SetUp() {}

void TestMmcMetaManager::TearDown() {}

TEST_F(TestMmcMetaManager, Init)
{
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{100, 1000};

    uint64_t defaultTtl = 2000;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl, 70, 60);
    metaMng->Start();
    std::map<std::string, MmcMemBlobDesc> blobMap;
    metaMng->Mount(loc, locInfo, blobMap);
    ASSERT_TRUE(metaMng != nullptr);
    metaMng->Stop();
}

TEST_F(TestMmcMetaManager, AllocAndFree)
{
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 1000000};

    uint64_t defaultTtl = 2000;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl, 70, 60);
    metaMng->Start();
    std::map<std::string, MmcMemBlobDesc> blobMap;
    metaMng->Mount(loc, locInfo, blobMap);

    AllocOptions allocReq{SIZE_32K, 1, MEDIA_DRAM, {0}, 0};  // blobSize, numBlobs, mediaType, preferredRank, flags
    MmcMemMetaDesc objMeta;
    Result ret = metaMng->Alloc("test_string", allocReq, 1, objMeta);
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(objMeta.NumBlobs() == 1);
    ASSERT_TRUE(objMeta.Size() == SIZE_32K);

    metaMng->UpdateState("test_string", loc, MMC_WRITE_OK, 1);

    ret = metaMng->Remove("test_string");
    ASSERT_TRUE(ret == MMC_OK);
    metaMng->Stop();
}

TEST_F(TestMmcMetaManager, AllocAndFreeMulti)
{
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 1000000};

    uint64_t defaultTtl = 200;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl, 70, 60);
    metaMng->Start();
    std::map<std::string, MmcMemBlobDesc> blobMap;
    metaMng->Mount(loc, locInfo, blobMap);

    uint16_t numKeys = 10U;
    std::vector<std::string> keys;
    std::vector<MmcMemMetaDesc> memMetaObjs;
    AllocOptions allocReq{SIZE_32K, 1, MEDIA_DRAM, {0}, 0};
    Result ret;
    for (int i = 0; i < numKeys; ++i) {
        MmcMemMetaDesc objMeta;
        string key = "testKey" + std::to_string(i);
        ret = metaMng->Alloc(key, allocReq, 1, objMeta);
        memMetaObjs.push_back(objMeta);
        keys.push_back(key);
    }
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(memMetaObjs[0].NumBlobs() == 1);
    ASSERT_TRUE(memMetaObjs[0].Size() == SIZE_32K);

    for (int i = 0; i < numKeys; ++i) {
        ret = metaMng->Remove(keys[i]);
    }
    metaMng->Stop();
}

TEST_F(TestMmcMetaManager, GetAndUpdate)
{
    // MmcMemPoolInitInfo poolInitInfo;
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 1000000};
    // poolInitInfo[loc] = locInfo;
    uint64_t defaultTtl = 200;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl, 70, 60);
    metaMng->Start();
    std::map<std::string, MmcMemBlobDesc> blobMap;
    metaMng->Mount(loc, locInfo, blobMap);

    uint16_t numKeys = 20U;
    std::vector<std::string> keys;
    std::vector<MmcMemMetaDesc> memMetaObjs;
    AllocOptions allocReq{SIZE_32K, 1, MEDIA_DRAM, {0}, 0};  // blobSize, numBlobs, mediaType, preferredRank, flags
    Result ret;
    for (int i = 0; i < numKeys; ++i) {
        MmcMemMetaDesc objMeta;
        string key = "testKey" + std::to_string(i);
        ret = metaMng->Alloc(key, allocReq, 1, objMeta);
        memMetaObjs.push_back(objMeta);
        keys.push_back(key);
    }
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(memMetaObjs[0].NumBlobs() == 1);
    ASSERT_TRUE(memMetaObjs[0].Size() == SIZE_32K);

    ret = metaMng->UpdateState(keys[2], loc, MMC_WRITE_OK, 1);
    ASSERT_TRUE(ret == MMC_OK);

    MmcMemMetaDesc objMeta2;
    ret = metaMng->Get(keys[2], 1, nullptr, objMeta2);
    ASSERT_TRUE(ret == MMC_OK);
    std::vector<MmcMemBlobDesc> blobs = objMeta2.blobs_;
    ASSERT_TRUE(blobs.size() == 1);
    metaMng->Stop();
}

TEST_F(TestMmcMetaManager, LRU)
{
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 163840};
    uint64_t defaultTtl = 100;

    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl, 70, 50);
    metaMng->Start();
    std::map<std::string, MmcMemBlobDesc> blobMap;
    metaMng->Mount(loc, locInfo, blobMap);

    uint16_t numKeys = 8U;
    std::vector<std::string> keys;
    std::vector<MmcMemMetaDesc> memMetaObjs;
    AllocOptions allocReq{SIZE_32K, 1, MEDIA_DRAM, {0}, 0};  // blobSize, numBlobs, mediaType, preferredRank, flags
    Result ret;
    Result writeRet;

    std::vector<MmcMemMetaDesc> objMetas;
    for (int i = 0; i < numKeys; ++i) {
        MmcMemMetaDesc objMeta;
        string key = "testKey" + std::to_string(i);
        MetaNetServerPtr server;
        metaMng->CheckAndEvict();
        usleep(1000 * 500);
        ret = metaMng->Alloc(key, allocReq, 1, objMeta);
        ASSERT_TRUE(ret == MMC_OK);
        memMetaObjs.push_back(objMeta);
        keys.push_back(key);
        writeRet = metaMng->UpdateState(key, loc, MMC_WRITE_OK, 1);
        ASSERT_TRUE(writeRet == MMC_OK);
        objMetas.push_back(objMeta);
    }

    ASSERT_TRUE(metaMng->ExistKey(keys[0]) == MMC_UNMATCHED_KEY);
    ASSERT_TRUE(metaMng->ExistKey(keys[numKeys - 1]) == MMC_OK);
    ASSERT_TRUE(memMetaObjs[numKeys - 1].Size() == SIZE_32K);

    for (int i = 0; i < numKeys; ++i) {
        ret = metaMng->Remove(keys[i]);
    }
    ASSERT_TRUE(metaMng->ExistKey(keys[0]) == MMC_UNMATCHED_KEY);
    ASSERT_TRUE(metaMng->ExistKey(keys[numKeys - 1]) == MMC_UNMATCHED_KEY);
    metaMng->Stop();
}

TEST_F(TestMmcMetaManager, AllocAndExistKey)
{

    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 1000000};
    uint64_t defaultTtl = 2000;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl, 70, 60);
    metaMng->Start();
    std::map<std::string, MmcMemBlobDesc> blobMap;
    metaMng->Mount(loc, locInfo, blobMap);

    AllocOptions allocReq{SIZE_32K, 1, MEDIA_DRAM, {0}, 0};
    MmcMemMetaDesc objMeta;
    Result ret = metaMng->Alloc("test_string", allocReq, 1, objMeta);
    metaMng->UpdateState("test_string", loc, MMC_WRITE_OK, 1);
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(objMeta.NumBlobs() == 1);
    ASSERT_TRUE(objMeta.Size() == SIZE_32K);

    ASSERT_TRUE(metaMng->ExistKey("test_string") == MMC_OK);
    ASSERT_TRUE(metaMng->ExistKey("another_test_string") == MMC_UNMATCHED_KEY);
    metaMng->Stop();
}

TEST_F(TestMmcMetaManager, AllocAndBatchExistKey)
{
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 1000000};
    uint64_t defaultTtl = 2000;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl, 70, 60);
    metaMng->Start();
    std::map<std::string, MmcMemBlobDesc> blobMap;
    metaMng->Mount(loc, locInfo, blobMap);

    uint16_t numKeys = 5U;
    std::vector<std::string> keys;
    std::vector<MmcMemMetaDesc> memMetaObjs;
    AllocOptions allocReq{SIZE_32K, 1, MEDIA_DRAM, {0}, 0};  // blobSize, numBlobs, mediaType, preferredRank, flags
    Result ret = MMC_ERROR;
    for (uint16_t i = 0U; i < numKeys; ++i) {
        MmcMemMetaDesc objMeta;
        string key = "testKey_" + std::to_string(i);
        ret = metaMng->Alloc(key, allocReq, 1, objMeta);
        memMetaObjs.push_back(objMeta);
        keys.push_back(key);
        metaMng->UpdateState(key, loc, MMC_WRITE_OK, 1);
    }
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(memMetaObjs[0].NumBlobs() == 1);
    ASSERT_TRUE(memMetaObjs[0].Size() == SIZE_32K);

    std::vector<std::string> allExistKeys;
    std::vector<std::string> partExistKeys;
    std::vector<std::string> allNotExistKeys;

    auto GetKeys = [](uint16_t start, uint16_t end, std::vector<std::string> &keys) {
        for (uint16_t i = start; i < end; ++i) {
            string key = "testKey_" + std::to_string(i);
            keys.push_back(key);
        }
    };
    GetKeys(0U, 5U, allExistKeys);
    GetKeys(2U, 7U, partExistKeys);
    GetKeys(5U, 10U, allNotExistKeys);

    auto CheckReturn = [&metaMng](const std::vector<std::string> &keys, const std::vector<Result> &targetResults) {
        std::vector<Result> results;
        for (auto& key : keys) {
            results.push_back(metaMng->ExistKey(key));
        }
        ASSERT_TRUE(results.size() == targetResults.size());
        for (size_t i = 0; i < results.size(); ++i) {
            ASSERT_TRUE(targetResults[i] == results[i]);
        }
    };
    CheckReturn(allExistKeys, std::vector<Result>(5, MMC_OK));
    CheckReturn(partExistKeys, {MMC_OK, MMC_OK, MMC_OK, MMC_UNMATCHED_KEY, MMC_UNMATCHED_KEY});
    CheckReturn(allNotExistKeys, std::vector<Result>(5, MMC_UNMATCHED_KEY));
    metaMng->Stop();
}

TEST_F(TestMmcMetaManager, Remove)
{
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 1000000};
    uint64_t defaultTtl = 2000;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl, 70, 60);
    metaMng->Start();
    std::map<std::string, MmcMemBlobDesc> blobMap;
    metaMng->Mount(loc, locInfo, blobMap);

    AllocOptions allocReq{SIZE_32K, 1, MEDIA_DRAM, {0}, 0};
    MmcMemMetaDesc objMeta;
    Result ret = metaMng->Alloc("testKey", allocReq, 1, objMeta);
    ASSERT_TRUE(ret == MMC_OK);

    // std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    ret = metaMng->Remove("testKey");
    ASSERT_TRUE(ret == MMC_OK);

    ret = metaMng->Remove("nonexistentKey");
    ASSERT_TRUE(ret == MMC_UNMATCHED_KEY);

    ret = metaMng->Alloc("testKey2", allocReq, 1, objMeta);
    ASSERT_TRUE(ret == MMC_OK);
    ret = metaMng->Remove("testKey2");
    ASSERT_TRUE(ret == MMC_OK);
    metaMng->Stop();
}

TEST_F(TestMmcMetaManager, Get_NotAllBlobsReady)
{
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 1000000};
    uint64_t defaultTtl = 2000;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl, 70, 60);
    metaMng->Start();
    std::map<std::string, MmcMemBlobDesc> blobMap;
    metaMng->Mount(loc, locInfo, blobMap);

    AllocOptions allocReq{SIZE_32K, 1, MEDIA_DRAM, {0}, 0};
    MmcMemMetaDesc objMeta;
    Result ret = metaMng->Alloc("test_key", allocReq, 1, objMeta);
    ASSERT_EQ(ret, MMC_OK);

    MmcMemMetaDesc resultMeta;
    ret = metaMng->Get("test_key", 1, nullptr, resultMeta);
    ASSERT_EQ(ret, MMC_OK);
    ASSERT_EQ(resultMeta.NumBlobs(), 0);

    metaMng->Remove("test_key");
    metaMng->Stop();
}

TEST_F(TestMmcMetaManager, Alloc_ThresholdEviction)
{
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 96 * 1024};
    uint64_t defaultTtl = 2000;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl, 70, 60);
    metaMng->Start();
    std::map<std::string, MmcMemBlobDesc> blobMap;
    metaMng->Mount(loc, locInfo, blobMap);

    std::vector<std::string> keys = {"key1", "key2"};
    for (const auto& key : keys) {
        AllocOptions allocReq{SIZE_32K, 1, MEDIA_DRAM, {0}, 0};
        MmcMemMetaDesc objMeta;
        Result ret = metaMng->Alloc(key, allocReq, 1, objMeta);
        ASSERT_EQ(ret, MMC_OK);
        metaMng->UpdateState(key, loc, MMC_WRITE_OK, 1);
    }

    MmcMemMetaDesc temp;
    metaMng->Get("key2", 1, nullptr, temp);

    AllocOptions allocReq{SIZE_32K, 1, MEDIA_DRAM, {0}, 0};
    MmcMemMetaDesc newObjMeta;
    Result ret = metaMng->Alloc("key3", allocReq, 1, newObjMeta);
    ASSERT_EQ(ret, MMC_OK);

    ASSERT_EQ(metaMng->ExistKey("key1"), MMC_OK);
    ASSERT_EQ(metaMng->ExistKey("key2"), MMC_OK);

    metaMng->Remove("key2");
    metaMng->Remove("key3");
    metaMng->Stop();
}