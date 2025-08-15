/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <secodefuzz/secodeFuzz.h>
#include "dt_fuzz.h"
#include "mmc_client.h"
#include "mmc_def.h"
#include "mmc_bm_proxy.h"
#include "mmc_local_service_default.h"
#include "mmc_msg_base.h"
#include "memcache_stubs.h"
#include "mmc_config_const.h"

using namespace ock::mmc;

namespace {

class TestMmcClient : public testing::Test {
public:
    static void SetUpTestSuite();
    static void TearDownTestSuite();
    void SetUp();
    void TearDown();

protected:
    static NetEnginePtr mServer;
    static NetEngineOptions mServerOptions;
    static MmcLocalServiceDefault mLocalService;
    static mmc_client_config_t mClientConfig;
};

NetEnginePtr TestMmcClient::mServer = NetEngine::Create();

NetEngineOptions TestMmcClient::mServerOptions;

MmcLocalServiceDefault TestMmcClient::mLocalService("FuzzService");

mmc_client_config_t TestMmcClient::mClientConfig;

void TestMmcClient::SetUpTestSuite()
{
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(DT_RUNNING_TIME);

    MockSmemBm();

    mServerOptions.name = "test_engine";
    mServerOptions.ip = "127.0.0.1";
    mServerOptions.port = 5559;
    mServerOptions.threadCount = 2;
    mServerOptions.rankId = 0;
    mServerOptions.tlsOption.tlsEnable = false;
    mServerOptions.startListener = true;

    mClientConfig.logLevel = 0;
    mClientConfig.tlsConfig.tlsEnable = false;
    mClientConfig.rankId = 0;
    const char * discoveryURL = "tcp://127.0.0.1:5559";
    std::copy_n(discoveryURL, std::strlen(discoveryURL) + 1, mClientConfig.discoveryURL);
    mClientConfig.discoveryURL[std::strlen(discoveryURL)] = '\0';
}

void TestMmcClient::TearDownTestSuite()
{
    GlobalMockObject::verify();
}

void TestMmcClient::SetUp()
{
    RegisterMockHandles(mServer);
    ASSERT_EQ(mServer->Start(mServerOptions), 0);

    mmc_local_service_config_t config{ .discoveryURL = "tcp://127.0.0.1:5559", .dataOpType = "tcp", .localHBMSize = 1 };
    mLocalService.Start(config);
}

void TestMmcClient::TearDown()
{
}

TEST_F(TestMmcClient, TestMmcClientInit)
{
    char fuzzName[] = "TestMmcClientInit";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        ASSERT_EQ(0, mmcc_init(&mClientConfig));
        mmcc_uninit();
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcClient, TestMmcClientUninit)
{
    char fuzzName[] = "TestMmcClientUninit";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        ASSERT_EQ(0, mmcc_init(&mClientConfig));
        mmcc_uninit();
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcClient, TestMmcClientExist)
{
    char fuzzName[] = "TestMmcClientExist";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        auto existResp = GetIsExistResp();
        existResp->ret_ = 0;
        ASSERT_EQ(0, mmcc_init(&mClientConfig));
        char *key = DT_SetGetStringV3(0, 9, 258, (char*)"test_key");
        ASSERT_EQ(strlen(key) > 0 && strlen(key) <= 256, !bool(mmcc_exist(key, 0)));
        mmcc_uninit();
        ResetResp();
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcClient, TestMmcClientBatchExist)
{
    char fuzzName[] = "TestMmcClientBatchExist";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        auto batchExistResp = GetBatchIsExistResp();
        ASSERT_EQ(0, mmcc_init(&mClientConfig));
        uint32_t keys_count = 3;
        char *key1 = DT_SetGetStringV3(0, 9, 258, (char*)"test_key");
        char *key2 = DT_SetGetStringV3(1, 9, 258, (char*)"test_key");
        char *key3 = DT_SetGetStringV3(2, 9, 258, (char*)"test_key");
        const char *keys[] = {key1, key2, key3};
        int32_t results[] = {-1, -1, -1};

        for (size_t i = 0; i < keys_count; ++i) {
            if (strlen(keys[i]) > 0 && strlen(keys[i]) <= 256) {
                batchExistResp->results_.push_back(0);
            }
        }

        ASSERT_EQ(0, mmcc_batch_exist(keys, keys_count, results, 0));
        for (size_t i = 0; i < keys_count; ++i) {
            ASSERT_EQ(strlen(keys[i]) > 0 && strlen(keys[i]) <= 256, !bool(results[i]));  // 0 - exist
        }

        mmcc_uninit();
        ResetResp();
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcClient, TestMmcClientQuery)
{
    char fuzzName[] = "TestMmcClientQuery";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        auto queryResp = GetQueryResp();
        queryResp->queryInfo_ = MemObjQueryInfo(0, 0, 0, true);
        ASSERT_EQ(0, mmcc_init(&mClientConfig));
        char *key = DT_SetGetStringV3(0, 9, 258, (char*)"test_key");
        mmc_data_info info;
        ASSERT_EQ(strlen(key) > 0 && strlen(key) <= 256, !bool(mmcc_query(key, &info, 0)));
        mmcc_uninit();
        ResetResp();
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcClient, TestMmcClientBatchQuery)
{
    char fuzzName[] = "TestMmcClientBatchQuery";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        auto batchQueryResp = GetBatchQueryResp();
        ASSERT_EQ(0, mmcc_init(&mClientConfig));

        uint32_t keys_count = 3;
        char *key1 = DT_SetGetStringV3(0, 9, 258, (char*)"test_key");
        char *key2 = DT_SetGetStringV3(1, 9, 258, (char*)"test_key");
        char *key3 = DT_SetGetStringV3(2, 9, 258, (char*)"test_key");
        const char *keys[] = {key1, key2, key3};
        mmc_data_info infos[] = {mmc_data_info(), mmc_data_info(), mmc_data_info()};

        for (size_t i = 0; i < keys_count; ++i) {
            if (strlen(keys[i]) > 0 && strlen(keys[i]) <= 256) {
                batchQueryResp->batchQueryInfos_.push_back(MemObjQueryInfo(1, 0, 1, true));
            }
        }

        ASSERT_EQ(0, mmcc_batch_query(keys, keys_count, infos, 0));
        for (size_t i = 0; i < keys_count; ++i) {
            ASSERT_EQ(strlen(keys[i]) > 0 && strlen(keys[i]) <= 256, infos[i].valid);
        }

        mmcc_uninit();
        ResetResp();
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcClient, TestMmcClientPutOneDim)
{
    char fuzzName[] = "TestMmcClientPutOneDim";
    int policyEnum[] = {0};
    int typeEnum[] = {0, 1};
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        auto allocResp = GetAllocResp();
        auto resp = GetResp();
        allocResp->blobs_ = std::vector<MmcMemBlobDesc>(1, MmcMemBlobDesc());
        allocResp->numBlobs_ = 1;
        allocResp->prot_ = 0;
        allocResp->priority_ = 0;
        resp->ret_ = 0;

        ASSERT_EQ(0, mmcc_init(&mClientConfig));

        char *key = DT_SetGetStringV3(0, 9, 258, (char*)"test_key");

        mmc_put_options opt;
        opt.mediaType = *(u16 *)DT_SetGetU16V3(1, 0);
        opt.policy = static_cast<affinity_policy>(*(s32 *)DT_SetGetNumberEnumV3(2, 0, policyEnum, 1));

        mmc_buffer buf;
        buf.addr = *(u64 *)DT_SetGetU64V3(3, 50);
        buf.type = *(u32 *)DT_SetGetNumberEnumV3(4, 0, typeEnum, 2);

        // OneDim Case
        buf.dimType = 0;
        uint64_t offset = *(u64 *)DT_SetGetU64V3(5, 3);
        uint64_t len = *(u64 *)DT_SetGetU64V3(6, 50);
        buf.oneDim = {offset, len};
        allocResp->blobs_[0].size_ = buf.oneDim.len;

        ASSERT_EQ(strlen(key) > 0 && strlen(key) <= 256 && buf.addr != 0, !bool(mmcc_put(key, &buf, opt, 0)));

        mmcc_uninit();
        ResetResp();
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcClient, TestMmcClientPutTwoDim)
{
    char fuzzName[] = "TestMmcClientPutTwoDim";
    int policyEnum[] = {0};
    int typeEnum[] = {0, 1};
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        auto allocResp = GetAllocResp();
        auto resp = GetResp();
        allocResp->blobs_ = std::vector<MmcMemBlobDesc>(1, MmcMemBlobDesc());
        allocResp->numBlobs_ = 1;
        allocResp->prot_ = 0;
        allocResp->priority_ = 0;
        resp->ret_ = 0;

        ASSERT_EQ(0, mmcc_init(&mClientConfig));

        char *key = DT_SetGetStringV3(0, 9, 258, (char*)"test_key");

        mmc_put_options opt;
        opt.mediaType = *(u16 *)DT_SetGetU16V3(1, 0);
        opt.policy = static_cast<affinity_policy>(*(s32 *)DT_SetGetNumberEnumV3(2, 0, policyEnum, 1));

        mmc_buffer buf;
        buf.addr = *(u64 *)DT_SetGetU64V3(3, 50);
        buf.type = *(u32 *)DT_SetGetNumberEnumV3(4, 0, typeEnum, 2);

        // TwoDim Case
        buf.dimType = 1;
        uint64_t dpitch_high = *(u16 *)DT_SetGetU16V3(5, 0);
        uint64_t dpitch_low = *(u32 *)DT_SetGetU32V3(6, 50);
        uint64_t dpitch = (dpitch_high << 32) + dpitch_low; // dpitch 在结构体定义中指定为 48 位
        uint64_t layerOffset = *(u16 *)DT_SetGetU16V3(7, 1); // layerOffset 在结构体定义中指定为 16 位
        uint32_t width = *(u32 *)DT_SetGetU32V3(8, 50);
        uint16_t layerNum = *(u16 *)DT_SetGetU16V3(9, 3);
        uint16_t layerCount = *(u16 *)DT_SetGetU16V3(10, 4);
        buf.twoDim = {dpitch, layerOffset, width, layerNum, layerCount};
        allocResp->blobs_[0].size_ = buf.twoDim.width * buf.twoDim.layerNum;

        ASSERT_EQ(strlen(key) > 0 && strlen(key) <= 256 && buf.addr != 0 && dpitch > width,
                  !bool(mmcc_put(key, &buf, opt, 0)));

        mmcc_uninit();
        ResetResp();
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcClient, TestMmcClientBatchPutOneDim)
{
    char fuzzName[] = "TestMmcClientBatchPutOneDim";
    int policyEnum[] = {0};
    int typeEnum[] = {0, 1};
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        auto batchAllocResp = GetBatchAllocResp();
        auto batchUpdateResp = GetBatchUpdateResp();
        std::vector<MmcMemBlobDesc> blob(1, MmcMemBlobDesc());

        ASSERT_EQ(0, mmcc_init(&mClientConfig));

        uint32_t keys_count = 3;
        char *key1 = DT_SetGetStringV3(0, 9, 258, (char*)"test_key");
        char *key2 = DT_SetGetStringV3(1, 9, 258, (char*)"test_key");
        char *key3 = DT_SetGetStringV3(2, 9, 258, (char*)"test_key");
        const char *keys[] = {key1, key2, key3};

        mmc_put_options opt;
        opt.mediaType = *(u16 *)DT_SetGetU16V3(3, 0);
        opt.policy = static_cast<affinity_policy>(*(s32 *)DT_SetGetNumberEnumV3(4, 0, policyEnum, 1));

        int dtSetGetCnt = 5;
        bool isAllValid = true;
        mmc_buffer bufs[keys_count];
        for (size_t i = 0; i < keys_count; ++i) {
            bufs[i].addr = *(u64 *)DT_SetGetU64V3(dtSetGetCnt++, 50);
            bufs[i].type = *(u32 *)DT_SetGetNumberEnumV3(dtSetGetCnt++, 0, typeEnum, 2);
            isAllValid &= bufs[i].addr != 0;
        }

        int32_t results[] = {-1, -1, -1};

        // OneDim Case
        for (size_t i = 0; i < keys_count; ++i) {
            bufs[i].dimType = 0;
            uint64_t offset = *(u64 *)DT_SetGetU64V3(dtSetGetCnt++, 3);
            uint64_t len = *(u64 *)DT_SetGetU64V3(dtSetGetCnt++, 50);
            bufs[i].oneDim = {offset, len};
        }

        for (size_t i = 0; i < keys_count; ++i) {
            if (strlen(keys[i]) > 0 && strlen(keys[i]) <= 256 && bufs[i].addr != 0) {
                batchAllocResp->blobs_.push_back(blob);
                batchAllocResp->blobs_.back()[0].size_ = bufs[i].oneDim.len;
                batchAllocResp->numBlobs_.push_back(1);
                batchAllocResp->prots_.push_back(0);
                batchAllocResp->priorities_.push_back(0);
                batchAllocResp->leases_.push_back(0);
                batchAllocResp->results_.push_back(0);
                batchUpdateResp->results_.push_back(0);
            }
        }

        isAllValid &= batchAllocResp->blobs_.size() == keys_count;
        ASSERT_EQ(isAllValid, !bool(mmcc_batch_put(keys, keys_count, bufs, opt, 0, results)));
        if (isAllValid) {
            for (size_t i = 0; i < keys_count; ++i) {
                ASSERT_EQ(strlen(keys[i]) > 0 && strlen(keys[i]) <= 256 && bufs[i].addr != 0, !bool(results[i]));
            }
        }

        mmcc_uninit();
        ResetResp();
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcClient, TestMmcClientBatchPutTwoDim)
{
    char fuzzName[] = "TestMmcClientBatchPutTwoDim";
    int policyEnum[] = {0};
    int typeEnum[] = {0, 1};
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        auto batchAllocResp = GetBatchAllocResp();
        auto batchUpdateResp = GetBatchUpdateResp();
        std::vector<MmcMemBlobDesc> blob(1, MmcMemBlobDesc());

        ASSERT_EQ(0, mmcc_init(&mClientConfig));

        uint32_t keys_count = 3;
        char *key1 = DT_SetGetStringV3(0, 9, 258, (char*)"test_key");
        char *key2 = DT_SetGetStringV3(1, 9, 258, (char*)"test_key");
        char *key3 = DT_SetGetStringV3(2, 9, 258, (char*)"test_key");
        const char *keys[] = {key1, key2, key3};

        mmc_put_options opt;
        opt.mediaType = *(u16 *)DT_SetGetU16V3(3, 0);
        opt.policy = static_cast<affinity_policy>(*(s32 *)DT_SetGetNumberEnumV3(4, 0, policyEnum, 1));

        int dtSetGetCnt = 5;
        mmc_buffer bufs[keys_count];
        for (size_t i = 0; i < keys_count; ++i) {
            bufs[i].addr = *(u64 *)DT_SetGetU64V3(dtSetGetCnt++, 50);
            bufs[i].type = *(u32 *)DT_SetGetNumberEnumV3(dtSetGetCnt++, 0, typeEnum, 2);
        }

        int32_t results[] = {-1, -1, -1};

        // TwoDim Case
        for (size_t i = 0; i < keys_count; ++i) {
            bufs[i].dimType = 1;
            uint64_t dpitch_high = *(u16 *)DT_SetGetU16V3(dtSetGetCnt++, 0);
            uint64_t dpitch_low = *(u32 *)DT_SetGetU32V3(dtSetGetCnt++, 50);
            uint64_t dpitch = (dpitch_high << 32) + dpitch_low; // dpitch 在结构体定义中指定为 48 位
            uint64_t layerOffset = *(u16 *)DT_SetGetU16V3(dtSetGetCnt++, 1); // layerOffset 在结构体定义中指定为 16 位
            uint32_t width = *(u32 *)DT_SetGetU32V3(dtSetGetCnt++, 50);
            uint16_t layerNum = *(u16 *)DT_SetGetU16V3(dtSetGetCnt++, 3);
            uint16_t layerCount = *(u16 *)DT_SetGetU16V3(dtSetGetCnt++, 4);
            bufs[i].twoDim = {dpitch, layerOffset, width, layerNum, layerCount};
        }

        for (size_t i = 0; i < keys_count; ++i) {
            if (strlen(keys[i]) > 0 && strlen(keys[i]) <= 256 && bufs[i].addr != 0) {
                batchAllocResp->blobs_.push_back(blob);
                batchAllocResp->blobs_.back()[0].size_ = bufs[i].twoDim.width * bufs[i].twoDim.layerNum;
                batchAllocResp->numBlobs_.push_back(1);
                batchAllocResp->prots_.push_back(0);
                batchAllocResp->priorities_.push_back(0);
                batchAllocResp->leases_.push_back(0);
                batchAllocResp->results_.push_back(0);
                batchUpdateResp->results_.push_back(0);
            }
        }

        bool isAllValid = batchAllocResp->blobs_.size() == keys_count;
        ASSERT_EQ(isAllValid, !bool(mmcc_batch_put(keys, keys_count, bufs, opt, 0, results)));
        if (isAllValid) {
            for (size_t i = 0; i < keys_count; ++i) {
                ASSERT_EQ(strlen(keys[i]) > 0 && strlen(keys[i]) <= 256 && bufs[i].addr != 0 &&
                          bufs[i].twoDim.dpitch > bufs[i].twoDim.width, !bool(results[i]));
            }
        }

        mmcc_uninit();
        ResetResp();
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcClient, TestMmcClientGetOneDim)
{
    char fuzzName[] = "TestMmcClientGetOneDim";
    int typeEnum[] = {0, 1};
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        auto allocResp = GetAllocResp();
        auto resp = GetResp();
        allocResp->blobs_ = std::vector<MmcMemBlobDesc>(1, MmcMemBlobDesc());
        allocResp->numBlobs_ = 1;
        allocResp->prot_ = 0;
        allocResp->priority_ = 0;
        resp->ret_ = 0;

        ASSERT_EQ(0, mmcc_init(&mClientConfig));

        char *key = DT_SetGetStringV3(0, 9, 258, (char*)"test_key");

        mmc_buffer buf;
        buf.addr = *(u64 *)DT_SetGetU64V3(1, 50);
        buf.type = *(u32 *)DT_SetGetNumberEnumV3(2, 0, typeEnum, 2);

        // OneDim Case
        buf.dimType = 0;
        uint64_t offset = *(u64 *)DT_SetGetU64V3(3, 3);
        uint64_t len = *(u64 *)DT_SetGetU64V3(4, 50);
        buf.oneDim = {offset, len};
        allocResp->blobs_[0].size_ = buf.oneDim.len;

        ASSERT_EQ(strlen(key) > 0 && strlen(key) <= 256 && buf.addr != 0, !bool(mmcc_get(key, &buf, 0)));

        mmcc_uninit();
        ResetResp();
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcClient, TestMmcClientGetTwoDim)
{
    char fuzzName[] = "TestMmcClientGetTwoDim";
    int typeEnum[] = {0, 1};
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        auto allocResp = GetAllocResp();
        auto resp = GetResp();
        allocResp->blobs_ = std::vector<MmcMemBlobDesc>(1, MmcMemBlobDesc());
        allocResp->numBlobs_ = 1;
        allocResp->prot_ = 0;
        allocResp->priority_ = 0;
        resp->ret_ = 0;

        ASSERT_EQ(0, mmcc_init(&mClientConfig));

        char *key = DT_SetGetStringV3(0, 9, 258, (char*)"test_key");

        mmc_buffer buf;
        buf.addr = *(u64 *)DT_SetGetU64V3(1, 50);
        buf.type = *(u32 *)DT_SetGetNumberEnumV3(2, 0, typeEnum, 2);

        // TwoDim Case
        buf.dimType = 1;
        uint64_t dpitch_high = *(u16 *)DT_SetGetU16V3(3, 0);
        uint64_t dpitch_low = *(u32 *)DT_SetGetU32V3(4, 50);
        uint64_t dpitch = (dpitch_high << 32) + dpitch_low; // dpitch 在结构体定义中指定为 48 位
        uint64_t layerOffset = *(u16 *)DT_SetGetU16V3(5, 1); // layerOffset 在结构体定义中指定为 16 位
        uint32_t width = *(u32 *)DT_SetGetU32V3(6, 50);
        uint16_t layerNum = *(u16 *)DT_SetGetU16V3(7, 3);
        uint16_t layerCount = *(u16 *)DT_SetGetU16V3(8, 4);
        buf.twoDim = {dpitch, layerOffset, width, layerNum, layerCount};
        allocResp->blobs_[0].size_ = buf.twoDim.width * buf.twoDim.layerNum;

        ASSERT_EQ(strlen(key) > 0 && strlen(key) <= 256 && buf.addr != 0 && buf.twoDim.dpitch >= buf.twoDim.width,
                  !bool(mmcc_get(key, &buf, 0)));

        mmcc_uninit();
        ResetResp();
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcClient, TestMmcClientBatchGetOneDim)
{
    char fuzzName[] = "TestMmcClientBatchGetOneDim";
    int typeEnum[] = {0, 1};
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        auto batchAllocResp = GetBatchAllocResp();
        auto batchUpdateResp = GetBatchUpdateResp();
        std::vector<MmcMemBlobDesc> blob(1, MmcMemBlobDesc());

        ASSERT_EQ(0, mmcc_init(&mClientConfig));

        uint32_t keys_count = 3;
        char *key1 = DT_SetGetStringV3(0, 9, 258, (char*)"test_key");
        char *key2 = DT_SetGetStringV3(1, 9, 258, (char*)"test_key");
        char *key3 = DT_SetGetStringV3(2, 9, 258, (char*)"test_key");
        const char *keys[] = {key1, key2, key3};

        int dtSetGetCnt = 3;
        mmc_buffer bufs[keys_count];
        for (size_t i = 0; i < keys_count; ++i) {
            bufs[i].addr = *(u64 *)DT_SetGetU64V3(dtSetGetCnt++, 50);
            bufs[i].type = *(u32 *)DT_SetGetNumberEnumV3(dtSetGetCnt++, 0, typeEnum, 2);
        }

        int32_t results[] = {-1, -1, -1};

        // OneDim Case
        for (size_t i = 0; i < keys_count; ++i) {
            bufs[i].dimType = 0;
            uint64_t offset = *(u64 *)DT_SetGetU64V3(dtSetGetCnt++, 3);
            uint64_t len = *(u64 *)DT_SetGetU64V3(dtSetGetCnt++, 50);
            bufs[i].oneDim = {offset, len};
        }

        for (size_t i = 0; i < keys_count; ++i) {
            if (strlen(keys[i]) > 0 && strlen(keys[i]) <= 256 && bufs[i].addr != 0) {
                batchAllocResp->blobs_.push_back(blob);
                batchAllocResp->blobs_.back()[0].size_ = bufs[i].oneDim.len;
                batchAllocResp->numBlobs_.push_back(1);
                batchAllocResp->prots_.push_back(0);
                batchAllocResp->priorities_.push_back(0);
                batchAllocResp->leases_.push_back(0);
                batchAllocResp->results_.push_back(0);
                batchUpdateResp->results_.push_back(0);
            }
        }

        bool isAllValid = batchAllocResp->blobs_.size() == keys_count;
        ASSERT_EQ(isAllValid, !bool(mmcc_batch_get(keys, keys_count, bufs, 0, results)));
        if (isAllValid) {
            for (size_t i = 0; i < keys_count; ++i) {
                ASSERT_EQ(strlen(keys[i]) > 0 && strlen(keys[i]) <= 256 && bufs[i].addr != 0, !bool(results[i]));
            }
        }

        mmcc_uninit();
        ResetResp();
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcClient, TestMmcClientBatchGetTwoDim)
{
    char fuzzName[] = "TestMmcClientBatchGetTwoDim";
    int typeEnum[] = {0, 1};
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        auto batchAllocResp = GetBatchAllocResp();
        auto batchUpdateResp = GetBatchUpdateResp();
        std::vector<MmcMemBlobDesc> blob(1, MmcMemBlobDesc());

        ASSERT_EQ(0, mmcc_init(&mClientConfig));

        uint32_t keys_count = 3;
        char *key1 = DT_SetGetStringV3(0, 9, 258, (char*)"test_key");
        char *key2 = DT_SetGetStringV3(1, 9, 258, (char*)"test_key");
        char *key3 = DT_SetGetStringV3(2, 9, 258, (char*)"test_key");
        const char *keys[] = {key1, key2, key3};

        int dtSetGetCnt = 3;
        mmc_buffer bufs[keys_count];
        for (size_t i = 0; i < keys_count; ++i) {
            bufs[i].addr = *(u64 *)DT_SetGetU64V3(dtSetGetCnt++, 50);
            bufs[i].type = *(u32 *)DT_SetGetNumberEnumV3(dtSetGetCnt++, 0, typeEnum, 2);
        }

        int32_t results[] = {-1, -1, -1};

        // TwoDim Case
        for (size_t i = 0; i < keys_count; ++i) {
            bufs[i].dimType = 1;
            uint64_t dpitch_high = *(u16 *)DT_SetGetU16V3(dtSetGetCnt++, 0);
            uint64_t dpitch_low = *(u32 *)DT_SetGetU32V3(dtSetGetCnt++, 50);
            uint64_t dpitch = (dpitch_high << 32) + dpitch_low; // dpitch 在结构体定义中指定为 48 位
            uint64_t layerOffset = *(u16 *)DT_SetGetU16V3(dtSetGetCnt++, 1); // layerOffset 在结构体定义中指定为 16 位
            uint32_t width = *(u32 *)DT_SetGetU32V3(dtSetGetCnt++, 50);
            uint16_t layerNum = *(u16 *)DT_SetGetU16V3(dtSetGetCnt++, 3);
            uint16_t layerCount = *(u16 *)DT_SetGetU16V3(dtSetGetCnt++, 4);
            bufs[i].twoDim = {dpitch, layerOffset, width, layerNum, layerCount};
        }

        for (size_t i = 0; i < keys_count; ++i) {
            if (strlen(keys[i]) > 0 && strlen(keys[i]) <= 256 && bufs[i].addr != 0) {
                batchAllocResp->blobs_.push_back(blob);
                batchAllocResp->blobs_.back()[0].size_ = bufs[i].twoDim.width * bufs[i].twoDim.layerNum;
                batchAllocResp->numBlobs_.push_back(1);
                batchAllocResp->prots_.push_back(0);
                batchAllocResp->priorities_.push_back(0);
                batchAllocResp->leases_.push_back(0);
                batchAllocResp->results_.push_back(0);
                batchUpdateResp->results_.push_back(0);
            }
        }

        bool isAllValid = batchAllocResp->blobs_.size() == keys_count;
        ASSERT_EQ(isAllValid, !bool(mmcc_batch_get(keys, keys_count, bufs, 0, results)));
        if (isAllValid) {
            for (size_t i = 0; i < keys_count; ++i) {
                ASSERT_EQ(strlen(keys[i]) > 0 && strlen(keys[i]) <= 256 && bufs[i].addr != 0 &&
                          bufs[i].twoDim.dpitch >= bufs[i].twoDim.width, !bool(results[i]));
            }
        }

        mmcc_uninit();
        ResetResp();
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcClient, TestMmcClientRemove)
{
    char fuzzName[] = "TestMmcClientRemove";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        auto resp = GetResp();
        resp->ret_ = 0;
        ASSERT_EQ(0, mmcc_init(&mClientConfig));
        char *key = DT_SetGetStringV3(0, 9, 258, (char*)"test_key");
        ASSERT_EQ(strlen(key) > 0 && strlen(key) <= 256, !bool(mmcc_remove(key, 0)));
        mmcc_uninit();
        ResetResp();
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcClient, TestMmcClientBatchRemove)
{
    char fuzzName[] = "TestMmcClientBatchRemove";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        auto batchRemoveResp = GetBatchRemoveResp();
        ASSERT_EQ(0, mmcc_init(&mClientConfig));

        uint32_t keys_count = 3;
        char *key1 = DT_SetGetStringV3(0, 9, 258, (char*)"test_key");
        char *key2 = DT_SetGetStringV3(1, 9, 258, (char*)"test_key");
        char *key3 = DT_SetGetStringV3(2, 9, 258, (char*)"test_key");
        const char *keys[] = {key1, key2, key3};
        int32_t results[] = {-1, -1, -1};

        for (size_t i = 0; i < keys_count; ++i) {
            if (strlen(keys[i]) > 0 && strlen(keys[i]) <= 256) {
                batchRemoveResp->results_.push_back(0);
            }
        }

        ASSERT_EQ(0, mmcc_batch_remove(keys, keys_count, results, 0));
        for (size_t i = 0; i < keys_count; ++i) {
            ASSERT_EQ(strlen(keys[i]) > 0 && strlen(keys[i]) <= 256, !bool(results[i]));
        }
        mmcc_uninit();
        ResetResp();
    }
    DT_FUZZ_END()
}

}
