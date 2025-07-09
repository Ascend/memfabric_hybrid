/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "hybm.h"
#include "hybm_big_mem.h"
#include "hybm_types.h"
#include "dl_acl_api.h"
#include "hybm_entity_factory.h"

using namespace ock::mf;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
const hybm_options g_options = {HYBM_TYPE_HBM_AI_CORE_INITIATE, HYBM_DOP_TYPE_MTE, HYBM_SCOPE_CROSS_NODE,
                        HYBM_RANK_TYPE_STATIC, 8, 0, 1024 * 1024 * 1024, 0};
const uint64_t g_allocSize = 2 * 1024 * 1024;

class HybmBigMemEntryTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        EXPECT_EQ(hybm_init(0, 0), 0);
    }
    static void TearDownTestSuite()
    {
        hybm_uninit();
    }
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(HybmBigMemEntryTest, hybm_create_entity_ShouldReturnNull_WhenSystemNotInitialized)
{
    hybm_uninit();

    uint16_t id = 1;
    uint32_t flags = 0;

    hybm_entity_t entity = hybm_create_entity(id, &g_options, flags);
    EXPECT_EQ(entity, nullptr);

    EXPECT_EQ(hybm_init(0, 0), 0);
}

TEST_F(HybmBigMemEntryTest, hybm_create_entity_ShouldReturnNull_WhenGetOrCreateEngineFailed)
{
    uint16_t id = 1;
    uint32_t flags = 0;
    EngineImplPtr ptr = nullptr;

    MOCKER_CPP(&MemEntityFactory::GetOrCreateEngine, EngineImplPtr (*)(uint16_t, uint32_t))
        .stubs().will(returnValue(ptr));
    hybm_entity_t entity = hybm_create_entity(id, &g_options, flags);
    EXPECT_EQ(entity, nullptr);
}

TEST_F(HybmBigMemEntryTest, hybm_create_entity_ShouldReturnNull_WhenEntityInitializeFailed)
{
    uint16_t id = 1;
    uint32_t flags = 0;

    MOCKER_CPP(&MemEntityDefault::Initialize, int32_t (*)(const hybm_options *))
            .stubs().will(returnValue(-1));
    hybm_entity_t entity = hybm_create_entity(id, &g_options, flags);
    EXPECT_EQ(entity, nullptr);
}

TEST_F(HybmBigMemEntryTest, hybm_create_entity_ShouldReturnNotNull_WhenSuccess)
{
    uint16_t id = 1;
    uint32_t flags = 0;

    hybm_entity_t entity = hybm_create_entity(id, &g_options, flags);
    EXPECT_NE(entity, nullptr);

    void *reservedMem = nullptr;
    EXPECT_EQ(hybm_reserve_mem_space(entity, 0, &reservedMem), BM_OK);

    hybm_mem_slice_t slice = hybm_alloc_local_memory(entity, HYBM_MEM_TYPE_DEVICE, g_allocSize, 0);
    EXPECT_NE(slice, nullptr);

    hybm_exchange_info info = {};
    EXPECT_EQ(hybm_export(entity, slice, 0, &info), BM_OK);
    EXPECT_EQ(hybm_import(entity, &info, 1, 0), BM_OK);
    EXPECT_EQ(hybm_mmap(entity, 0), BM_OK);

    EXPECT_EQ(hybm_set_extra_context(entity, &g_options, sizeof(g_options)), BM_OK);

    hybm_unmap(entity, 0);
    EXPECT_EQ(hybm_remove_imported(entity, 0, 0), BM_OK);
    EXPECT_EQ(hybm_free_local_memory(entity, slice, g_allocSize, 0), BM_OK);
    EXPECT_EQ(hybm_unreserve_mem_space(entity, 0, reservedMem), BM_OK);

    hybm_destroy_entity(entity, 0);
}