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
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl);
    metaMng->Mount(loc, locInfo);
    ASSERT_TRUE(metaMng != nullptr);
}

TEST_F(TestMmcMetaManager, AllocAndFree)
{

    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 1000000};

    uint64_t defaultTtl = 2000;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl);
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
}

TEST_F(TestMmcMetaManager, AllocAndFreeMulti)
{

    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 1000000};

    uint64_t defaultTtl = 200;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl);
    metaMng->Mount(loc, locInfo);

    uint16_t numKeys = 10U;
    std::vector<std::string> keys;
    std::vector<MmcMemObjMetaPtr> memMetaObjs;
    AllocOptions allocReq{SIZE_32K, 1, 0, 0, 0};
    Result ret;
    for (int i = 0; i < numKeys; ++i) {
        MmcMemObjMetaPtr objMeta;
        string key = "testKey" + std::to_string(i);
        ret = metaMng->Alloc(key, allocReq, 1, objMeta);
        memMetaObjs.push_back(objMeta);
        keys.push_back(key);
    }
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(memMetaObjs[0]->NumBlobs() == 1);
    ASSERT_TRUE(memMetaObjs[0]->Size() == SIZE_32K);

    for (int i = 0; i < numKeys; ++i) {
        ret = metaMng->Remove(keys[i]);
    }
    ASSERT_TRUE(memMetaObjs[0]->NumBlobs() == 0);
}

TEST_F(TestMmcMetaManager, GetAndUpdate)
{
    // MmcMemPoolInitInfo poolInitInfo;
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 1000000};
    // poolInitInfo[loc] = locInfo;
    uint64_t defaultTtl = 200;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl);
    metaMng->Mount(loc, locInfo);

    uint16_t numKeys = 20U;
    std::vector<std::string> keys;
    std::vector<MmcMemObjMetaPtr> memMetaObjs;
    AllocOptions allocReq{SIZE_32K, 1, 0, 0, 0};  // blobSize, numBlobs, mediaType, preferredRank, flags
    Result ret;
    for (int i = 0; i < numKeys; ++i) {
        MmcMemObjMetaPtr objMeta;
        string key = "testKey" + std::to_string(i);
        ret = metaMng->Alloc(key, allocReq, 1, objMeta);
        memMetaObjs.push_back(objMeta);
        keys.push_back(key);
    }
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(memMetaObjs[0]->NumBlobs() == 1);
    ASSERT_TRUE(memMetaObjs[0]->Size() == SIZE_32K);

    ret = metaMng->UpdateState(keys[2], loc, 0, 1, MMC_WRITE_FAIL);
    ASSERT_TRUE(ret != MMC_OK);
    ret = metaMng->UpdateState(keys[2], loc, 0, 1, MMC_WRITE_OK);
    ASSERT_TRUE(ret == MMC_OK);

    MmcMemObjMetaPtr objMeta2;
    ret = metaMng->Get(keys[2], objMeta2);
    ASSERT_TRUE(ret == MMC_OK);
    std::vector<MmcMemBlobPtr> blobs = objMeta2->GetBlobs();
    ASSERT_TRUE(blobs.size() == 1);
    ASSERT_TRUE(blobs[0]->State() == DATA_READY);
}

TEST_F(TestMmcMetaManager, LRU)
{
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 163840};
    uint64_t defaultTtl = 100;

    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl);
    metaMng->Mount(loc, locInfo);

    uint16_t numKeys = 8U;
    std::vector<std::string> keys;
    std::vector<MmcMemObjMetaPtr> memMetaObjs;
    AllocOptions allocReq{SIZE_32K, 1, 0, 0, 0};  // blobSize, numBlobs, mediaType, preferredRank, flags
    Result ret;

    for (int i = 0; i < numKeys; ++i) {
        MmcMemObjMetaPtr objMeta;
        string key = "testKey" + std::to_string(i);
        ret = metaMng->Alloc(key, allocReq, 1, objMeta);
        std::cout << "Alloc Result:" << ret << std::endl;
        memMetaObjs.push_back(objMeta);
        keys.push_back(key);
    }
    for (int i = 0; i < numKeys; ++i) {
        std::cout << "ExistKey:" << metaMng->ExistKey(keys[i]) << std::endl;
    }
    ASSERT_TRUE(metaMng->ExistKey(keys[0]) == MMC_UNMATCHED_KEY);
    ASSERT_TRUE(metaMng->ExistKey(keys[numKeys - 2]) == MMC_OK);
    ASSERT_TRUE(memMetaObjs[numKeys - 2]->Size() == SIZE_32K);

    for (int i = 0; i < numKeys; ++i) {
        ret = metaMng->Remove(keys[i]);
    }
    ASSERT_TRUE(metaMng->ExistKey(keys[0]) == MMC_UNMATCHED_KEY);
    ASSERT_TRUE(metaMng->ExistKey(keys[numKeys - 2]) == MMC_UNMATCHED_KEY);
}

TEST_F(TestMmcMetaManager, AllocAndExistKey)
{

    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 1000000};
    uint64_t defaultTtl = 2000;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl);
    metaMng->Mount(loc, locInfo);

    AllocOptions allocReq{SIZE_32K, 1, 0, 0, 0};
    MmcMemObjMetaPtr objMeta;
    Result ret = metaMng->Alloc("test_string", allocReq, 1, objMeta);
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(objMeta->NumBlobs() == 1);
    ASSERT_TRUE(objMeta->Size() == SIZE_32K);

    ASSERT_TRUE(metaMng->ExistKey("test_string") == MMC_OK);
    ASSERT_TRUE(metaMng->ExistKey("another_test_string") == MMC_UNMATCHED_KEY);
}

TEST_F(TestMmcMetaManager, AllocAndBatchExistKey)
{
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 1000000};
    uint64_t defaultTtl = 2000;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl);
    metaMng->Mount(loc, locInfo);

    uint16_t numKeys = 20U;
    std::vector<std::string> keys;
    std::vector<MmcMemObjMetaPtr> memMetaObjs;
    AllocOptions allocReq{SIZE_32K, 1, 0, 0, 0};  // blobSize, numBlobs, mediaType, preferredRank, flags
    Result ret = MMC_ERROR;
    for (uint16_t i = 0U; i < numKeys; ++i) {
        MmcMemObjMetaPtr objMeta;
        string key = "testKey_" + std::to_string(i);
        ret = metaMng->Alloc(key, allocReq, 1, objMeta);
        memMetaObjs.push_back(objMeta);
        keys.push_back(key);
    }
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(memMetaObjs[0]->NumBlobs() == 1);
    ASSERT_TRUE(memMetaObjs[0]->Size() == SIZE_32K);

    std::vector<std::string> allExistKeys, partExistKeys, allNotExistKeys;
    auto GetKeys = [](uint16_t start, uint16_t end, std::vector<std::string> &keys) {
        for (uint16_t i = start; i < end; ++i) {
            string key = "testKey_" + std::to_string(i);
            keys.push_back(key);
        }
    };
    GetKeys(0U, 20U, allExistKeys);
    GetKeys(10U, 30U, partExistKeys);
    GetKeys(20U, 40U, allNotExistKeys);

    auto CheckReturn = [&metaMng](const std::vector<std::string> &keys, const Result targetResult,
                                  const uint16_t mmcOkCnt) {
        uint16_t mmcResultOkCnt = 0;
        std::vector<Result> results;
        ASSERT_TRUE(metaMng->BatchExistKey(keys, results) == targetResult);
        for (const Result &result : results) {
            if (result == MMC_OK) {
                ++mmcResultOkCnt;
            }
        }
        ASSERT_TRUE(mmcResultOkCnt == mmcOkCnt);
    };
    CheckReturn(allExistKeys, MMC_OK, 20U);
    CheckReturn(partExistKeys, MMC_OK, 10U);
    CheckReturn(allNotExistKeys, MMC_UNMATCHED_KEY, 0U);
}

TEST_F(TestMmcMetaManager, Remove)
{
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 1000000};
    uint64_t defaultTtl = 2000;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl);
    metaMng->Mount(loc, locInfo);

    AllocOptions allocReq{SIZE_32K, 1, 0, 0, 0};
    MmcMemObjMetaPtr objMeta;
    Result ret = metaMng->Alloc("testKey", allocReq, 1, objMeta);
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(objMeta != nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    ret = metaMng->Remove("testKey");
    ASSERT_TRUE(ret == MMC_OK);

    ret = metaMng->Remove("nonexistentKey");
    ASSERT_TRUE(ret == MMC_UNMATCHED_KEY);

    ret = metaMng->Alloc("testKey2", allocReq, 1, objMeta);
    ASSERT_TRUE(ret == MMC_OK);
    ret = metaMng->Remove("testKey2");
    ASSERT_TRUE(ret == MMC_OK);
}

TEST_F(TestMmcMetaManager, BatchRemove)
{
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 1000000};
    uint64_t defaultTtl = 2000;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl);
    metaMng->Mount(loc, locInfo);

    std::vector<std::string> keys = {"key1", "key2", "key3"};
    std::vector<Result> results;

    AllocOptions allocReq{SIZE_32K, 1, 0, 0, 0};
    MmcMemObjMetaPtr objMeta;
    for (const auto &key : keys) {
        Result ret = metaMng->Alloc(key, allocReq, 1, objMeta);
        ASSERT_TRUE(ret == MMC_OK);
        ASSERT_TRUE(objMeta != nullptr);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    Result ret = metaMng->BatchRemove(keys, results);
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_EQ(results.size(), keys.size());
    for (const auto &result : results) {
        ASSERT_TRUE(result == MMC_OK);
    }

    std::vector<std::string> nonExistentKeys = {"nonexistentKey1", "nonexistentKey2"};
    ret = metaMng->BatchRemove(nonExistentKeys, results);
    ASSERT_TRUE(ret == MMC_OK);
    for (const auto &result : results) {
        ASSERT_TRUE(result == MMC_UNMATCHED_KEY);
    }

    ret = metaMng->Alloc("leaseNotExpiredKey", allocReq, 1, objMeta);
    ASSERT_TRUE(ret == MMC_OK);
    std::vector<std::string> leaseNotExpiredKeys = {"leaseNotExpiredKey"};
    ret = metaMng->BatchRemove(leaseNotExpiredKeys, results);
    ASSERT_TRUE(ret == MMC_OK);
    for (const auto &result : results) {
        ASSERT_TRUE(result == MMC_OK);
    }
}

TEST_F(TestMmcMetaManager, BatchGet)
{
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 1000000};
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(2000);
    metaMng->Mount(loc, locInfo);

    const uint16_t numKeys = 20U;
    std::vector<std::string> keys;
    std::vector<MmcMemObjMetaPtr> memMetaObjs;
    AllocOptions allocReq{SIZE_32K, 1, 0, 0, 0};  // blobSize, numBlobs, mediaType, preferredRank, flags
    Result ret;

    for (int i = 0; i < numKeys; ++i) {
        MmcMemObjMetaPtr objMeta;
        std::string key = "testKey" + std::to_string(i);
        ret = metaMng->Alloc(key, allocReq, 1, objMeta);
        memMetaObjs.push_back(objMeta);
        keys.push_back(key);
    }
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(memMetaObjs[0]->NumBlobs() == 1);
    ASSERT_TRUE(memMetaObjs[0]->Size() == SIZE_32K);

    std::vector<MmcMemObjMetaPtr> objMetas;
    std::vector<Result> getResults;
    ret = metaMng->BatchGet(keys, objMetas, getResults);
    ASSERT_TRUE(ret == MMC_OK);

    for (size_t i = 0; i < keys.size(); ++i) {
        ASSERT_TRUE(getResults[i] == MMC_OK) << "Key " << keys[i] << " failed";
        ASSERT_TRUE(objMetas[i] != nullptr) << "Key " << keys[i] << " objMeta is null";
        ASSERT_TRUE(objMetas[i]->NumBlobs() == 1) << "Key " << keys[i] << " blob count mismatch";
        ASSERT_TRUE(objMetas[i]->Size() == SIZE_32K) << "Key " << keys[i] << " size mismatch";
    }

    std::vector<std::string> partialKeys = {keys[0], "nonexistentKey"};
    std::vector<MmcMemObjMetaPtr> partialObjMetas;
    std::vector<Result> partialGetResults;
    ret = metaMng->BatchGet(partialKeys, partialObjMetas, partialGetResults);

    ASSERT_TRUE(partialGetResults[0] == MMC_OK) << "Partial key " << partialKeys[0] << " failed";
    ASSERT_TRUE(partialGetResults[1] == MMC_ERROR) << "Partial key " << partialKeys[1] << " not unmatched";

    std::vector<std::string> nonExistentKeys = {"nonexistentKey1", "nonexistentKey2"};
    std::vector<MmcMemObjMetaPtr> nonExistentObjMetas;
    std::vector<Result> nonExistentGetResults;
    ret = metaMng->BatchGet(nonExistentKeys, nonExistentObjMetas, nonExistentGetResults);

    for (size_t i = 0; i < nonExistentKeys.size(); ++i) {
        ASSERT_TRUE(nonExistentGetResults[i] == MMC_ERROR)
            << "Nonexistent key " << nonExistentKeys[i] << " not unmatched ";
    }

    for (const auto &key : keys) {
        metaMng->Remove(key);
    }
}