/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "hybm.h"
#include "hybm_big_mem.h"
#include "hybm_types.h"
#include "dl_acl_api.h"

#define private public
#include "hybm_data_operator_sdma.h"
#undef private

using namespace ock::mf;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
namespace {
HostDataOpSDMA g_dataOperator = HostDataOpSDMA();
void *g_srcVA = reinterpret_cast<void *>(0x100000000000ULL);
void *g_dstVA = reinterpret_cast<void *>(0x100000000000ULL);
const uint64_t g_size = 1024;
}

class HybmDataOpSdmaTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        EXPECT_EQ(hybm_init(0, 0), BM_OK);
        EXPECT_EQ(g_dataOperator.Initialize(), BM_OK);
    }

    static void TearDownTestSuite()
    {
        g_dataOperator.UnInitialize();
        hybm_uninit();
    }

    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(HybmDataOpSdmaTest, DataCopy_ShouldReturnSuccess)
{
    int ret = 0;
    ret = g_dataOperator.DataCopy(g_srcVA, g_dstVA, g_size, HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);

    ret = g_dataOperator.DataCopy(g_srcVA, g_dstVA, g_size, HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);

    ret = g_dataOperator.DataCopy(g_srcVA, g_dstVA, g_size, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);

    ret = g_dataOperator.DataCopy(g_srcVA, g_dstVA, g_size, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);

    ret = g_dataOperator.DataCopy(g_srcVA, g_dstVA, g_size, HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);

    ret = g_dataOperator.DataCopy(g_srcVA, g_dstVA, g_size, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    ret = g_dataOperator.DataCopyAsync(g_srcVA, g_dstVA, g_size, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
    EXPECT_EQ(ret, BM_ERROR);

    EXPECT_EQ(g_dataOperator.Wait(0), BM_ERROR);
}

TEST_F(HybmDataOpSdmaTest, DataCopy2D_ShouldReturnSuccess)
{
    int ret = 0;
    ret = g_dataOperator.DataCopy2d(g_srcVA, g_size, g_dstVA, g_size, g_size, 1,
                                    HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);
    ret = g_dataOperator.DataCopy2d(g_srcVA, g_size, g_dstVA, g_size, g_size, 1,
                                    HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);

    ret = g_dataOperator.DataCopy2d(g_srcVA, g_size, g_dstVA, g_size, g_size, 1,
                                    HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);

    ret = g_dataOperator.DataCopy2d(g_srcVA, g_size, g_dstVA, g_size, g_size, 1,
                                    HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);

    ret = g_dataOperator.DataCopy2d(g_srcVA, g_size, g_dstVA, g_size, g_size, 1,
                                    HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);

    ret = g_dataOperator.DataCopy2d(g_srcVA, g_size, g_dstVA, g_size, g_size, 1,
                                    HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

TEST_F(HybmDataOpSdmaTest, CopyHost2Gva_ShouldReturnFail_WhenAclApiFailed)
{
    int ret = 0;
    MOCKER_CPP(&DlAclApi::AclrtMalloc, int (*)(void **, size_t, uint32_t)).stubs().will(returnValue(-1));
    ret = g_dataOperator.CopyHost2Gva(g_srcVA, g_dstVA, g_size, nullptr);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);

    ret = g_dataOperator.CopyGva2Host(g_srcVA, g_dstVA, g_size, nullptr);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    GlobalMockObject::verify();

    MOCKER_CPP(&DlAclApi::AclrtMemcpy, int (*)(void *, size_t, void *, size_t, uint32_t))
        .stubs().will(returnValue(-1));
    ret = g_dataOperator.CopyHost2Gva(g_srcVA, g_dstVA, g_size, nullptr);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);

    ret = g_dataOperator.CopyGva2Host(g_srcVA, g_dstVA, g_size, nullptr);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    GlobalMockObject::verify();

    MOCKER_CPP(&DlAclApi::AclrtMemcpyAsync, int (*)(void *, size_t, void *, size_t, uint32_t, void *))
        .stubs().will(returnValue(-1));
    ret = g_dataOperator.CopyHost2Gva(g_srcVA, g_dstVA, g_size, nullptr);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);

    ret = g_dataOperator.CopyGva2Host(g_srcVA, g_dstVA, g_size, nullptr);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    GlobalMockObject::verify();

    MOCKER_CPP(&DlAclApi::AclrtSynchronizeStream, int (*)(void *)).stubs().will(returnValue(-1));
    ret = g_dataOperator.CopyHost2Gva(g_srcVA, g_dstVA, g_size, nullptr);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);

    ret = g_dataOperator.CopyGva2Host(g_srcVA, g_dstVA, g_size, nullptr);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    GlobalMockObject::verify();

    MOCKER_CPP(&DlAclApi::AclrtFree, int (*)(void *)).stubs().will(returnValue(-1));
    ret = g_dataOperator.CopyHost2Gva(g_srcVA, g_dstVA, g_size, nullptr);
    EXPECT_EQ(ret, BM_OK);

    ret = g_dataOperator.CopyGva2Host(g_srcVA, g_dstVA, g_size, nullptr);
    EXPECT_EQ(ret, BM_OK);
    GlobalMockObject::verify();
}

TEST_F(HybmDataOpSdmaTest, CopyHost2Gva2d_ShouldReturnFail_WhenAclApiFailed)
{
    int ret = 0;
    MOCKER_CPP(&DlAclApi::AclrtMalloc, int (*)(void **, size_t, uint32_t)).stubs().will(returnValue(-1));
    ret = g_dataOperator.CopyHost2Gva2d(g_srcVA, g_size, g_dstVA, g_size, g_size, 1, nullptr);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);

    ret = g_dataOperator.CopyGva2Host2d(g_srcVA, g_size, g_dstVA, g_size, g_size, 1, nullptr);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    GlobalMockObject::verify();

    MOCKER_CPP(&DlAclApi::AclrtMemcpy2d, int (*)(void *, size_t, const void *, size_t, size_t, size_t, uint32_t))
        .stubs().will(returnValue(-1));
    ret = g_dataOperator.CopyHost2Gva2d(g_srcVA, g_size, g_dstVA, g_size, g_size, 1, nullptr);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);

    ret = g_dataOperator.CopyGva2Host2d(g_srcVA, g_size, g_dstVA, g_size, g_size, 1, nullptr);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    GlobalMockObject::verify();

    MOCKER_CPP(&DlAclApi::AclrtMemcpyAsync, int (*)(void *, size_t, void *, size_t, uint32_t, void *))
        .stubs().will(returnValue(-1));
    ret = g_dataOperator.CopyHost2Gva2d(g_srcVA, g_size, g_dstVA, g_size, g_size, 1, nullptr);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);

    ret = g_dataOperator.CopyGva2Host2d(g_srcVA, g_size, g_dstVA, g_size, g_size, 1, nullptr);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    GlobalMockObject::verify();

    MOCKER_CPP(&DlAclApi::AclrtSynchronizeStream, int (*)(void *)).stubs().will(returnValue(-1));
    ret = g_dataOperator.CopyHost2Gva2d(g_srcVA, g_size, g_dstVA, g_size, g_size, 1, nullptr);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);

    ret = g_dataOperator.CopyGva2Host2d(g_srcVA, g_size, g_dstVA, g_size, g_size, 1, nullptr);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    GlobalMockObject::verify();

    MOCKER_CPP(&DlAclApi::AclrtFree, int (*)(void *)).stubs().will(returnValue(-1));
    ret = g_dataOperator.CopyHost2Gva2d(g_srcVA, g_size, g_dstVA, g_size, g_size, 1, nullptr);
    EXPECT_EQ(ret, BM_OK);

    ret = g_dataOperator.CopyGva2Host2d(g_srcVA, g_size, g_dstVA, g_size, g_size, 1, nullptr);
    EXPECT_EQ(ret, BM_OK);
    GlobalMockObject::verify();
}