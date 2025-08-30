/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "hybm.h"
#include "hybm_types.h"
#include "dl_acl_api.h"
#include "dl_api.h"

#define private public
#include "hybm_entity_default.h"
#undef private

using namespace ock::mf;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
namespace {
const hybm_options g_options_unified_addr = {HYBM_TYPE_HOST_INITIATE,
                                             HYBM_MEM_TYPE_DEVICE,
                                             HYBM_DOP_TYPE_DEVICE_RDMA,
                                             HYBM_SCOPE_CROSS_NODE,
                                             8,
                                             0,
                                             0,
                                             1024 * 1024 * 1024,
                                             0,
                                             true,
                                             HYBM_ROLE_PEER,
                                             "tcp://127.0.0.1:10002"};
const hybm_options g_options_non_unified_addr = {HYBM_TYPE_HOST_INITIATE,
                                                 HYBM_MEM_TYPE_HOST,
                                                 HYBM_DOP_TYPE_SDMA,
                                                 HYBM_SCOPE_CROSS_NODE,
                                                 8,
                                                 0,
                                                 0,
                                                 1024 * 1024 * 1024,
                                                 0,
                                                 false,
                                                 HYBM_ROLE_PEER,
                                                 "tcp://127.0.0.1:10002"};
const uint64_t g_allocSize = 2 * 1024 * 1024;
}  // namespace

class HybmEntityDefaultTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        auto path = std::getenv("ASCEND_HOME_PATH");
        EXPECT_NE(path, nullptr);
        auto libPath = std::string(path).append("/lib64");
        EXPECT_EQ(DlApi::LoadLibrary(libPath), BM_OK);
    }
    static void TearDownTestSuite()
    {
        DlApi::CleanupLibrary();
    }
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(HybmEntityDefaultTest, Initialize_UA_ShouldReturnOk_WhenAlreadyInitialized)
{
    int ret = 0;
    MemEntityDefault entity(0);
    ret = entity.Initialize(&g_options_unified_addr);
    EXPECT_EQ(ret, BM_OK);

    ret = entity.Initialize(&g_options_unified_addr);
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmEntityDefaultTest, Initialize_Non_UA_ShouldReturnOk)
{
    int ret = 0;
    MemEntityDefault entity(0);
    ret = entity.Initialize(&g_options_non_unified_addr);
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmEntityDefaultTest, Initialize_ShouldReturnInvalidParam_WhenIdInvalid)
{
    int ret = 0;
    MemEntityDefault entity(-1);
    ret = entity.Initialize(&g_options_unified_addr);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    MemEntityDefault entity2(HYBM_ENTITY_NUM_MAX);
    ret = entity.Initialize(&g_options_unified_addr);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

TEST_F(HybmEntityDefaultTest, Initialize_ShouldReturnInvalidParam_WhenOptionsInvalid)
{
    int ret = 0;
    MemEntityDefault entity(0);
    hybm_options options = g_options_unified_addr;
    ret = entity.Initialize(nullptr);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    options.rankId = options.rankCount + 1;
    ret = entity.Initialize(&options);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    options = g_options_unified_addr;
    options.singleRankVASpace = 0;
    ret = entity.Initialize(&options);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    options = g_options_unified_addr;
    options.singleRankVASpace = DEVICE_LARGE_PAGE_SIZE - 1;
    ret = entity.Initialize(&options);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

TEST_F(HybmEntityDefaultTest, Initialize_ShouldReturnDlFunctionFailed_WhenCreateStreamFail)
{
    int ret = 0;
    MemEntityDefault entity(0);
    MOCKER_CPP(&DlAclApi::AclrtCreateStream, int (*)(void **)).stubs().will(returnValue(-1));
    ret = entity.Initialize(&g_options_unified_addr);
    EXPECT_EQ(ret, 0);
}

TEST_F(HybmEntityDefaultTest, Initialize_ShouldReturnInvalidParam_WhenSegmentIsNullptr)
{
    int ret = 0;
    MemEntityDefault entity(0);
    MemSegmentPtr ptr = nullptr;
    MOCKER_CPP(&MemEntityDefault::CheckOptions, int (*)(const hybm_options *)).stubs().will(returnValue(0));
    hybm_options options = g_options_unified_addr;
    options.rankId = options.rankCount + 1;
    ret = entity.Initialize(&options);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

TEST_F(HybmEntityDefaultTest, AllocLocalMemory_ShouldReturnNotInitialized_WhenNotInitialized)
{
    void *reservedMem = nullptr;
    MemEntityDefault entity(0);
    EXPECT_EQ(entity.ReserveMemorySpace(&reservedMem), BM_NOT_INITIALIZED);

    hybm_mem_slice_t slice;
    EXPECT_EQ(entity.AllocLocalMemory(g_allocSize, 0, slice), BM_NOT_INITIALIZED);

    hybm_exchange_info info;
    ExchangeInfoWriter writer1(&info);
    EXPECT_EQ(entity.ExportExchangeInfo(writer1, 0), BM_NOT_INITIALIZED);

    ExchangeInfoWriter writer2(&info);
    EXPECT_EQ(entity.ExportExchangeInfo(slice, writer2, 0), BM_NOT_INITIALIZED);

    void *addresses[1] = {nullptr};
    ExchangeInfoReader reader(&info);
    EXPECT_EQ(entity.ImportExchangeInfo(&reader, 1, addresses, 0), BM_NOT_INITIALIZED);

    EXPECT_EQ(entity.SetExtraContext(&info, 1), BM_NOT_INITIALIZED);

    EXPECT_EQ(entity.Mmap(), BM_NOT_INITIALIZED);

    entity.Unmap();

    std::vector<uint32_t> ranks;
    EXPECT_EQ(entity.RemoveImported(ranks), BM_NOT_INITIALIZED);

    hybm_copy_params params = {nullptr, nullptr, 0};
    EXPECT_EQ(entity.CopyData(params, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0), BM_NOT_INITIALIZED);

    hybm_copy_2d_params params2D = {nullptr, 0, nullptr, 0, 0, 0};
    EXPECT_EQ(entity.CopyData2d(params2D, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0), BM_NOT_INITIALIZED);

    EXPECT_EQ(entity.CheckAddressInEntity(nullptr, 0), false);

    entity.ReleaseResources();
}