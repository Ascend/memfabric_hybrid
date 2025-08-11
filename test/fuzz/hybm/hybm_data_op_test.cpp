/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
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
const uint64_t g_rankSize = 8;
const uint64_t g_localMemSize = 1024 * 1024 * 1024;
const hybm_options g_options = {HYBM_TYPE_HBM_HOST_INITIATE,
                                HYBM_DOP_TYPE_MTE,
                                HYBM_SCOPE_CROSS_NODE,
                                HYBM_RANK_TYPE_STATIC,
                                g_rankSize,
                                0,
                                0,
                                g_localMemSize,
                                0,
                                true};
const uint64_t g_allocSize = 2 * 1024 * 1024;
hybm_entity_t g_entity = nullptr;
hybm_mem_slice_t g_slice = nullptr;
void *g_reservedMem = nullptr;
}  // namespace

class TestHybmOp : public testing::Test {
public:
    void SetUp() override;
    void TearDown() override;
};

void TestHybmOp::SetUp()
{
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(DT_RUNNING_TIME);
    uint16_t id = 1;
    uint32_t flags = 0;

    EXPECT_EQ(hybm_init(0, 0), BM_OK);

    g_entity = hybm_create_entity(id, &g_options, flags);
    EXPECT_NE(g_entity, nullptr);

    EXPECT_EQ(hybm_reserve_mem_space(g_entity, 0, &g_reservedMem), BM_OK);

    g_slice = hybm_alloc_local_memory(g_entity, HYBM_MEM_TYPE_DEVICE, g_allocSize, 0);
    EXPECT_NE(g_slice, nullptr);

    hybm_exchange_info info = {};
    void* addresses[1] = { nullptr };
    EXPECT_EQ(hybm_export(g_entity, g_slice, 0, &info), BM_OK);
    EXPECT_EQ(hybm_import(g_entity, &info, 1, addresses, 0), BM_OK);
    EXPECT_EQ(hybm_mmap(g_entity, 0), BM_OK);
}

void TestHybmOp::TearDown()
{
    hybm_unmap(g_entity, 0);
    EXPECT_EQ(hybm_remove_imported(g_entity, 0, 0), BM_OK);
    EXPECT_EQ(hybm_free_local_memory(g_entity, g_slice, g_allocSize, 0), BM_OK);
    EXPECT_EQ(hybm_unreserve_mem_space(g_entity, 0, g_reservedMem), BM_OK);

    hybm_destroy_entity(g_entity, 0);
    hybm_uninit();
    GlobalMockObject::reset();
}

TEST_F(TestHybmOp, hybm_data_copy)
{
    char fuzzName[] = "test_hybm_data_copy";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        hybm_copy_params copyParams = {g_reservedMem, g_reservedMem, g_allocSize};
        auto ret = hybm_data_copy(g_entity, &copyParams,
                                  HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
        EXPECT_EQ(ret, BM_OK);

        ret = hybm_data_copy(g_entity, &copyParams, HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE,
                             nullptr, 0);
        EXPECT_EQ(ret, BM_OK);

        ret = hybm_data_copy(g_entity, &copyParams, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE,
                             nullptr, 0);
        EXPECT_EQ(ret, BM_OK);

        ret = hybm_data_copy(g_entity, &copyParams, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST,
                             nullptr, 0);
        EXPECT_EQ(ret, BM_OK);

        ret = hybm_data_copy(g_entity, &copyParams, HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE,
                             nullptr, 0);
        EXPECT_EQ(ret, BM_OK);

        // copy_2d
        hybm_copy_2d_params copy2DParams = {g_reservedMem, g_allocSize, g_reservedMem, g_allocSize, g_allocSize, 1};
        ret = hybm_data_copy_2d(g_entity, &copy2DParams,
                                HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
        EXPECT_EQ(ret, BM_OK);

        ret = hybm_data_copy_2d(g_entity, &copy2DParams,
                                HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, nullptr, 0);
        EXPECT_EQ(ret, BM_OK);

        ret = hybm_data_copy_2d(g_entity, &copy2DParams,
                                HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, nullptr, 0);
        EXPECT_EQ(ret, BM_OK);

        ret = hybm_data_copy_2d(g_entity, &copy2DParams,
                                HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, nullptr, 0);
        EXPECT_EQ(ret, BM_OK);

        ret = hybm_data_copy_2d(g_entity, &copy2DParams,
                                HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
        EXPECT_EQ(ret, BM_OK);
    }
    DT_FUZZ_END()
}