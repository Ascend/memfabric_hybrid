/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include "hybm.h"
#include "hybm_big_mem.h"
#include "hybm_data_op.h"
#include "hybm_types.h"
#include "dl_acl_api.h"
#include "hybm_entity_factory.h"
#include "secodefuzz/secodeFuzz.h"
#include "dt_fuzz.h"

using namespace ock::mf;

namespace {
const hybm_options g_options = {HYBM_TYPE_HBM_AI_CORE_INITIATE,
                                HYBM_DOP_TYPE_MTE,
                                HYBM_SCOPE_CROSS_NODE,
                                HYBM_RANK_TYPE_STATIC,
                                8,
                                0,
                                0,
                                1024 * 1024 * 1024,
                                0,
                                true,
                                HYBM_ROLE_PEER, "tcp://127.0.0.1:10002"};
const uint64_t g_allocSize = 2 * 1024 * 1024;
}  // namespace

class TestHybm : public testing::Test {
public:
    void SetUp();
    void TearDown();
};

void TestHybm::SetUp()
{
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(DT_RUNNING_TIME);
}

void TestHybm::TearDown()
{
    GlobalMockObject::reset();
}

TEST_F(TestHybm, hybm_init_set_log)
{
    char fuzzName[] = "test_hybm_init_set_log";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        uint16_t deviceId = 0;
        uint64_t flags = 0;
        int32_t logLevel = *(int32_t *)DT_SetGetS32(&g_Element[0], 0);

        int32_t result = hybm_init(deviceId, flags);
        EXPECT_EQ(result, 0);

        auto ret = hybm_set_log_level(logLevel);
        if (logLevel >= 0 && logLevel < 4) {
            EXPECT_EQ(ret, 0);
        } else {
            EXPECT_NE(ret, 0);
        }

        EXPECT_NE(hybm_get_error_string(0), nullptr);

        hybm_uninit();
    }
    DT_FUZZ_END()
}

TEST_F(TestHybm, hybm_create_entity)
{
    char fuzzName[] = "test_hybm_create_entity";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        EXPECT_EQ(hybm_init(0, 0), 0);

        uint16_t id = 1;
        uint32_t flags = 0;

        auto entity = hybm_create_entity(id, &g_options, flags);
        EXPECT_NE(entity, nullptr);

        void *reservedMem = nullptr;
        EXPECT_EQ(hybm_reserve_mem_space(entity, 0, &reservedMem), BM_OK);

        hybm_mem_slice_t slice = hybm_alloc_local_memory(entity, HYBM_MEM_TYPE_DEVICE, g_allocSize, 0);
        EXPECT_NE(slice, nullptr);

        hybm_exchange_info info = {};
        void* addresses[1] = { nullptr };
        EXPECT_EQ(hybm_export(entity, slice, 0, &info), BM_OK);
        EXPECT_EQ(hybm_import(entity, &info, 1, addresses, 0), BM_OK);
        EXPECT_EQ(hybm_mmap(entity, 0), BM_OK);

        EXPECT_EQ(hybm_set_extra_context(entity, &g_options, sizeof(g_options)), BM_OK);

        hybm_unmap(entity, 0);
        EXPECT_EQ(hybm_remove_imported(entity, 0, 0), BM_OK);
        EXPECT_EQ(hybm_free_local_memory(entity, slice, g_allocSize, 0), BM_OK);
        EXPECT_EQ(hybm_unreserve_mem_space(entity, 0, reservedMem), BM_OK);

        hybm_destroy_entity(entity, 0);

        hybm_uninit();
    }
    DT_FUZZ_END()
}