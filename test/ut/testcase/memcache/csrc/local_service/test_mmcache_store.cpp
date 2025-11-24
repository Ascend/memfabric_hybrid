/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include <iostream>
#include <memory>
#include <stdlib.h>
#include <cstdio>
#include <fstream>
#include "gtest/gtest.h"
#include "mmcache.h"
#include "mmc_def.h"
#include "mmc_service.h"
#include "mmc_client.h"
#include "mmc_types.h"
#include "mmc_env.h"
#include "mmc.h"
#include "mmcache_store.h"

using namespace testing;
using namespace std;
using namespace ock::mmc;

class TestMmcacheStore : public testing::Test {
public:
    TestMmcacheStore();

    void SetUp() override;

    void TearDown() override;

protected:
    std::string confPath_ = "./local-service.conf";
};

TestMmcacheStore::TestMmcacheStore() {}

void TestMmcacheStore::SetUp()
{
    cout << "this is TestMmcacheStore TEST_F setup:" << std::endl;
}

void TestMmcacheStore::TearDown()
{
    cout << "this is TestMmcacheStore TEST_F teardown" << std::endl;
    std::remove(confPath_.c_str());
}

static int GenerateLocalConf(std::string confPath)
{
    std::ofstream outFile(confPath);
    if (!outFile.is_open()) {
        std::cerr << "无法打开文件！" << std::endl;
        return 1;
    }

    outFile << "ock.mmc.meta_service_url = tcp://127.0.0.1:5869" << std::endl;
    outFile << " ock.mmc.log_level = info " << std::endl;

    outFile << "ock.mmc.tls.enable = false" << std::endl;
    outFile << "ock.mmc.tls.ca.path = /opt/ock/security/certs/ca.cert.pem" << std::endl;
    outFile << "ock.mmc.tls.ca.crl.path = /opt/ock/security/certs/ca.crl.pem" << std::endl;
    outFile << "ock.mmc.tls.cert.path = /opt/ock/security/certs/client.cert.pem" << std::endl;
    outFile << "ock.mmc.tls.key.path = /opt/ock/security/certs/client.private.key.pem" << std::endl;
    outFile << "ock.mmc.tls.key.pass.path = /opt/ock/security/certs/client.passphrase" << std::endl;
    outFile << "ock.mmc.tls.package.path = /opt/ock/security/libs/" << std::endl;
    outFile << "ock.mmc.tls.decrypter.path =" << std::endl;

    outFile << "ock.mmc.local_service.world_size = 1" << std::endl;
    outFile << "ock.mmc.local_service.config_store_url = tcp://127.0.0.1:5882" << std::endl;
    outFile << "ock.mmc.config_store.tls.enable = false" << std::endl;
    outFile << "ock.mmc.config_store.tls.ca.path = /opt/ock/security/certs/ca.cert.pem" << std::endl;
    outFile << "ock.mmc.config_store.tls.ca.crl.path = /opt/ock/security/certs/ca.crl.pem" << std::endl;
    outFile << "ock.mmc.config_store.tls.cert.path = /opt/ock/security/certs/client.cert.pem" << std::endl;
    outFile << "ock.mmc.config_store.tls.key.path = /opt/ock/security/certs/client.private.key.pem" << std::endl;
    outFile << "ock.mmc.config_store.tls.key.pass.path = /opt/ock/security/certs/client.passphrase" << std::endl;
    outFile << "ock.mmc.config_store.tls.package.path = /opt/ock/security/libs/" << std::endl;
    outFile << "ock.mmc.config_store.tls.decrypter.path =" << std::endl;
    outFile << "ock.mmc.local_service.protocol = device_sdma" << std::endl;
    outFile << "ock.mmc.local_service.dram.size = 32MB" << std::endl;
    outFile << "ock.mmc.local_service.hbm.size = 32MB" << std::endl;

    outFile << "ock.mmc.local_service.hcom_url = tcp://127.0.0.1:7000" << std::endl;
    outFile << "ock.mmc.local_service.hcom.tls.enable = false" << std::endl;
    outFile << "ock.mmc.local_service.hcom.tls.ca.path = /opt/ock/security/certs/ca.cert.pem" << std::endl;
    outFile << "ock.mmc.local_service.hcom.tls.ca.crl.path = /opt/ock/security/certs/ca.crl.pem" << std::endl;
    outFile << "ock.mmc.local_service.hcom.tls.cert.path = /opt/ock/security/certs/client.cert.pem" << std::endl;
    outFile << "ock.mmc.local_service.hcom.tls.key.path = /opt/ock/security/certs/client.private.key.pem" << std::endl;
    outFile << "ock.mmc.local_service.hcom.tls.key.pass.path = /opt/ock/security/certs/client.passphrase" << std::endl;
    outFile << "ock.mmc.local_service.hcom.tls.decrypter.path =" << std::endl;

    outFile << "ock.mmc.client.retry_milliseconds = 0" << std::endl;
    outFile << "ock.mmc.client.timeout.seconds = 60" << std::endl;

    outFile.close();
    return 0;
}

static void UrlStringToChar(std::string &urlString, char *urlChar)
{
    for (uint32_t i = 0; i < urlString.length(); i++) {
        urlChar[i] = urlString.at(i);
    }
    urlChar[urlString.length()] = '\0';
}

static void TestStore(std::shared_ptr<ObjectStore> &store, const std::vector<void *> &buffers,
                      const std::vector<size_t> &sizes)
{
    auto ret = store->PutFromLayers("k", buffers, sizes, 33);
    EXPECT_NE(ret, 0);
    ret = store->PutFromLayers("", buffers, sizes, 3);
    EXPECT_NE(ret, 0);
    ret = store->PutFromLayers("", {}, {}, 3);
    EXPECT_NE(ret, 0);
    uint32_t localServiceId = 0;
    store->GetLocalServiceId(localServiceId);
    EXPECT_EQ(localServiceId, 0);
    store->RegisterBuffer(buffers[0], sizes[0]);
    ret = store->PutFrom("k", buffers[0], sizes[0], 3);
    EXPECT_EQ(ret, 0);
    store->BatchIsExist({"k"});
    auto info = store->GetKeyInfo("k");
    EXPECT_NE(info.GetBlobNum(), 0);
    store->BatchGetKeyInfo({"k"});
    store->BatchRemove({"k"});
    auto bret = store->BatchPutFrom({"k"}, buffers, sizes, 3);
    EXPECT_EQ(!bret.empty(), true);
    store->BatchGetInto({"k"}, buffers, sizes, 3);
    store->BatchRemove({"k"});
    bret = store->BatchPutFromLayers({"k"}, {buffers}, {sizes}, 3);
    EXPECT_EQ(bret[0], 0);
    store->BatchGetIntoLayers({"K"}, {buffers}, {sizes}, 2);
    store->BatchRemove({"k"});
}

TEST_F(TestMmcacheStore, Init)
{
    // 1、启动 meta
    std::string metaUrl = "tcp://127.0.0.1:5869";
    std::string bmUrl = "tcp://127.0.0.1:5882";

    mmc_meta_service_config_t metaServiceConfig{};
    metaServiceConfig.logLevel = 0;
    metaServiceConfig.logRotationFileSize = 2 * 1024 * 1024;
    metaServiceConfig.logRotationFileCount = 20;
    metaServiceConfig.accTlsConfig.tlsEnable = false;
    metaServiceConfig.evictThresholdHigh = 80;
    metaServiceConfig.evictThresholdLow = 60;
    metaServiceConfig.haEnable = false;
    UrlStringToChar(metaUrl, metaServiceConfig.discoveryURL);
    UrlStringToChar(bmUrl, metaServiceConfig.configStoreURL);
    mmc_meta_service_t meta_service = mmcs_meta_service_start(&metaServiceConfig);
    ASSERT_TRUE(meta_service != nullptr);
    mmc_set_extern_logger([](int level, const char *msg) { std::cerr << msg << std::endl; });
    mmc_set_log_level(1);
    // 2、启动 local
    std::shared_ptr<ObjectStore> store = ObjectStore::CreateObjectStore();
    auto ret = GenerateLocalConf(confPath_);
    ASSERT_EQ(ret, 0);
    MMC_LOCAL_CONF_PATH = confPath_;
    ret = store->Init(0);
    ASSERT_EQ(ret, 0);

    std::string key = "test1";
    std::vector<void *> buffers;
    std::vector<size_t> sizes;
    buffers.push_back((void *)malloc(1024));
    sizes.push_back(1024);
    buffers.push_back((void *)malloc(4096));
    sizes.push_back(4096);
    buffers.push_back((void *)malloc(1024));
    sizes.push_back(1024);
    buffers.push_back((void *)malloc(4096));
    sizes.push_back(4096);
    TestStore(store, buffers, sizes);
    ret = store->PutFromLayers(key, buffers, sizes, 3);
    EXPECT_EQ(ret, 0);
    ret = store->IsExist(key);
    EXPECT_EQ(ret, 1);
    ret = store->GetIntoLayers(key, buffers, sizes, 2);
    EXPECT_EQ(ret, 0);
    ret = store->Remove(key);
    EXPECT_EQ(ret, 0);
    // 3、stop
    store->TearDown();
    mmcs_meta_service_stop(meta_service);
}

static mmc_meta_service_t StartMetaService()
{
    // 1、启动 meta
    std::string metaUrl = "tcp://127.0.0.1:5869";
    std::string bmUrl = "tcp://127.0.0.1:5882";

    mmc_meta_service_config_t metaServiceConfig{};
    metaServiceConfig.logLevel = 0;
    metaServiceConfig.logRotationFileSize = 2 * 1024 * 1024;
    metaServiceConfig.logRotationFileCount = 20;
    metaServiceConfig.accTlsConfig.tlsEnable = false;
    metaServiceConfig.evictThresholdHigh = 80;
    metaServiceConfig.evictThresholdLow = 60;
    metaServiceConfig.haEnable = false;
    UrlStringToChar(metaUrl, metaServiceConfig.discoveryURL);
    UrlStringToChar(bmUrl, metaServiceConfig.configStoreURL);
    mmc_meta_service_t meta_service = mmcs_meta_service_start(&metaServiceConfig);
    return meta_service;
}

TEST_F(TestMmcacheStore, RegisterBufferTest)
{
    std::shared_ptr<ObjectStore> store = ObjectStore::CreateObjectStore();
    void *nonPtr = nullptr;
    int test = 1;
    void *testPtr = &test;
    auto ret = store->RegisterBuffer(nonPtr, 0);
    EXPECT_NE(ret, 0);

    ret = store->RegisterBuffer(testPtr, 0);
    EXPECT_NE(ret, 0);

    ret = store->RegisterBuffer(testPtr, 1024 * 1024 * 2);
    EXPECT_NE(ret, 0);

    auto meta_service = StartMetaService();
    EXPECT_NE(meta_service, nullptr);
    ret = GenerateLocalConf(confPath_);
    ASSERT_EQ(ret, 0);
    MMC_LOCAL_CONF_PATH = confPath_;
    ret = store->Init(0);
    ASSERT_EQ(ret, 0);

    ret = store->RegisterBuffer(testPtr, 1024 * 1024 * 2);
    EXPECT_EQ(ret, 0);

    store->TearDown();
    mmcs_meta_service_stop(meta_service);
}

TEST_F(TestMmcacheStore, GetIntoTest)
{
    std::shared_ptr<ObjectStore> store = ObjectStore::CreateObjectStore();
    auto ret = store->GetInto("test", nullptr, 0, 0);
    EXPECT_NE(ret, 0);

    ret = store->GetInto("test", nullptr, 0, 1);
    EXPECT_NE(ret, 0);

    ret = store->GetInto("test", nullptr, 0, 2);
    EXPECT_NE(ret, 0);
}

TEST_F(TestMmcacheStore, PutFromTest)
{
    std::shared_ptr<ObjectStore> store = ObjectStore::CreateObjectStore();
    auto ret = store->PutFrom("test", nullptr, 0, 0);
    EXPECT_NE(ret, 0);

    ret = store->PutFrom("test", nullptr, 0, 1);
    EXPECT_NE(ret, 0);

    ret = store->PutFrom("test", nullptr, 0, 2);
    EXPECT_NE(ret, 0);
}

TEST_F(TestMmcacheStore, BatchRemoveTest)
{
    std::shared_ptr<ObjectStore> store = ObjectStore::CreateObjectStore();

    std::vector<std::string> keys;
    auto ret = store->BatchRemove(keys);
    EXPECT_EQ(ret.size(), 0);

    keys.emplace_back("test");
    ret = store->BatchRemove(keys);
    EXPECT_NE(ret[0], 0);

    for (int i = 0; i < MAX_BATCH_OP_COUNT + 1; ++i) {
        keys.push_back("test_" + std::to_string(i));
    }
    ret = store->BatchRemove(keys);
    EXPECT_EQ(ret[0], MMC_INVALID_PARAM);
}

TEST_F(TestMmcacheStore, BatchIsExist)
{
    std::shared_ptr<ObjectStore> store = ObjectStore::CreateObjectStore();
    std::vector<std::string> keys;

    auto ret = store->BatchIsExist(keys);
    EXPECT_EQ(ret.size(), 0);

    keys.emplace_back("test");
    ret = store->BatchIsExist(keys);
    EXPECT_NE(ret[0], 0);

    for (int i = 0; i < MAX_BATCH_OP_COUNT + 1; ++i) {
        keys.push_back("test_" + std::to_string(i));
    }
    ret = store->BatchIsExist(keys);
    EXPECT_EQ(ret[0], MMC_INVALID_PARAM);
}