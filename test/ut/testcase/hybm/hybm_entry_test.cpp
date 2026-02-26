/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
* MemFabric_Hybrid is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include "hybm.h"
#include "hybm_common_include.h"
#include "hybm_logger.h"
#include "hybm_gva.h"
#include "hybm_ptracer.h"
#include "hybm_stream_manager.h"
#include "dl_api.h"
#include "hybm_gva_version.h"

using namespace ock::mf;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

class HybmEntryTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }

    void SetUp() override
    {
        GlobalMockObject::reset();
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
        GlobalMockObject::reset();
    }

protected:
    void MockHybmInitSuccess()
    {
        MOCKER_CPP(HalGvaPrecheck, int32_t(*)()).stubs().will(returnValue(0));
        MOCKER_CPP(DlApi::LoadLibrary, ock::mf::Result(*)(const std::string&, uint32_t)).stubs().will(returnValue(0));
        MOCKER_CPP(ptracer_init, int32_t(*)(ptracer_config_t*)).stubs().will(returnValue(0));
        MOCKER_CPP(hybm_init_hbm_gva, int32_t(*)(uint16_t, uint64_t, uint64_t&)).stubs().will(returnValue(0));
    }

    void MockHybmUninitSuccess()
    {
        MOCKER_CPP(ptracer_uninit, void (*)()).stubs();
        MOCKER_CPP(HybmStreamManager::DestroyAllThreadHybmStream, void (*)()).stubs();
        MOCKER_CPP(DlApi::CleanupLibrary, void (*)()).stubs();
    }
};

TEST_F(HybmEntryTest, hybm_init_success)
{
    MockHybmInitSuccess();

    auto ret = hybm_init(0, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(HybmHasInited());
    EXPECT_EQ(HybmGetInitDeviceId(), 0);
    hybm_uninit();
}

TEST_F(HybmEntryTest, hybm_init_duplicate_same_device)
{
    MockHybmInitSuccess();

    auto ret1 = hybm_init(0, 0);
    EXPECT_EQ(ret1, 0);

    MOCKER_CPP(DlApi::LoadLibrary, ock::mf::Result(*)(const std::string&, uint32_t)).stubs().will(returnValue(BM_OK));

    auto ret2 = hybm_init(0, 0);
    EXPECT_EQ(ret2, 0);
    EXPECT_EQ(HybmGetInitDeviceId(), 0);
    hybm_uninit();
    hybm_uninit();
}

TEST_F(HybmEntryTest, hybm_init_duplicate_different_device)
{
    MockHybmInitSuccess();

    auto ret1 = hybm_init(0, 0);
    EXPECT_EQ(ret1, 0);

    auto ret2 = hybm_init(1, 0);
    EXPECT_EQ(ret2, BM_ERROR);
    hybm_uninit();
}

TEST_F(HybmEntryTest, hybm_init_hal_gva_precheck_failed)
{
    MOCKER_CPP(HalGvaPrecheck, int32_t(*)()).stubs().will(returnValue(-1));

    auto ret = hybm_init(0, 0);
    EXPECT_EQ(ret, BM_ERROR);
    EXPECT_FALSE(HybmHasInited());
}

TEST_F(HybmEntryTest, hybm_init_load_library_failed)
{
    MOCKER_CPP(HalGvaPrecheck, int32_t(*)()).stubs().will(returnValue(0));
    MOCKER_CPP(DlApi::LoadLibrary, ock::mf::Result(*)(const std::string&, uint32_t)).stubs().will(returnValue(-1));
    MOCKER_CPP(ptracer_init, int32_t(*)(ptracer_config_t*)).stubs().will(returnValue(0));
    MOCKER_CPP(ptracer_uninit, void (*)()).stubs();
    MOCKER_CPP(DlApi::CleanupLibrary, void (*)()).stubs();

    auto ret = hybm_init(0, 0);
    EXPECT_EQ(ret, BM_ERROR);
    EXPECT_FALSE(HybmHasInited());
}

TEST_F(HybmEntryTest, hybm_init_hbm_gva_failed)
{
    MOCKER_CPP(HalGvaPrecheck, int32_t(*)()).stubs().will(returnValue(0));
    MOCKER_CPP(DlApi::LoadLibrary, ock::mf::Result(*)(const std::string&, uint32_t)).stubs().will(returnValue(0));
    MOCKER_CPP(ptracer_init, int32_t(*)(ptracer_config_t*)).stubs().will(returnValue(0));
    MOCKER_CPP(hybm_init_hbm_gva, int32_t(*)(uint16_t, uint64_t, uint64_t&)).stubs().will(returnValue(-1));
    MOCKER_CPP(ptracer_uninit, void (*)()).stubs();
    MOCKER_CPP(DlApi::CleanupLibrary, void (*)()).stubs();

    auto ret = hybm_init(0, 0);
    EXPECT_EQ(ret, BM_ERROR);
    EXPECT_FALSE(HybmHasInited());
}

TEST_F(HybmEntryTest, hybm_uninit_success)
{
    MockHybmInitSuccess();
    MockHybmUninitSuccess();

    auto ret = hybm_init(0, 0);
    EXPECT_EQ(ret, 0);

    hybm_uninit();
    EXPECT_FALSE(HybmHasInited());
}

TEST_F(HybmEntryTest, hybm_uninit_duplicate_call)
{
    MockHybmInitSuccess();
    MockHybmUninitSuccess();

    auto ret = hybm_init(0, 0);
    EXPECT_EQ(ret, 0);

    hybm_uninit();
    EXPECT_FALSE(HybmHasInited());

    hybm_uninit();
    EXPECT_FALSE(HybmHasInited());
}

TEST_F(HybmEntryTest, hybm_uninit_incremental)
{
    MockHybmInitSuccess();
    MockHybmUninitSuccess();

    auto ret1 = hybm_init(0, 0);
    EXPECT_EQ(ret1, 0);

    MOCKER_CPP(DlApi::LoadLibrary, ock::mf::Result(*)(const std::string&, uint32_t)).stubs().will(returnValue(0));

    auto ret2 = hybm_init(0, 0);
    EXPECT_EQ(ret2, 0);

    hybm_uninit();
    EXPECT_TRUE(HybmHasInited());
    hybm_uninit();
    EXPECT_FALSE(HybmHasInited());
}

void TestLogger(int level, const char* msg)
{
    (void)level;
    (void)msg;
}

TEST_F(HybmEntryTest, hybm_set_extern_logger_success)
{
    hybm_set_extern_logger(TestLogger);
}

TEST_F(HybmEntryTest, hybm_set_extern_logger_nullptr)
{
    hybm_set_extern_logger(nullptr);
}

TEST_F(HybmEntryTest, hybm_set_log_level_valid_levels)
{
    auto ret0 = hybm_set_log_level(0);
    EXPECT_EQ(ret0, 0);

    auto ret1 = hybm_set_log_level(1);
    EXPECT_EQ(ret1, 0);

    auto ret2 = hybm_set_log_level(2);
    EXPECT_EQ(ret2, 0);

    auto ret3 = hybm_set_log_level(3);
    EXPECT_EQ(ret3, 0);

    auto ret4 = hybm_set_log_level(4);
    EXPECT_EQ(ret4, 0);
}

TEST_F(HybmEntryTest, hybm_set_log_level_invalid_levels)
{
    auto ret_neg = hybm_set_log_level(-1);
    EXPECT_EQ(ret_neg, -1);

    auto ret_large = hybm_set_log_level(5);
    EXPECT_EQ(ret_large, -1);
}

TEST_F(HybmEntryTest, hybm_get_error_string)
{
    auto str1 = hybm_get_error_string(0);
    EXPECT_NE(str1, nullptr);
    EXPECT_NE(std::string(str1).find("error(0)"), std::string::npos);
}

TEST_F(HybmEntryTest, hybm_init_thread_safety)
{
    MockHybmInitSuccess();
    MOCKER_CPP(DlApi::LoadLibrary, ock::mf::Result(*)(const std::string&, uint32_t)).stubs().will(returnValue(0));

    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < 4; i++) {
        threads.emplace_back([&successCount]() {
            auto ret = hybm_init(0, 0);
            if (ret == 0) {
                successCount++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successCount.load(), 4);

    for (int i = 0; i < 5; i++) {
        hybm_uninit();
    }

    EXPECT_FALSE(HybmHasInited());
}

TEST_F(HybmEntryTest, hybm_uninit_thread_safety)
{
    MockHybmInitSuccess();
    MockHybmUninitSuccess();

    auto ret = hybm_init(0, 0);
    EXPECT_EQ(ret, 0);

    std::vector<std::thread> threads;

    for (int i = 0; i < 4; i++) {
        threads.emplace_back([]() { hybm_uninit(); });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(HybmHasInited());
}

TEST_F(HybmEntryTest, hybm_init_max_device_id)
{
    MockHybmInitSuccess();

    auto ret = hybm_init(UINT16_MAX, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(HybmGetInitDeviceId(), UINT16_MAX);
    hybm_uninit();
}

TEST_F(HybmEntryTest, hybm_init_flags)
{
    MockHybmInitSuccess();

    auto ret1 = hybm_init(0, 0);
    EXPECT_EQ(ret1, 0);
    hybm_uninit();

    auto ret2 = hybm_init(1, 0xFFFFFFFF);
    EXPECT_EQ(ret2, 0);
    hybm_uninit();
}

TEST_F(HybmEntryTest, hybm_init_cleanup_on_failure)
{
    MOCKER_CPP(HalGvaPrecheck, int32_t(*)()).stubs().will(returnValue(0));
    MOCKER_CPP(DlApi::LoadLibrary, ock::mf::Result(*)(const std::string&, uint32_t)).stubs().will(returnValue(0));
    MOCKER_CPP(ptracer_init, int32_t(*)(ptracer_config_t*)).stubs().will(returnValue(0));
    MOCKER_CPP(hybm_init_hbm_gva, int32_t(*)(uint16_t, uint64_t, uint64_t&)).stubs().will(returnValue(-1));
    MOCKER_CPP(ptracer_uninit, void (*)()).stubs();
    MOCKER_CPP(DlApi::CleanupLibrary, void (*)()).stubs();

    auto ret = hybm_init(0, 0);
    EXPECT_EQ(ret, BM_ERROR);
    EXPECT_FALSE(HybmHasInited());
}

TEST_F(HybmEntryTest, hybm_init_ptracer_failure)
{
    MOCKER_CPP(HalGvaPrecheck, int32_t(*)()).stubs().will(returnValue(0));
    MOCKER_CPP(DlApi::LoadLibrary, ock::mf::Result(*)(const std::string&, uint32_t)).stubs().will(returnValue(0));
    MOCKER_CPP(ptracer_init, int32_t(*)(ptracer_config_t*)).stubs().will(returnValue(-1));
    MOCKER_CPP(hybm_init_hbm_gva, int32_t(*)(uint16_t, uint64_t, uint64_t&)).stubs().will(returnValue(0));

    auto ret = hybm_init(0, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(HybmHasInited());
    hybm_uninit();
}
