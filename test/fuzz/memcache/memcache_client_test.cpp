/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <secodefuzz/secodeFuzz.h>
#include "dt_fuzz.h"
#include "mmc_client.h"
#include "mmc_service.h"
#include "mmc_def.h"
#include "mmc_bm_proxy.h"
#include "mmc_local_service_default.h"
#include "mmc_meta_service_default.h"
#include "memcache_stubs.h"
#include "mmc_config_const.h"
#include "memcache_fuzz_common.h"

using namespace ock::mmc;

namespace {
class TestMmcFuzzClientInit : public TestMmcFuzzBase {
public:
    static void SetUpTestSuite();
    static void TearDownTestSuite();
    void SetUp() override;
    void TearDown() override;
};

void TestMmcFuzzClientInit::SetUpTestSuite()
{
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(DT_RUNNING_TIME);
    MockSmemAll();
}

void TestMmcFuzzClientInit::TearDownTestSuite()
{
    GlobalMockObject::verify();
}

void TestMmcFuzzClientInit::SetUp()
{
}

void TestMmcFuzzClientInit::TearDown()
{
}

TEST_F(TestMmcFuzzClientInit, TestMmcClientInit)
{
    char fuzzName[] = "TestMmcClientInit";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        ASSERT_NE(metaService = mmcs_meta_service_start(&metaServiceConfig), nullptr);
        ASSERT_NE(localService = mmcs_local_service_start(&localServiceConfig), nullptr);

        mmc_client_config_t tempConfig = clientConfig;
        tempConfig.logLevel = *(s32 *)DT_SetGetS32(&g_Element[0], 0);
        tempConfig.rankId = *(u32 *)DT_SetGetU32(&g_Element[1], 0);
        tempConfig.timeOut = *(u32 *)DT_SetGetU32(&g_Element[2], 0);

        mmc_data_info infoQuery{.valid = false};
        mmc_buffer buf{.addr = 1, .type = 0, .offset = 0, .len = 0};
        mmc_put_options opt{0, NATIVE_AFFINITY};

        ASSERT_NE(mmcc_put("test", &buf, opt, 0), 0);
        ASSERT_NE(mmcc_get("test", &buf, 0), 0);
        ASSERT_NE(mmcc_exist("test", 0), 0);
        ASSERT_NE(mmcc_query("test", &infoQuery, 0), 0);
        ASSERT_NE(mmcc_remove("test", 0), 0);

        bool isValid = tempConfig.logLevel <= 3 && tempConfig.logLevel >= 0;
        ASSERT_EQ(mmcc_init(&tempConfig) == 0, isValid);

        ASSERT_EQ(mmcc_put("test", &buf, opt, 0) == 0, isValid);
        ASSERT_EQ(mmcc_get("test", &buf, 0) == 0, isValid);
        ASSERT_EQ(mmcc_exist("test", 0) == 0, isValid);
        ASSERT_EQ(mmcc_query("test", &infoQuery, 0) == 0, isValid);
        ASSERT_EQ(mmcc_remove("test", 0) == 0, isValid);

        mmcs_local_service_stop(localService);
        mmcc_uninit();
        mmcs_meta_service_stop(metaService);
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcFuzzClientInit, TestMmcClientInitFailureNoLocal)
{
    char fuzzName[] = "TestMmcClientInitFailureNoLocal";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        ASSERT_NE(metaService = mmcs_meta_service_start(&metaServiceConfig), nullptr);

        mmc_client_config_t tempConfig = clientConfig;
        tempConfig.logLevel = *(s32 *)DT_SetGetS32(&g_Element[0], 0);
        tempConfig.rankId = *(u32 *)DT_SetGetU32(&g_Element[1], 0);
        tempConfig.timeOut = *(u32 *)DT_SetGetU32(&g_Element[2], 0);

        ASSERT_EQ(mmcc_init(&tempConfig) == 0, tempConfig.logLevel <= 3 && tempConfig.logLevel >= 0);

        mmc_data_info infoQuery{.valid = true};
        mmc_buffer buf{.addr = 1, .type = 0, .offset = 0, .len = 0};
        mmc_put_options opt{0, NATIVE_AFFINITY};
        ASSERT_NE(mmcc_put("test", &buf, opt, 0), 0);
        ASSERT_NE(mmcc_get("test", &buf, 0), 0);
        ASSERT_NE(mmcc_exist("test", 0), 0);
        ASSERT_EQ(mmcc_query("test", &infoQuery, 0) == 0, tempConfig.logLevel <= 3 && tempConfig.logLevel >= 0);
        ASSERT_EQ(infoQuery.valid, false || !(tempConfig.logLevel <= 3 && tempConfig.logLevel >= 0));
        ASSERT_NE(mmcc_remove("test", 0), 0);

        mmcc_uninit();
        mmcs_meta_service_stop(metaService);
    }
    DT_FUZZ_END()
}


class TestMmcFuzzClientFeature : public TestMmcFuzzBase {
public:
    static void SetUpTestSuite();
    static void TearDownTestSuite();
    void SetUp() override;
    void TearDown() override;
};

void TestMmcFuzzClientFeature::SetUpTestSuite()
{
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(DT_RUNNING_TIME);
    MockSmemAll();
}

void TestMmcFuzzClientFeature::TearDownTestSuite()
{
    GlobalMockObject::verify();
}

void TestMmcFuzzClientFeature::SetUp()
{
    ASSERT_NE(metaService = mmcs_meta_service_start(&metaServiceConfig), nullptr);
    ASSERT_NE(localService = mmcs_local_service_start(&localServiceConfig), nullptr);
    ASSERT_EQ(mmcc_init(&clientConfig), 0);
}

void TestMmcFuzzClientFeature::TearDown()
{
    mmcs_local_service_stop(localService);
    mmcc_uninit();
    mmcs_meta_service_stop(metaService);
    localService = nullptr;
    metaService = nullptr;
}

TEST_F(TestMmcFuzzClientFeature, TestMmcClientExist)
{
    char fuzzName[] = "TestMmcClientExist";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        char *key = DT_SetGetString(&g_Element[0], 9, 258, (char*)"test_key");
        ASSERT_NE(mmcc_exist(key, 0), 0);
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcFuzzClientFeature, TestMmcClientBatchExist)
{
    char fuzzName[] = "TestMmcClientBatchExist";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        uint32_t keys_count = 3;
        char *key1 = DT_SetGetString(&g_Element[0], 11, 258, (char*)"test_key_1");
        char *key2 = DT_SetGetString(&g_Element[1], 11, 258, (char*)"Test_key_2");
        char *key3 = DT_SetGetString(&g_Element[2], 11, 258, (char*)"test_Key_3");
        const char *keys[] = {key1, key2, key3};
        int32_t results[] = {1, 1, 1};
        int validKeysCnt = 0;
        for (size_t i = 0; i < keys_count; ++i) {
            validKeysCnt += strlen(keys[i]) > 0 && strlen(keys[i]) <= 256 ? 1 : 0;
        }

        ASSERT_EQ(mmcc_batch_exist(keys, keys_count, results, 0) == 0, validKeysCnt > 0);
        for (size_t i = 0; i < keys_count; ++i) {
            ASSERT_EQ(results[i] == 1, validKeysCnt > 0 ? false : true);  // 1 - init value
        }
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcFuzzClientFeature, TestMmcClientQuery)
{
    char fuzzName[] = "TestMmcClientQuery";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        char *key = DT_SetGetString(&g_Element[0], 9, 258, (char*)"test_key");
        mmc_data_info info{.valid = true};
        bool isValid = strlen(key) > 0 && strlen(key) <= 256;
        ASSERT_EQ(mmcc_query(key, &info, 0) == 0, isValid);
        ASSERT_EQ(info.valid == false, isValid);
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcFuzzClientFeature, TestMmcClientBatchQuery)
{
    char fuzzName[] = "TestMmcClientBatchQuery";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        uint32_t keys_count = 3;
        char *key1 = DT_SetGetString(&g_Element[0], 11, 258, (char*)"test_key_1");
        char *key2 = DT_SetGetString(&g_Element[1], 11, 258, (char*)"Test_key_2");
        char *key3 = DT_SetGetString(&g_Element[2], 11, 258, (char*)"test_Key_3");
        const char *keys[] = {key1, key2, key3};
        mmc_data_info infos[] = {
            mmc_data_info{.valid = true}, mmc_data_info{.valid = true}, mmc_data_info{.valid = true}
        };
        int validKeysCnt = 0;
        for (size_t i = 0; i < keys_count; ++i) {
            validKeysCnt += strlen(keys[i]) > 0 && strlen(keys[i]) <= 256 ? 1 : 0;
        }

        ASSERT_EQ(mmcc_batch_query(keys, keys_count, infos, 0) == 0, validKeysCnt > 0);
        for (size_t i = 0; i < keys_count; ++i) {
            ASSERT_EQ(infos[i].valid == false, validKeysCnt > 0 ? true : false);
        }
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcFuzzClientFeature, TestMmcClientPutOneDim)
{
    char fuzzName[] = "TestMmcClientPutOneDim";
    int policyEnum[] = {0};
    int typeEnum[] = {0, 1};
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        char *key = DT_SetGetString(&g_Element[0], 9, 258, (char*)"test_key");

        mmc_put_options opt;
        opt.mediaType = *(u16 *)DT_SetGetU16(&g_Element[1], 0);
        opt.policy = static_cast<affinity_policy>(*(s32 *)DT_SetGetNumberEnum(&g_Element[2], 0, policyEnum, 1));

        mmc_buffer buf;
        buf.addr = *(u64 *)DT_SetGetU64(&g_Element[3], 50);
        buf.type = *(u32 *)DT_SetGetNumberEnum(&g_Element[4], 0, typeEnum, 2);

        // OneDim Case
        uint64_t offset = *(u64 *)DT_SetGetU64(&g_Element[5], 3);
        uint64_t len = *(u64 *)DT_SetGetU64(&g_Element[6], 50);
        buf.offset = offset;
        buf.len = len;

        bool keyValid = strlen(key) > 0 && strlen(key) <= 256;
        bool isValid = keyValid && buf.addr != 0 && buf.addr != 0 && len <= MAX_BUF_SIZE;
        mmc_data_info infoQuery{.valid = false};
        mmc_buffer bufGet{.addr = 1, .type = buf.type, .offset = buf.offset, .len = buf.len};

        ASSERT_NE(mmcc_exist(key, 0), 0);
        ASSERT_EQ(mmcc_query(key, &infoQuery, 0) == 0, keyValid);
        ASSERT_EQ(infoQuery.valid, false);
        ASSERT_NE(mmcc_get(key, &bufGet, 0), 0);
        ASSERT_NE(mmcc_remove(key, 0), 0);

        ASSERT_EQ(mmcc_put(key, &buf, opt, 0) == 0, isValid);

        ASSERT_EQ(mmcc_exist(key, 0) == 0, isValid);
        ASSERT_EQ(mmcc_query(key, &infoQuery, 0) == 0, keyValid);
        ASSERT_EQ(infoQuery.valid, isValid);
        ASSERT_EQ(mmcc_get(key, &bufGet, 0) == 0, isValid);
        ASSERT_EQ(mmcc_remove(key, 0) == 0, isValid);
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcFuzzClientFeature, TestMmcClientBatchPutOneDim)
{
    char fuzzName[] = "TestMmcClientBatchPutOneDim";
    int policyEnum[] = {0};
    int typeEnum[] = {0, 1};
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        uint32_t keys_count = 3;
        char *key1 = DT_SetGetString(&g_Element[0], 11, 258, (char*)"test_key_1");
        char *key2 = DT_SetGetString(&g_Element[1], 11, 258, (char*)"Test_key_2");
        char *key3 = DT_SetGetString(&g_Element[2], 11, 258, (char*)"test_Key_3");
        const char *keys[] = {key1, key2, key3};

        mmc_put_options opt;
        opt.mediaType = *(u16 *)DT_SetGetU16(&g_Element[3], 0);
        opt.policy = static_cast<affinity_policy>(*(s32 *)DT_SetGetNumberEnum(&g_Element[4], 0, policyEnum, 1));

        int dtSetGetCnt = 5;
        bool isAllValid = true;
        mmc_buffer bufs[keys_count];
        for (size_t i = 0; i < keys_count; ++i) {
            bufs[i].addr = *(u64 *)DT_SetGetU64(&g_Element[dtSetGetCnt++], 50);
            bufs[i].type = *(u32 *)DT_SetGetNumberEnum(&g_Element[dtSetGetCnt++], 0, typeEnum, 2);
            isAllValid &= bufs[i].addr != 0;
            isAllValid &= strlen(keys[i]) > 0 && strlen(keys[i]) <= 256;
        }

        int32_t results[] = {-1, -1, -1};

        // OneDim Case
        for (size_t i = 0; i < keys_count; ++i) {
            uint64_t offset = *(u64 *)DT_SetGetU64(&g_Element[dtSetGetCnt++], 3);
            uint64_t len = *(u64 *)DT_SetGetU64(&g_Element[dtSetGetCnt++], 50);
            bufs[i].offset = offset;
            bufs[i].len = len;
        }

        ASSERT_EQ(mmcc_batch_put(keys, keys_count, bufs, opt, 0, results) == 0, isAllValid);  // todo results validate
        int32_t resultsExist[3] = {1, 1, 1};
        int32_t resultsGet[3] = {1, 1, 1};
        int32_t resultsRemove[3] = {1, 1, 1};
        mmc_data_info infoQuery[3] {};
        mmc_buffer bufsGet[3] {{.addr = 1}, {.addr = 1}, {.addr = 1}};
        mmcc_batch_exist(keys, keys_count, resultsExist, 0);
        mmcc_batch_query(keys, keys_count, infoQuery, 0);
        mmcc_batch_get(keys, keys_count, bufsGet, 0, resultsGet);
        mmcc_batch_remove(keys, keys_count, resultsRemove, 0);
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcFuzzClientFeature, TestMmcClientGetOneDim)
{
    char fuzzName[] = "TestMmcClientGetOneDim";
    int typeEnum[] = {0, 1};
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        char *key = DT_SetGetString(&g_Element[0], 9, 258, (char*)"test_key");

        mmc_buffer buf;
        buf.addr = *(u64 *)DT_SetGetU64(&g_Element[1], 50);
        buf.type = *(u32 *)DT_SetGetNumberEnum(&g_Element[2], 0, typeEnum, 2);

        // OneDim Case
        uint64_t offset = *(u64 *)DT_SetGetU64(&g_Element[3], 3);
        uint64_t len = *(u64 *)DT_SetGetU64(&g_Element[4], 50);
        buf.offset = offset;
        buf.len = len;

        ASSERT_NE(mmcc_get(key, &buf, 0), 0);
        ASSERT_NE(mmcc_get(key, nullptr, 0), 0);
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcFuzzClientFeature, TestMmcClientBatchGetOneDim)
{
    char fuzzName[] = "TestMmcClientBatchGetOneDim";
    int typeEnum[] = {0, 1};
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        uint32_t keys_count = 3;
        char *key1 = DT_SetGetString(&g_Element[0], 11, 258, (char*)"test_key_1");
        char *key2 = DT_SetGetString(&g_Element[1], 11, 258, (char*)"Test_key_2");
        char *key3 = DT_SetGetString(&g_Element[2], 11, 258, (char*)"test_Key_3");
        const char *keys[] = {key1, key2, key3};

        int dtSetGetCnt = 3;
        bool isAllValid = true;
        mmc_buffer bufs[keys_count];
        for (size_t i = 0; i < keys_count; ++i) {
            bufs[i].addr = *(u64 *)DT_SetGetU64(&g_Element[dtSetGetCnt++], 50);
            bufs[i].type = *(u32 *)DT_SetGetNumberEnum(&g_Element[dtSetGetCnt++], 0, typeEnum, 2);
            isAllValid &= strlen(keys[i]) > 0 && strlen(keys[i]) <= 256;
        }

        int32_t results[] = {-1, -1, -1};

        // OneDim Case
        for (size_t i = 0; i < keys_count; ++i) {
            uint64_t offset = *(u64 *)DT_SetGetU64(&g_Element[dtSetGetCnt++], 3);
            uint64_t len = *(u64 *)DT_SetGetU64(&g_Element[dtSetGetCnt++], 50);
            bufs[i].offset = offset;
            bufs[i].len = len;
        }

        ASSERT_EQ(mmcc_batch_get(keys, keys_count, bufs, 0, results) == 0, isAllValid);  // todo results validate
        ASSERT_NE(mmcc_batch_get(keys, keys_count, nullptr, 0, results), 0);
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcFuzzClientFeature, TestMmcClientRemove)
{
    char fuzzName[] = "TestMmcClientRemove";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        char *key = DT_SetGetString(&g_Element[0], 9, 258, (char*)"test_key");
        ASSERT_NE(mmcc_remove(key, 0), 0);
    }
    DT_FUZZ_END()
}

TEST_F(TestMmcFuzzClientFeature, TestMmcClientBatchRemove)
{
    char fuzzName[] = "TestMmcClientBatchRemove";
    DT_FUZZ_START(0, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        uint32_t keys_count = 3;
        char *key1 = DT_SetGetString(&g_Element[0], 11, 258, (char*)"test_key_1");
        char *key2 = DT_SetGetString(&g_Element[1], 11, 258, (char*)"Test_key_2");
        char *key3 = DT_SetGetString(&g_Element[2], 11, 258, (char*)"test_Key_3");
        const char *keys[] = {key1, key2, key3};
        int32_t results[] = {1, 1, 1};
        int validKeysCnt = 0;
        for (size_t i = 0; i < keys_count; ++i) {
            validKeysCnt += strlen(keys[i]) > 0 && strlen(keys[i]) <= 256 ? 1 : 0;
        }

        ASSERT_EQ(0, mmcc_batch_remove(keys, keys_count, results, validKeysCnt > 0));
        for (size_t i = 0; i < keys_count; ++i) {
            ASSERT_NE(results[i], 1);  // 1 - init value
        }
    }
    DT_FUZZ_END()
}

}
