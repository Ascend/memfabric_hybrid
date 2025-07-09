/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "hybm.h"
#include "hybm_big_mem.h"
#include "hybm_data_op.h"
#include "hybm_types.h"
#include "dl_acl_api.h"
#include "hybm_entity_factory.h"

using namespace ock::mf;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
const uint64_t g_rankSize = 8;
const uint64_t g_localMemSize = 1024 * 1024 * 1024;
const hybm_options g_options = {HYBM_TYPE_HBM_AI_CORE_INITIATE, HYBM_DOP_TYPE_MTE, HYBM_SCOPE_CROSS_NODE,
                                HYBM_RANK_TYPE_STATIC, g_rankSize, 0, g_localMemSize, 0};
const uint64_t g_allocSize = 2 * 1024 * 1024;
hybm_entity_t g_entity = nullptr;
hybm_mem_slice_t g_slice = nullptr;
void *g_reservedMem = nullptr;

class HybmDataOpEntryTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        uint16_t id = 1;
        uint32_t flags = 0;

        EXPECT_EQ(hybm_init(0, 0), BM_OK);

        g_entity = hybm_create_entity(id, &g_options, flags);
        EXPECT_NE(g_entity, nullptr);

        EXPECT_EQ(hybm_reserve_mem_space(g_entity, 0, &g_reservedMem), BM_OK);

        g_slice = hybm_alloc_local_memory(g_entity, HYBM_MEM_TYPE_DEVICE, g_allocSize, 0);
        EXPECT_NE(g_slice, nullptr);

        hybm_exchange_info info = {};
        EXPECT_EQ(hybm_export(g_entity, g_slice, 0, &info), BM_OK);
        EXPECT_EQ(hybm_import(g_entity, &info, 1, 0), BM_OK);
        EXPECT_EQ(hybm_mmap(g_entity, 0), BM_OK);
    }

    static void TearDownTestSuite()
    {
        hybm_unmap(g_entity, 0);
        EXPECT_EQ(hybm_remove_imported(g_entity, 0, 0), BM_OK);
        EXPECT_EQ(hybm_free_local_memory(g_entity, g_slice, g_allocSize, 0), BM_OK);
        EXPECT_EQ(hybm_unreserve_mem_space(g_entity, 0, g_reservedMem), BM_OK);

        hybm_destroy_entity(g_entity, 0);
        hybm_uninit();
    }

    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(HybmDataOpEntryTest, hybm_data_copy_ShouldReturnOK)
{
    auto ret = hybm_data_copy(g_entity, g_reservedMem, g_reservedMem, g_allocSize,
                         HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmDataOpEntryTest, hybm_data_copy_ShouldReturnInvalidParam)
{
    auto ret = hybm_data_copy(nullptr, nullptr, nullptr, 0, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy(g_entity, nullptr, nullptr, 0, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy(g_entity, g_reservedMem, nullptr, 0, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy(g_entity, g_reservedMem, g_reservedMem, 0, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy(g_entity, g_reservedMem, g_reservedMem, g_allocSize, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    auto startAddr = g_reservedMem;
    auto endAddr = g_reservedMem + g_localMemSize * g_rankSize;
    ret = hybm_data_copy(g_entity, startAddr - 1, startAddr - 1, g_allocSize,
                         HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy(g_entity, endAddr, endAddr, g_allocSize, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy(g_entity, endAddr, endAddr, g_allocSize, HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy(g_entity, startAddr - 1, startAddr - 1, g_allocSize,
                         HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy(g_entity, endAddr, endAddr, g_allocSize, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy(g_entity, endAddr, endAddr, g_allocSize, HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

TEST_F(HybmDataOpEntryTest, hybm_data_copy_2d_ShouldReturnOK)
{
    auto ret = hybm_data_copy_2d(g_entity, g_reservedMem, g_allocSize, g_reservedMem, g_allocSize, g_allocSize, 1,
                                HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmDataOpEntryTest, hybm_data_copy_2d_ShouldReturnInvalidParam)
{
    auto startAddr = g_reservedMem;
    auto endAddr = g_reservedMem + g_localMemSize * g_rankSize;

    auto ret = hybm_data_copy_2d(nullptr, nullptr, 0, nullptr, 0, 0, 0, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy_2d(g_entity, nullptr, 0, nullptr, 0, 0, 0, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy_2d(g_entity, startAddr, 0, nullptr, 0, 0, 0, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy_2d(g_entity, startAddr, g_allocSize, nullptr, 0, 0, 0,
                            HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy_2d(g_entity, startAddr, g_allocSize, startAddr, 0, 0, 0,
                            HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy_2d(g_entity, startAddr, g_allocSize, startAddr, g_allocSize, 0, 0,
                            HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy_2d(g_entity, startAddr, g_allocSize, startAddr, g_allocSize, g_allocSize, 0,
                            HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy_2d(g_entity, startAddr, g_allocSize, startAddr, g_allocSize, g_allocSize, 1,
                            HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    ret = hybm_data_copy_2d(g_entity, startAddr - 1, g_allocSize, startAddr - 1, g_allocSize, g_allocSize, 1,
                            HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy_2d(g_entity, endAddr, g_allocSize, endAddr, g_allocSize, g_allocSize, 1,
                            HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy_2d(g_entity, endAddr, g_allocSize, endAddr, g_allocSize, g_allocSize, 1,
                            HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy_2d(g_entity, startAddr - 1, g_allocSize, startAddr - 1, g_allocSize, g_allocSize, 1,
                            HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy_2d(g_entity, endAddr, g_allocSize, endAddr, g_allocSize, g_allocSize, 1,
                            HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = hybm_data_copy_2d(g_entity, endAddr, g_allocSize, endAddr, g_allocSize, g_allocSize, 1,
                            HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}