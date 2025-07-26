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
    std::string hcomUrl = "tcp://127.0.0.1:5682";
    mmc_meta_service_config_t metaServiceConfig;
    UrlStringToChar(metaUrl, metaServiceConfig.discoveryURL);
    auto metaServiceDefault = MmcMakeRef<MmcMetaServiceDefault>("testMetaService");
    MmcMetaServicePtr metaServicePtr = Convert<MmcMetaServiceDefault, MmcMetaService>(metaServiceDefault);
    ASSERT_TRUE(metaServicePtr->Start(metaServiceConfig) == MMC_OK);

    mmc_local_service_config_t localServiceConfig = {"", 0, 0, 1, bmUrl, hcomUrl, 0, 0, "sdma", 0, 104857600, 0};
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
    std::string hcomUrl = "tcp://127.0.0.1:5683";
    mmc_meta_service_config_t metaServiceConfig;
    UrlStringToChar(metaUrl, metaServiceConfig.discoveryURL);
    auto metaServiceDefault = MmcMakeRef<MmcMetaServiceDefault>("testMetaService");
    MmcMetaServicePtr metaServicePtr = Convert<MmcMetaServiceDefault, MmcMetaService>(metaServiceDefault);
    ASSERT_TRUE(metaServicePtr->Start(metaServiceConfig) == MMC_OK);

    mmc_local_service_config_t localServiceConfig = {"", 0, 0, 1, bmUrl, hcomUrl, 0, 0, "sdma", 0, 104857600, 0};
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
    ASSERT_TRUE(respNotExist.ret_ != MMC_OK);

    localServicePtr->Stop();
    metaServicePtr->Stop();
}

TEST_F(TestMmcMetaService, BatchExistRequest)
{
    std::string metaUrl = "tcp://127.0.0.1:5680";
    std::string bmUrl = "tcp://127.0.0.1:5683";
    std::string hcomUrl = "tcp://127.0.0.1:5684";
    mmc_meta_service_config_t metaServiceConfig;
    UrlStringToChar(metaUrl, metaServiceConfig.discoveryURL);
    auto metaServiceDefault = MmcMakeRef<MmcMetaServiceDefault>("testMetaService");
    MmcMetaServicePtr metaServicePtr = Convert<MmcMetaServiceDefault, MmcMetaService>(metaServiceDefault);
    ASSERT_TRUE(metaServicePtr->Start(metaServiceConfig) == MMC_OK);

    mmc_local_service_config_t localServiceConfig = {"", 0, 0, 1, bmUrl, hcomUrl, 0, 0, "sdma", 0, 104857600, 0};
    UrlStringToChar(metaUrl, localServiceConfig.discoveryURL);
    auto localServiceDefault = MmcMakeRef<MmcLocalServiceDefault>("testLocalService");
    MmcLocalServicePtr localServicePtr = Convert<MmcLocalServiceDefault, MmcLocalService>(localServiceDefault);
    ASSERT_TRUE(localServicePtr->Start(localServiceConfig) == MMC_OK);

    AllocRequest reqAlloc;
    reqAlloc.options_ = {SIZE_32K, 1, 0, 0, 0};
    for (uint16_t i = 0U; i < 5U; ++i) {
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
    GetKeys(0U, 5U, allExistKeys);
    GetKeys(2U, 7U, partExistKeys);
    GetKeys(5U, 10U, allNotExistKeys);

    auto CheckReturn = [&localServiceDefault](const std::vector<std::string> &keys,
                                              const std::vector<Result> &targetResults) {
        BatchIsExistRequest reqExist;
        reqExist.keys_ = keys;
        BatchIsExistResponse respExist;
        ASSERT_TRUE(localServiceDefault->SyncCallMeta(reqExist, respExist, 30) == MMC_OK);
        ASSERT_TRUE(respExist.results_.size() == targetResults.size());
        for (size_t i = 0; i < targetResults.size(); ++i) {
            ASSERT_TRUE(targetResults[i] == respExist.results_[i]);
        }
    };
    CheckReturn(allExistKeys, std::vector<Result>(5, MMC_OK));
    CheckReturn(partExistKeys, {MMC_OK, MMC_OK, MMC_OK, MMC_UNMATCHED_KEY, MMC_UNMATCHED_KEY});
    CheckReturn(allNotExistKeys, std::vector<Result>(5, MMC_UNMATCHED_KEY));
    
    localServicePtr->Stop();
    metaServicePtr->Stop();
}

TEST_F(TestMmcMetaService, QueryRequest)
{
    std::string metaUrl = "tcp://127.0.0.1:5679";
    std::string bmUrl = "tcp://127.0.0.1:5682";
    std::string hcomUrl = "tcp://127.0.0.1:5683";
    mmc_meta_service_config_t metaServiceConfig;
    UrlStringToChar(metaUrl, metaServiceConfig.discoveryURL);
    auto metaServiceDefault = MmcMakeRef<MmcMetaServiceDefault>("testMetaService");
    MmcMetaServicePtr metaServicePtr = Convert<MmcMetaServiceDefault, MmcMetaService>(metaServiceDefault);
    ASSERT_TRUE(metaServicePtr->Start(metaServiceConfig) == MMC_OK);

    mmc_local_service_config_t localServiceConfig = {"", 0, 0, 1, bmUrl, hcomUrl, 0, 0, "sdma", 0, 104857600, 0};
    UrlStringToChar(metaUrl, localServiceConfig.discoveryURL);
    auto localServiceDefault = MmcMakeRef<MmcLocalServiceDefault>("testLocalService");
    MmcLocalServicePtr localServicePtr = Convert<MmcLocalServiceDefault, MmcLocalService>(localServiceDefault);
    ASSERT_TRUE(localServicePtr->Start(localServiceConfig) == MMC_OK);

    AllocRequest reqAlloc;
    reqAlloc.key_ = "test";
    reqAlloc.options_ = {SIZE_32K, 1, 0, 0, 0};
    AllocResponse respAlloc;
    ASSERT_TRUE(localServiceDefault->SyncCallMeta(reqAlloc, respAlloc, 30) == MMC_OK);

    QueryRequest reqQuery;
    reqQuery.key_ = "test";
    QueryResponse respQuery;
    ASSERT_TRUE(localServiceDefault->SyncCallMeta(reqQuery, respQuery, 30) == MMC_OK);
    ASSERT_TRUE(respQuery.queryInfo_.valid_ == true);
    ASSERT_TRUE(respQuery.queryInfo_.size_ == SIZE_32K);
    ASSERT_TRUE(respQuery.queryInfo_.prot_ == 0);
    ASSERT_TRUE(respQuery.queryInfo_.numBlobs_ == 1);

    QueryRequest reqQueryNone;
    reqQueryNone.key_ = "another_test";
    QueryResponse respQueryNone;
    ASSERT_TRUE(localServiceDefault->SyncCallMeta(reqQueryNone, respQueryNone, 30) == MMC_OK);
    ASSERT_TRUE(respQueryNone.queryInfo_.valid_ == false);

    localServicePtr->Stop();
    metaServicePtr->Stop();
}

TEST_F(TestMmcMetaService, BatchQueryRequest)
{
    std::string metaUrl = "tcp://127.0.0.1:5680";
    std::string bmUrl = "tcp://127.0.0.1:5683";
    std::string hcomUrl = "tcp://127.0.0.1:5684";
    mmc_meta_service_config_t metaServiceConfig;
    UrlStringToChar(metaUrl, metaServiceConfig.discoveryURL);
    auto metaServiceDefault = MmcMakeRef<MmcMetaServiceDefault>("testMetaService");
    MmcMetaServicePtr metaServicePtr = Convert<MmcMetaServiceDefault, MmcMetaService>(metaServiceDefault);
    ASSERT_TRUE(metaServicePtr->Start(metaServiceConfig) == MMC_OK);

    mmc_local_service_config_t localServiceConfig = {"", 0, 0, 1, bmUrl, hcomUrl, 0, 0, "sdma", 0, 104857600, 0};
    UrlStringToChar(metaUrl, localServiceConfig.discoveryURL);
    auto localServiceDefault = MmcMakeRef<MmcLocalServiceDefault>("testLocalService");
    MmcLocalServicePtr localServicePtr = Convert<MmcLocalServiceDefault, MmcLocalService>(localServiceDefault);
    ASSERT_TRUE(localServicePtr->Start(localServiceConfig) == MMC_OK);

    AllocRequest reqAlloc;
    std::map<std::string, std::pair<uint32_t, uint8_t>> keyMap;  // key : {size, numBlobs}
    reqAlloc.options_ = {SIZE_32K, 1, 0, 0, 0};
    for (uint16_t i = 0U; i < 3U; ++i) {
        reqAlloc.key_ = "test_" + std::to_string(i);
        reqAlloc.options_.numBlobs_ = i + 1;
        keyMap.insert({reqAlloc.key_, {SIZE_32K, static_cast<uint8_t>(i + 1)}});
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
    GetKeys(0U, 3U, allExistKeys);
    GetKeys(2U, 5U, partExistKeys);
    GetKeys(3U, 7U, allNotExistKeys);

    auto CheckReturn = [&localServiceDefault, &keyMap](const std::vector<std::string> &keys) {
        BatchQueryRequest reqQuery;
        reqQuery.keys_ = keys;
        BatchQueryResponse respQuery;
        ASSERT_TRUE(localServiceDefault->SyncCallMeta(reqQuery, respQuery, 30) == MMC_OK);
        ASSERT_TRUE(respQuery.batchQueryInfos_.size() == keys.size());
        auto queryInfo = respQuery.batchQueryInfos_.begin();
        for (const std::string &key : keys) {
            if (keyMap.find(key) != keyMap.end()) {
                ASSERT_TRUE(true == queryInfo->valid_);
                ASSERT_TRUE(keyMap[key].first == queryInfo->size_);
                ASSERT_TRUE(keyMap[key].second == queryInfo->numBlobs_);
                ASSERT_TRUE(0 == queryInfo->prot_);
            } else {
                ASSERT_TRUE(false == queryInfo->valid_);
            }
            ++queryInfo;
        }
    };
    CheckReturn(allExistKeys);
    CheckReturn(partExistKeys);
    CheckReturn(allNotExistKeys);

    localServicePtr->Stop();
    metaServicePtr->Stop();
}
