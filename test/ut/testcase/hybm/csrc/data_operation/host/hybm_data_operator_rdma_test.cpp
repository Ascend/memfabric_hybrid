/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "hybm.h"
#include "hybm_big_mem.h"
#include "hybm_types.h"
#include "dl_acl_api.h"

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
        EXPECT_EQ(hybm_init(0, HYBM_LOAD_FLAG_NEED_HOST_RDMA), BM_OK);
        hybm_set_log_level(0);

        transport::TransportOptions options;
        options.rankId = g_srcRankId;
        options.rankCount = g_rankCount;
        options.protocol = HYBM_DOP_TYPE_ROCE;
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