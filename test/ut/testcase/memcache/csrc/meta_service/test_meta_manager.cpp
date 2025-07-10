/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "mmc_meta_manager.h"
#include "mmc_ref.h"
#include "gtest/gtest.h"
#include <iostream>
#include <chrono>
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

void TestMmcMetaManager::SetUp()
{
    cout << "this is Meta Manger TEST_F setup:" << std::endl;
}

void TestMmcMetaManager::TearDown()
{
    cout << "this is Meta Manager TEST_F teardown" << std::endl;
}

TEST_F(TestMmcMetaManager, Init)
{
    // MmcMemPoolInitInfo poolInitInfo;
    MmcLocation loc{0, 0};
    MmcLocalMemlInitInfo locInfo{100, 1000};
    // poolInitInfo[loc] = locInfo;

    uint64_t defaultTtl = 2000;

    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl);
    metaMng->Mount(loc, locInfo);
    ASSERT_TRUE(metaMng != nullptr);
}

TEST_F(TestMmcMetaManager, AllocAndFreeFail)
{
    // MmcMemPoolInitInfo poolInitInfo;
    MmcLocation loc{0, 0};
    MmcLocalMemlInitInfo locInfo{0, 1000000};
    // poolInitInfo[loc] = locInfo;
    uint64_t defaultTtl = 2000;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl);
    metaMng->Mount(loc, locInfo);

    AllocOptions allocReq{SIZE_32K, 1, 0, 0, 0};
    MmcMemObjMetaPtr objMeta;
    Result ret = metaMng->Alloc("test_string", allocReq, objMeta);
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(objMeta->NumBlobs() == 1);
    ASSERT_TRUE(objMeta->Size() == SIZE_32K);

    ret = metaMng->Remove("test_string");
    ASSERT_TRUE(ret == MMC_LEASE_NOT_EXPIRED);
}

TEST_F(TestMmcMetaManager, AllocAndFreeOK)
{
    // MmcMemPoolInitInfo poolInitInfo;
    MmcLocation loc{0, 0};
    MmcLocalMemlInitInfo locInfo{0, 1000000};
    // poolInitInfo[loc] = locInfo;
    uint64_t defaultTtl = 2000;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl);
    metaMng->Mount(loc, locInfo);

    uint16_t numKeys = 1U;
    std::vector<std::string> keys;
    std::vector<MmcMemObjMetaPtr> memMetaObjs;
    AllocOptions allocReq{SIZE_32K, 1, 0, 0, 0};  // blobSize, numBlobs, mediaType, preferredRank, flags
    Result ret;
    for (int i = 0; i < numKeys; ++i) {
        MmcMemObjMetaPtr objMeta;
        string key = "testKey" + std::to_string(i);
        ret = metaMng->Alloc(key, allocReq, objMeta);
        memMetaObjs.push_back(objMeta);
        keys.push_back(key);
    }
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(memMetaObjs[0]->NumBlobs() == 1);
    ASSERT_TRUE(memMetaObjs[0]->Size() == SIZE_32K);

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    for (int i = 0; i < numKeys; ++i) {
        ret = metaMng->Remove(keys[i]);
    }
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(memMetaObjs[0]->NumBlobs() == 0);
}

TEST_F(TestMmcMetaManager, GetAndUpdate)
{
    // MmcMemPoolInitInfo poolInitInfo;
    MmcLocation loc{0, 0};
    MmcLocalMemlInitInfo locInfo{0, 1000000};
    // poolInitInfo[loc] = locInfo;
    uint64_t defaultTtl = 2000;
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
        ret = metaMng->Alloc(key, allocReq, objMeta);
        memMetaObjs.push_back(objMeta);
        keys.push_back(key);
    }
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(memMetaObjs[0]->NumBlobs() == 1);
    ASSERT_TRUE(memMetaObjs[0]->Size() == SIZE_32K);

    ret = metaMng->UpdateState(keys[2], loc, MMC_WRITE_FAIL);
    ASSERT_TRUE(ret != MMC_OK);
    ret = metaMng->UpdateState(keys[2], loc, MMC_WRITE_OK);
    ASSERT_TRUE(ret == MMC_OK);

    MmcMemObjMetaPtr objMeta2;
    ret = metaMng->Get(keys[2], objMeta2);
    ASSERT_TRUE(ret == MMC_OK);
    std::vector<MmcMemBlobPtr> blobs;
    ret = metaMng->GetBlobs(keys[2], nullptr, blobs);
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(blobs.size() == 1);
    ASSERT_TRUE(blobs[0]->State() == DATA_READY);
}

TEST_F(TestMmcMetaManager, AllocAndExistKey)
{
    MmcLocation loc{0, 0};
    MmcLocalMemlInitInfo locInfo{0, 1000000};
    uint64_t defaultTtl = 2000;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl);
    metaMng->Mount(loc, locInfo);

    AllocOptions allocReq{SIZE_32K, 1, 0, 0, 0};
    MmcMemObjMetaPtr objMeta;
    Result ret = metaMng->Alloc("test_string", allocReq, objMeta);
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(objMeta->NumBlobs() == 1);
    ASSERT_TRUE(objMeta->Size() == SIZE_32K);

    ret = metaMng->ExistKey("test_string");
    ASSERT_TRUE(ret == MMC_OK);
    ret = metaMng->ExistKey("another_test_string");
    ASSERT_TRUE(ret == MMC_UNMATCHED_KEY);
}

TEST_F(TestMmcMetaManager, AllocAndBatchExistKey)
{
    MmcLocation loc{0, 0};
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
        ret = metaMng->Alloc(key, allocReq, objMeta);
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

    auto CheckReturn = [&metaMng](
        const std::vector<std::string> &keys, const Result targetResult, const uint16_t mmcOkCnt) {
        uint16_t mmcResultOkCnt = 0;
        std::vector<Result> results;
        Result ret = metaMng->BatchExistKey(keys, results);
        ASSERT_TRUE(ret == targetResult);
        for (const Result& result : results) {
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