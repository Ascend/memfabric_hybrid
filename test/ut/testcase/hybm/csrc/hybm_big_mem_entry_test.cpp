/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
namespace {
const uint16_t g_id = 1;
const uint32_t g_flags = 1;
const hybm_options g_options = {HYBM_TYPE_HBM_AI_CORE_INITIATE, HYBM_DOP_TYPE_MTE, HYBM_SCOPE_CROSS_NODE,
                                HYBM_RANK_TYPE_STATIC, 8, 0, 0, 1024 * 1024 * 1024, 0, true};
const uint64_t g_allocSize = 2 * 1024 * 1024;
}

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

    hybm_entity_t entity = hybm_create_entity(g_id, &g_options, g_flags);
    EXPECT_EQ(entity, nullptr);

    EXPECT_EQ(hybm_init(0, 0), 0);
}

TEST_F(HybmBigMemEntryTest, hybm_create_entity_ShouldReturnNull_WhenGetOrCreateEngineFailed)
{
    EngineImplPtr ptr = nullptr;

    MOCKER_CPP(&MemEntityFactory::GetOrCreateEngine, EngineImplPtr (*)(uint16_t, uint32_t))
        .stubs().will(returnValue(ptr));
    hybm_entity_t entity = hybm_create_entity(g_id, &g_options, g_flags);
    EXPECT_EQ(entity, nullptr);
}

TEST_F(HybmBigMemEntryTest, hybm_create_entity_ShouldReturnNull_WhenEntityInitializeFailed)
{
    MOCKER_CPP(&MemEntityDefault::Initialize, int32_t (*)(const hybm_options *))
            .stubs().will(returnValue(-1));
    hybm_entity_t entity = hybm_create_entity(g_id, &g_options, g_flags);
    EXPECT_EQ(entity, nullptr);
}

TEST_F(HybmBigMemEntryTest, hybm_create_entity_ShouldReturnNotNull_WhenSuccess)
{
    hybm_entity_t entity = hybm_create_entity(g_id, &g_options, g_flags);
    EXPECT_NE(entity, nullptr);

    void *reservedMem = nullptr;
    EXPECT_EQ(hybm_reserve_mem_space(entity, 0, &reservedMem), BM_OK);

    hybm_mem_slice_t slice = hybm_alloc_local_memory(entity, HYBM_MEM_TYPE_DEVICE, g_allocSize, 0);
    EXPECT_NE(slice, nullptr);

    hybm_exchange_info info = {};
    EXPECT_EQ(hybm_export(entity, slice, 0, &info), BM_OK);
    void* addresses[1] = { nullptr };
    EXPECT_EQ(hybm_import(entity, &info, 1, addresses, 0), BM_OK);
    EXPECT_EQ(hybm_mmap(entity, 0), BM_OK);

    EXPECT_EQ(hybm_set_extra_context(entity, &g_options, sizeof(g_options)), BM_OK);

    hybm_unmap(entity, 0);
    EXPECT_EQ(hybm_remove_imported(entity, 0, 0), BM_OK);
    EXPECT_EQ(hybm_free_local_memory(entity, slice, g_allocSize, 0), BM_OK);
    EXPECT_EQ(hybm_unreserve_mem_space(entity, 0, reservedMem), BM_OK);

    hybm_destroy_entity(entity, 0);
}

TEST_F(HybmBigMemEntryTest, hybm_destroy_entity_ShouldNotCrash_WhenEntityInvalid)
{
    hybm_entity_t entity = hybm_create_entity(g_id, &g_options, g_flags);
    EXPECT_NE(entity, nullptr);
    hybm_destroy_entity(entity, 0);
    hybm_destroy_entity(entity, 0);
    hybm_destroy_entity(nullptr, 0);
}

TEST_F(HybmBigMemEntryTest, hybm_reserve_mem_space_ShouldReturnInvalidParam)
{
    hybm_entity_t entity = hybm_create_entity(g_id, &g_options, g_flags);
    EXPECT_NE(entity, nullptr);

    void *reservedMem = nullptr;
    EXPECT_EQ(hybm_reserve_mem_space(nullptr, 0, &reservedMem), BM_INVALID_PARAM);
    EXPECT_EQ(reservedMem, nullptr);
    EXPECT_EQ(hybm_reserve_mem_space(entity, 0, nullptr), BM_INVALID_PARAM);
    EXPECT_EQ(reservedMem, nullptr);

    hybm_destroy_entity(entity, 0);

    EXPECT_EQ(hybm_reserve_mem_space(entity, 0, &reservedMem), BM_INVALID_PARAM);
    EXPECT_EQ(reservedMem, nullptr);
}

TEST_F(HybmBigMemEntryTest, hybm_unreserve_mem_space_ShouldReturnInvalidParam)
{
    hybm_entity_t entity = hybm_create_entity(g_id, &g_options, g_flags);
    EXPECT_NE(entity, nullptr);

    void *reservedMem = nullptr;
    EXPECT_EQ(hybm_unreserve_mem_space(nullptr, 0, &reservedMem), BM_INVALID_PARAM);
    EXPECT_EQ(reservedMem, nullptr);

    hybm_destroy_entity(entity, 0);

    EXPECT_EQ(hybm_unreserve_mem_space(entity, 0, &reservedMem), BM_INVALID_PARAM);
    EXPECT_EQ(reservedMem, nullptr);
}

TEST_F(HybmBigMemEntryTest, hybm_alloc_local_memory_ShouldReturnNull_WhenEntityInvalid)
{
    hybm_entity_t entity = hybm_create_entity(g_id, &g_options, g_flags);
    EXPECT_NE(entity, nullptr);

    EXPECT_EQ(hybm_alloc_local_memory(nullptr, HYBM_MEM_TYPE_DEVICE, g_allocSize, 0), nullptr);

    hybm_destroy_entity(entity, 0);

    EXPECT_EQ(hybm_alloc_local_memory(entity, HYBM_MEM_TYPE_DEVICE, g_allocSize, 0), nullptr);
}

TEST_F(HybmBigMemEntryTest, hybm_free_local_memory_ShouldReturnInvalidParam)
{
    hybm_entity_t entity = hybm_create_entity(g_id, &g_options, g_flags);
    EXPECT_NE(entity, nullptr);

    auto slice = hybm_alloc_local_memory(entity, HYBM_MEM_TYPE_DEVICE, g_allocSize, 0);
    EXPECT_EQ(hybm_free_local_memory(nullptr, slice, g_allocSize, 0), BM_INVALID_PARAM);
    EXPECT_EQ(hybm_free_local_memory(entity, nullptr, g_allocSize, 0), BM_INVALID_PARAM);
    EXPECT_EQ(hybm_free_local_memory(entity, slice, g_allocSize, 0), BM_OK);
    EXPECT_EQ(hybm_free_local_memory(entity, slice, g_allocSize, 0), BM_INVALID_PARAM);

    hybm_destroy_entity(entity, 0);

    EXPECT_EQ(hybm_free_local_memory(entity, slice, g_allocSize, 0), BM_INVALID_PARAM);
}

TEST_F(HybmBigMemEntryTest, hybm_register_local_memory_ShouldReturnInvalidParam)
{
    hybm_entity_t entity = hybm_create_entity(g_id, &g_options, g_flags);
    EXPECT_NE(entity, nullptr);

    void *ptr = malloc(g_allocSize);
    EXPECT_NE(ptr, nullptr);

    EXPECT_EQ(hybm_register_local_memory(nullptr, HYBM_MEM_TYPE_DEVICE, nullptr, g_allocSize, 0), nullptr);

    hybm_destroy_entity(entity, 0);

    EXPECT_EQ(hybm_register_local_memory(entity, HYBM_MEM_TYPE_DEVICE, nullptr, g_allocSize, 0), nullptr);

    free(ptr);
}

TEST_F(HybmBigMemEntryTest, hybm_export_ShouldReturnInvalidParam)
{
    hybm_entity_t entity = hybm_create_entity(g_id, &g_options, g_flags);
    EXPECT_NE(entity, nullptr);

    hybm_exchange_info info = {};
    EXPECT_EQ(hybm_export(nullptr, nullptr, 0, &info), BM_INVALID_PARAM);

    hybm_destroy_entity(entity, 0);

    EXPECT_EQ(hybm_export(entity, nullptr, 0, &info), BM_INVALID_PARAM);
}

TEST_F(HybmBigMemEntryTest, hybm_export_slice_size_ShouldReturnInvalidParam)
{
    hybm_entity_t entity = hybm_create_entity(g_id, &g_options, g_flags);
    EXPECT_NE(entity, nullptr);

    size_t size;
    EXPECT_EQ(hybm_export_slice_size(nullptr, &size), BM_INVALID_PARAM);
    EXPECT_EQ(hybm_export_slice_size(entity, nullptr), BM_INVALID_PARAM);

    hybm_destroy_entity(entity, 0);

    EXPECT_EQ(hybm_export_slice_size(entity, &size), BM_INVALID_PARAM);
}

TEST_F(HybmBigMemEntryTest, hybm_import_ShouldReturnInvalidParam)
{
    hybm_entity_t entity = hybm_create_entity(g_id, &g_options, g_flags);
    EXPECT_NE(entity, nullptr);

    hybm_exchange_info info = {};
    void *addresses[1] = {nullptr};
    EXPECT_EQ(hybm_import(nullptr, &info, 1, addresses, 0), BM_INVALID_PARAM);
    EXPECT_EQ(hybm_import(entity, nullptr, 1, addresses, 0), BM_INVALID_PARAM);

    hybm_destroy_entity(entity, 0);

    EXPECT_EQ(hybm_import(entity, &info, 1, addresses, 0), BM_INVALID_PARAM);
}

TEST_F(HybmBigMemEntryTest, hybm_mmap_ShouldReturnInvalidParam)
{
    hybm_entity_t entity = hybm_create_entity(g_id, &g_options, g_flags);
    EXPECT_NE(entity, nullptr);

    EXPECT_EQ(hybm_mmap(nullptr, 0), BM_INVALID_PARAM);

    hybm_destroy_entity(entity, 0);

    EXPECT_EQ(hybm_mmap(entity, 0), BM_INVALID_PARAM);
}

TEST_F(HybmBigMemEntryTest, hybm_unmap_ShouldNotCrash_WhenEntityInvalid)
{
    hybm_entity_t entity = hybm_create_entity(g_id, &g_options, g_flags);
    EXPECT_NE(entity, nullptr);

    hybm_unmap(nullptr, 0);
    hybm_destroy_entity(entity, 0);
    hybm_unmap(entity, 0);
}

TEST_F(HybmBigMemEntryTest, hybm_remove_imported_ShouldReturnInvalidParam)
{
    hybm_entity_t entity = hybm_create_entity(g_id, &g_options, g_flags);
    EXPECT_NE(entity, nullptr);

    EXPECT_EQ(hybm_remove_imported(nullptr, 0, 0), BM_INVALID_PARAM);

    hybm_destroy_entity(entity, 0);

    EXPECT_EQ(hybm_remove_imported(entity, 0, 0), BM_INVALID_PARAM);
}

TEST_F(HybmBigMemEntryTest, hybm_set_extra_context_ShouldReturnInvalidParam)
{
    hybm_entity_t entity = hybm_create_entity(g_id, &g_options, g_flags);
    EXPECT_NE(entity, nullptr);

    EXPECT_EQ(hybm_set_extra_context(nullptr, &g_options, sizeof(g_options)), BM_INVALID_PARAM);
    EXPECT_EQ(hybm_set_extra_context(entity, nullptr, sizeof(g_options)), BM_INVALID_PARAM);

    hybm_destroy_entity(entity, 0);

    EXPECT_EQ(hybm_set_extra_context(entity, &g_options, sizeof(g_options)), BM_INVALID_PARAM);
}
