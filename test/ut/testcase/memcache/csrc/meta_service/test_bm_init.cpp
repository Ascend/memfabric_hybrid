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

class TestBmInit : public testing::Test {
public:
    TestBmInit();

    void SetUp() override;

    void TearDown() override;

protected:
};
TestBmInit::TestBmInit() {}

void TestBmInit::SetUp()
{
    cout << "this is NetEngine TEST_F setup:" << endl;
}

void TestBmInit::TearDown()
{
    cout << "this is NetEngine TEST_F teardown" << endl;
}

static void UrlStringToChar(std::string &urlString, char *urlChar)
{
    for (uint32_t i = 0; i < urlString.length(); i++) {
        urlChar[i] = urlString.at(i);
    }
    urlChar[urlString.length()] = '\0';
}

TEST_F(TestBmInit, Init)
{
    std::string metaUrl = "tcp://127.0.0.1:5678";
    std::string bmUrl = "tcp://127.0.0.1:5681";
    std::string hcomUrl = "tcp://127.0.0.1:5682";
    mmc_meta_service_config_t metaServiceConfig{};
    metaServiceConfig.logLevel = 0;
    metaServiceConfig.logRotationFileSize = 2 * 1024 * 1024;
    metaServiceConfig.logRotationFileCount = 20;
    metaServiceConfig.evictThresholdHigh = 70;
    metaServiceConfig.evictThresholdLow = 60;
    metaServiceConfig.haEnable = true;
    metaServiceConfig.accTlsConfig.tlsEnable = false;
    UrlStringToChar(metaUrl, metaServiceConfig.discoveryURL);
    UrlStringToChar(bmUrl, metaServiceConfig.configStoreURL);
    auto metaServiceDefault = MmcMakeRef<MmcMetaServiceDefault>("testMetaService");
    MmcMetaServicePtr metaServicePtr = Convert<MmcMetaServiceDefault, MmcMetaService>(metaServiceDefault);
    ASSERT_TRUE(metaServicePtr->Start(metaServiceConfig) == MMC_OK);

    mmc_local_service_config_t localServiceConfig1 = {"", 0, 0, 1, "", "", 0, "device_sdma", 0, 104857600, 0};
    localServiceConfig1.logLevel = 0;
    localServiceConfig1.accTlsConfig.tlsEnable = false;
    UrlStringToChar(metaUrl, localServiceConfig1.discoveryURL);
    UrlStringToChar(bmUrl, localServiceConfig1.bmIpPort);
    UrlStringToChar(hcomUrl, localServiceConfig1.bmHcomUrl);
    auto localServiceDefault1 = MmcMakeRef<MmcLocalServiceDefault>("testLocalService1");
    MmcLocalServicePtr localServicePtr1 = Convert<MmcLocalServiceDefault, MmcLocalService>(localServiceDefault1);
    ASSERT_TRUE(localServicePtr1->Start(localServiceConfig1) == MMC_OK);

    PingMsg req;
    req.msgId = ML_PING_REQ;
    req.num = 123;
    PingMsg resp;
    ASSERT_TRUE(localServiceDefault1->SyncCallMeta(req, resp, 30) == MMC_OK);

    AllocRequest reqAlloc;
    reqAlloc.key_ = "test";
    reqAlloc.options_ = AllocOptions(SIZE_32K, 1, MEDIA_HBM, {0}, 0);
    AllocResponse response;
    ASSERT_TRUE(localServiceDefault1->SyncCallMeta(reqAlloc, response, 30) == MMC_OK);
    ASSERT_TRUE(response.numBlobs_ == 1);
    ASSERT_TRUE(response.blobs_[0].size_ == SIZE_32K);
    localServicePtr1->Stop();
    metaServicePtr->Stop();
}