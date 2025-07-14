/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include <iostream>
#include "gtest/gtest.h"
#include "mmc_ref.h"
#include "mmc_meta_service_default.h"
#include "mmc_local_service_default.h"
#include "mmc_msg_client_meta.h"
#include "mmc_locality_strategy.h"
#include "mmc_mem_obj_meta.h"

using namespace testing;
using namespace std;
using namespace ock::mmc;

class TestMmcMetaService : public testing::Test {
public:
    TestMmcMetaService();

    void SetUp() override;

    void TearDown() override;

protected:
};
TestMmcMetaService::TestMmcMetaService() {}

void TestMmcMetaService::SetUp()
{
    cout << "this is NetEngine TEST_F setup:";
}

void TestMmcMetaService::TearDown()
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

TEST_F(TestMmcMetaService, Init)
{
    std::string metaUrl = "tcp://127.0.0.1:5678";
    std::string bmUrl = "tcp://127.0.0.1:5681";
    mmc_meta_service_config_t metaServiceConfig;
    UrlStringToChar(metaUrl, metaServiceConfig.discoveryURL);
    metaServiceConfig.worldSize = 1;
    auto metaServiceDefault = MmcMakeRef<MmcMetaServiceDefault>("testMetaService");
    MmcMetaServicePtr metaServicePtr = Convert<MmcMetaServiceDefault, MmcMetaService>(metaServiceDefault);
    ASSERT_TRUE(metaServicePtr->Start(metaServiceConfig) == MMC_OK);

    mmc_local_service_config_t localServiceConfig = {"", 0, 0, 1, bmUrl, 0, 0, "sdma", 0, 104857600, 0};
    UrlStringToChar(metaUrl, localServiceConfig.discoveryURL);
    auto localServiceDefault = MmcMakeRef<MmcLocalServiceDefault>("testLocalService");
    MmcLocalServicePtr localServicePtr = Convert<MmcLocalServiceDefault, MmcLocalService>(localServiceDefault);
    ASSERT_TRUE(localServicePtr->Start(localServiceConfig) == MMC_OK);

    PingMsg req;
    req.msgId = ML_PING_REQ;
    req.num = 123;
    PingMsg resp;
    ASSERT_TRUE(localServiceDefault->SyncCallMeta(req, resp, 30) == MMC_OK);


    AllocRequest reqAlloc;
    reqAlloc.key_ = "test";
    reqAlloc.options_ = {SIZE_32K, 1, 0, 0, 0};
    AllocResponse respAlloc;
    ASSERT_TRUE(localServiceDefault->SyncCallMeta(reqAlloc, respAlloc, 30) == MMC_OK);
    ASSERT_TRUE(respAlloc.numBlobs_ == 1);
    ASSERT_TRUE(respAlloc.blobs_.size() == 1);
    ASSERT_TRUE(respAlloc.blobs_[0].size_ == SIZE_32K);
    localServicePtr->Stop();
    metaServicePtr->Stop();
}

TEST_F(TestMmcMetaService, ExistRequest)
{
    std::string metaUrl = "tcp://127.0.0.1:5679";
    std::string bmUrl = "tcp://127.0.0.1:5682";
    mmc_meta_service_config_t metaServiceConfig;
    UrlStringToChar(metaUrl, metaServiceConfig.discoveryURL);
    metaServiceConfig.worldSize = 1;
    auto metaServiceDefault = MmcMakeRef<MmcMetaServiceDefault>("testMetaService");
    MmcMetaServicePtr metaServicePtr = Convert<MmcMetaServiceDefault, MmcMetaService>(metaServiceDefault);
    ASSERT_TRUE(metaServicePtr->Start(metaServiceConfig) == MMC_OK);

    mmc_local_service_config_t localServiceConfig = {"", 0, 0, 1, bmUrl, 0, 0, "sdma", 0, 104857600, 0};
    UrlStringToChar(metaUrl, localServiceConfig.discoveryURL);
    auto localServiceDefault = MmcMakeRef<MmcLocalServiceDefault>("testLocalService");
    MmcLocalServicePtr localServicePtr = Convert<MmcLocalServiceDefault, MmcLocalService>(localServiceDefault);
    ASSERT_TRUE(localServicePtr->Start(localServiceConfig) == MMC_OK);

    AllocRequest reqAlloc;
    reqAlloc.key_ = "test";
    reqAlloc.options_ = {SIZE_32K, 1, 0, 0, 0};
    AllocResponse respAlloc;
    ASSERT_TRUE(localServiceDefault->SyncCallMeta(reqAlloc, respAlloc, 30) == MMC_OK);

    IsExistRequest reqExist;
    reqExist.key_ = "test";
    Response respExist;
    ASSERT_TRUE(localServiceDefault->SyncCallMeta(reqExist, respExist, 30) == MMC_OK);
    ASSERT_TRUE(respExist.ret_ == MMC_OK);

    IsExistRequest reqNotExist;
    reqNotExist.key_ = "another_test";
    Response respNotExist;
    ASSERT_TRUE(localServiceDefault->SyncCallMeta(reqNotExist, respNotExist, 30) == MMC_OK);
    ASSERT_TRUE(respNotExist.ret_ == MMC_UNMATCHED_KEY);

    localServicePtr->Stop();
    metaServicePtr->Stop();
}

TEST_F(TestMmcMetaService, BatchExistRequest)
{
    std::string metaUrl = "tcp://127.0.0.1:5680";
    std::string bmUrl = "tcp://127.0.0.1:5683";
    mmc_meta_service_config_t metaServiceConfig;
    UrlStringToChar(metaUrl, metaServiceConfig.discoveryURL);
    metaServiceConfig.worldSize = 1;
    auto metaServiceDefault = MmcMakeRef<MmcMetaServiceDefault>("testMetaService");
    MmcMetaServicePtr metaServicePtr = Convert<MmcMetaServiceDefault, MmcMetaService>(metaServiceDefault);
    ASSERT_TRUE(metaServicePtr->Start(metaServiceConfig) == MMC_OK);

    mmc_local_service_config_t localServiceConfig = {"", 0, 0, 1, bmUrl, 0, 0, "sdma", 0, 104857600, 0};
    UrlStringToChar(metaUrl, localServiceConfig.discoveryURL);
    auto localServiceDefault = MmcMakeRef<MmcLocalServiceDefault>("testLocalService");
    MmcLocalServicePtr localServicePtr = Convert<MmcLocalServiceDefault, MmcLocalService>(localServiceDefault);
    ASSERT_TRUE(localServicePtr->Start(localServiceConfig) == MMC_OK);

    AllocRequest reqAlloc;
    reqAlloc.options_ = {SIZE_32K, 1, 0, 0, 0};
    for (uint16_t i = 0U; i < 10U; ++i) {
        reqAlloc.key_ = "test_" + std::to_string(i);
        AllocResponse respAlloc;
        ASSERT_TRUE(localServiceDefault->SyncCallMeta(reqAlloc, respAlloc, 30) == MMC_OK);
    }

    std::vector<std::string> allExistKeys, partExistKeys, allNotExistKeys;
    std::vector<Result> results;
    auto GetKeys = [](uint16_t start, uint16_t end, std::vector<std::string> &keys) {
        for (uint16_t i = start; i < end; ++i) {
            string key = "test_" + std::to_string(i);
            keys.push_back(key);
        }
    };
    GetKeys(0U, 10U, allExistKeys);
    GetKeys(5U, 15U, partExistKeys);
    GetKeys(10U, 20U, allNotExistKeys);

    auto CheckReturn = [&localServiceDefault](const std::vector<std::string> &keys, const Result targetResult, const uint16_t mmcOkCnt) {
        BatchIsExistRequest reqExist;
        reqExist.keys_ = keys;
        BatchIsExistResponse respExist;
        ASSERT_TRUE(localServiceDefault->SyncCallMeta(reqExist, respExist, 30) == MMC_OK);
        ASSERT_TRUE(respExist.ret_ == targetResult);  
        
        uint16_t mmcResultOkCnt = 0;
        for (const Result& result : respExist.results_) {
            if (result == MMC_OK) {
                ++mmcResultOkCnt;
            }
        }
        ASSERT_TRUE(mmcResultOkCnt == mmcOkCnt);
    };
    CheckReturn(allExistKeys, MMC_OK, 10U);
    CheckReturn(partExistKeys, MMC_OK, 5U);
    CheckReturn(allNotExistKeys, MMC_UNMATCHED_KEY, 0U);
    
    localServicePtr->Stop();
    metaServicePtr->Stop();
}
