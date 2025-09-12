/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "hybm.h"
#include "hybm_big_mem.h"
#include "hybm_types.h"
#include "dl_api.h"
#include "dl_acl_api.h"
#include "host_hcom_transport_manager.h"

#define private public
#include "hybm_data_operator_rdma.h"
#undef private

using namespace ock::mf;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
namespace {
const uint32_t g_srcRankId = 0;
const uint32_t g_desRankId = 1;
const uint32_t g_rankCount = 2;
transport::TransManagerPtr g_transportManager =
    transport::TransportManager::Create(transport::TransportType::TT_HCOM);
std::shared_ptr<DataOperator> g_dataOperator = std::make_shared<HostDataOpRDMA>(g_srcRankId, g_transportManager);
void *g_srcVA = reinterpret_cast<void *>(0x100000000000ULL);
void *g_dstVA = reinterpret_cast<void *>(0x100000000000ULL);
const uint64_t g_size = 1024;
const uint64_t g_regSize = 1024 * 1024;
}

class HybmDataOpRdmaTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        auto ret = hybm_init(0, 0);
        ASSERT_EQ(BM_OK, ret);
        hybm_set_log_level(0);

        ret = DlApi::LoadExtendLibrary(DL_EXT_LIB_HOST_RDMA);
        ASSERT_EQ(BM_OK, ret);

        transport::TransportOptions options;
        options.rankId = g_srcRankId;
        options.rankCount = g_rankCount;
        options.protocol = HYBM_DOP_TYPE_HOST_RDMA;
        options.nic = "tcp://127.0.0.1:10002";
        EXPECT_EQ(g_transportManager->OpenDevice(options), BM_OK);
        EXPECT_EQ(g_dataOperator->Initialize(), BM_OK);
        transport::TransportMemoryRegion info;
        info.size = g_regSize;
        info.addr = reinterpret_cast<uint64_t>(g_srcVA);
        info.access = transport::REG_MR_ACCESS_FLAG_BOTH_READ_WRITE;
        info.flags = transport::REG_MR_FLAG_DRAM;

        EXPECT_EQ(g_transportManager->RegisterMemoryRegion(info), BM_OK);
        transport::HybmTransPrepareOptions prepareOptions;
        transport::TransportRankPrepareInfo prepareInfo;
        prepareInfo.nic = "tcp://127.0.0.1:10002";
        transport::TransportMemoryKey memoryKey;
        std::fill(std::begin(memoryKey.keys), std::end(memoryKey.keys), 0);
        uint64_t addr = reinterpret_cast<uint64_t>(g_dstVA);
        memoryKey.keys[2] = static_cast<uint32_t>(addr);
        memoryKey.keys[3] = static_cast<uint32_t>(addr >> 32);
        memoryKey.keys[8] = g_regSize;
        prepareInfo.memKeys = {memoryKey};
        prepareOptions.options.emplace(1, prepareInfo);
        EXPECT_EQ(g_transportManager->Prepare(prepareOptions), BM_OK);
        EXPECT_EQ(g_transportManager->Connect(), BM_OK);
    }

    static void TearDownTestSuite()
    {
        g_dataOperator->UnInitialize();
        hybm_uninit();
    }

    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(HybmDataOpRdmaTest, DataCopy_ShouldReturnSuccess)
{
    int ret = 0;
    ExtOptions options{};
    options.flags = 0;
    options.stream = nullptr;
    options.srcRankId = g_srcRankId;
    options.destRankId = g_desRankId;
    hybm_copy_params params = {g_srcVA, g_dstVA, g_size};
    ret = g_dataOperator->DataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options);
    EXPECT_EQ(ret, BM_OK);
    ret = g_dataOperator->DataCopy(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST, options);
    EXPECT_EQ(ret, BM_OK);
    ret = g_dataOperator->DataCopy(params, HYBM_GLOBAL_HOST_TO_GLOBAL_HOST, options);
    EXPECT_EQ(ret, BM_OK);
    ret = g_dataOperator->DataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, options);
    EXPECT_EQ(ret, BM_OK);
    ret = g_dataOperator->DataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, options);
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmDataOpRdmaTest, DataCopy2d_ShouldReturnSuccess)
{
    int ret = 0;
    ExtOptions options{};
    options.flags = 0;
    options.stream = nullptr;
    options.srcRankId = g_srcRankId;
    options.destRankId = g_desRankId;
    hybm_copy_2d_params params = {g_srcVA, g_size, g_dstVA, g_size, g_size, 1};
    ret = g_dataOperator->DataCopy2d(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options);
    EXPECT_EQ(ret, BM_OK);

    ret = g_dataOperator->DataCopy2d(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST, options);
    EXPECT_EQ(ret, BM_OK);
    ret = g_dataOperator->DataCopy2d(params, HYBM_GLOBAL_HOST_TO_GLOBAL_HOST, options);
    EXPECT_EQ(ret, BM_OK);
    ret = g_dataOperator->DataCopy2d(params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, options);
    EXPECT_EQ(ret, BM_OK);
    ret = g_dataOperator->DataCopy2d(params, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, options);
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmDataOpRdmaTest, CopyAsync)
{
    EXPECT_EQ(g_dataOperator->Initialize(), BM_OK);
    int ret = 0;
    ExtOptions options{};
    options.flags = 0;
    options.stream = nullptr;
    options.srcRankId = g_srcRankId;
    options.destRankId = g_desRankId;
    hybm_copy_params params = {g_srcVA, g_dstVA, g_size};
    ret = g_dataOperator->DataCopyAsync(params, HYBM_DATA_COPY_DIRECTION_BUTT, options);
    EXPECT_EQ(ret, BM_ERROR);
    ret = g_dataOperator->Wait(1);
    EXPECT_EQ(ret, BM_ERROR);
    auto g_hostDataOperator = std::dynamic_pointer_cast<HostDataOpRDMA>(g_dataOperator);
    ret = g_hostDataOperator->RtMemoryCopyAsync(g_srcVA, g_dstVA, g_size, 0, options);
    EXPECT_EQ(ret, BM_OK);
    hybm_copy_2d_params params2d;
    params2d.src = g_srcVA;
    params2d.spitch = g_size;
    params2d.dest = g_dstVA;
    params2d.dpitch = g_size;
    params2d.width = g_size;
    params2d.height = 1;
    ret = g_hostDataOperator->RtMemoryCopy2dAsync(params2d, 0, options);
    EXPECT_EQ(ret, BM_OK);

    options.stream = malloc(1024);
    ret = g_hostDataOperator->RtMemoryCopyAsync(g_srcVA, g_dstVA, g_size, 0, options);
    EXPECT_EQ(ret, BM_OK);
    ret = g_hostDataOperator->RtMemoryCopy2dAsync(params2d, 0, options);
    EXPECT_EQ(ret, BM_OK);
    MOCKER_CPP(&DlAclApi::AclrtMemcpyAsync, int32_t(*)(void *, size_t, const void *, size_t, uint32_t,
        void *)).stubs().will(returnValue(1));
    MOCKER_CPP(&DlAclApi::AclrtMemcpy2dAsync, int32_t(*)(void *, size_t, const void *, size_t,
        size_t, size_t, uint32_t, void *)).stubs().will(returnValue(1));
    ret = g_hostDataOperator->RtMemoryCopyAsync(g_srcVA, g_dstVA, g_size, 0, options);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    ret = g_hostDataOperator->RtMemoryCopy2dAsync(params2d, 0, options);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    GlobalMockObject::verify();
    MOCKER_CPP(&DlAclApi::AclrtSynchronizeStream, int32_t(*)(void *)).stubs().will(returnValue(1));
    ret = g_hostDataOperator->RtMemoryCopyAsync(g_srcVA, g_dstVA, g_size, 0, options);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    ret = g_hostDataOperator->RtMemoryCopy2dAsync(params2d, 0, options);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    GlobalMockObject::verify();
}

TEST_F(HybmDataOpRdmaTest, DataCopyErrorBranch)
{
    int ret = 0;
    ExtOptions options{};
    options.flags = 0;
    options.stream = nullptr;
    options.srcRankId = 3;
    options.destRankId = g_desRankId;
    hybm_copy_params params = {g_srcVA, g_dstVA, g_size};
    ret = g_dataOperator->DataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, options);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = g_dataOperator->DataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, options);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = g_dataOperator->DataCopy(params, HYBM_GLOBAL_HOST_TO_GLOBAL_HOST, options);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = g_dataOperator->DataCopy(params, HYBM_DATA_COPY_DIRECTION_BUTT, options);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    hybm_copy_2d_params params2d = {g_srcVA, g_size, g_dstVA, g_size, g_size, 1};
    ret = g_dataOperator->DataCopy2d(params2d, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, options);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = g_dataOperator->DataCopy2d(params2d, HYBM_DATA_COPY_DIRECTION_BUTT, options);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    hybm_copy_2d_params params2d_error = {g_srcVA, g_size, g_dstVA, g_size, 0, 1};
    ret = g_dataOperator->DataCopy2d(params2d_error, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options);
    EXPECT_EQ(ret, BM_ERROR);
    ret = g_dataOperator->DataCopy2d(params2d_error, HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST, options);
    EXPECT_EQ(ret, BM_ERROR);
    ret = g_dataOperator->DataCopy2d(params2d_error, HYBM_GLOBAL_HOST_TO_GLOBAL_HOST, options);
    EXPECT_EQ(ret, BM_ERROR);
    ret = g_dataOperator->DataCopy2d(params2d_error, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, options);
    EXPECT_EQ(ret, BM_ERROR);
    ret = g_dataOperator->DataCopy2d(params2d_error, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, options);
    EXPECT_EQ(ret, BM_ERROR);

    MOCKER_CPP(&DlAclApi::AclrtCreateStream, int32_t(*)(void **)).stubs().will(returnValue(-1));
    ret = g_dataOperator->DataCopy2d(params2d, HYBM_DATA_COPY_DIRECTION_BUTT, options);
    EXPECT_EQ(ret, BM_ERROR);
    GlobalMockObject::verify();
}

TEST_F(HybmDataOpRdmaTest, ErrorBranch)
{
    g_dataOperator->UnInitialize();
    MOCKER_CPP(&DlAclApi::AclrtCreateStream, int32_t(*)(void **)).stubs().will(returnValue(1));
    int ret = g_dataOperator->Initialize();
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    GlobalMockObject::verify();

    g_dataOperator->UnInitialize();
    MOCKER_CPP(&transport::host::HcomTransportManager::RegisterMemoryRegion,
        int32_t(*)(const transport::TransportMemoryRegion &)).stubs().will(returnValue(4));
    ret = g_dataOperator->Initialize();
    EXPECT_EQ(ret, BM_MALLOC_FAILED);
    GlobalMockObject::verify();
}