/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "dl_hal_api.h"
#include "hybm_logger.h"
#define private public
#include "hybm_stream.h"
#include "hybm_stream_manager.h"
#undef private

using namespace ock;
using namespace ock::mf;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
namespace {
uint32_t id    = 0;
uint32_t prio  = 0;
uint32_t flags = 0;
int count      = 1;
}
class HybmTsEngineTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

static void mock_operation()
{
    MOCKER_CPP(&DlHalApi::HalResourceIdAlloc, int(*)(uint32_t, struct halResourceIdInputInfo *,
        struct halResourceIdOutputInfo *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHalApi::HalSqCqAllocate, int(*)(uint32_t, struct halSqCqInputInfo *,
        struct halSqCqOutputInfo *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHalApi::HalResourceConfig, int(*)(uint32_t, struct halResourceIdInputInfo *,
        struct halResourceConfigInfo *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHalApi::HalSqCqFree, int(*)(uint32_t, struct halSqCqFreeInfo *))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHalApi::HalResourceIdFree, int(*)(uint32_t, struct halResourceIdInputInfo *))
        .stubs().will(returnValue(0));
}

static int HalSqCqQuery_stub(uint32_t devId, struct halSqCqQueryInfo *info)
{
    if (info->prop == DRV_SQCQ_PROP_SQ_CQE_STATUS) {
        info->value[0] = 0;
    } else {
        info->value[0] = 1;
    }
    return BM_OK;
}

static int HalSqCqQueryElse_stub(uint32_t devId, struct halSqCqQueryInfo *info)
{
    info->value[0] = 1;
    return BM_OK;
}

static int HalCqReportRecv_stub(uint32_t devId, struct halReportRecvInfo *info)
{
    if (count == 0) {
        info->report_cqe_num = 0;
        return BM_ERROR;
    }
    info->report_cqe_num = 1;
    count--;
    return BM_OK;
}

static int HalResourceIdAlloc_stub(uint32_t devId, struct halResourceIdInputInfo *in,
                                   struct halResourceIdOutputInfo *out)
{
    out->resourceId = 1;
    return BM_OK;
}

TEST_F(HybmTsEngineTest, HybmStream_Initialize_ShouldReturnOk)
{
    HybmStream stream(id, prio, flags);
    mock_operation();
    int ret = stream.Initialize();
    EXPECT_EQ(ret, BM_OK);
    ret = stream.Initialize();
    EXPECT_EQ(ret, BM_OK);
    stream.Destroy();
}

TEST_F(HybmTsEngineTest, HybmStream_Initialize_ErrorBranch)
{
    HybmStream stream(id, prio, flags);
    MOCKER_CPP(&DlHalApi::HalResourceIdAlloc, int(*)(uint32_t, struct halResourceIdInputInfo *,
        struct halResourceIdOutputInfo *)).stubs().will(returnValue(-1));
    int ret = stream.Initialize();
    EXPECT_EQ(ret, BM_ERROR);
    GlobalMockObject::verify();

    MOCKER(&DlHalApi::HalResourceIdAlloc).stubs().will(invoke(HalResourceIdAlloc_stub));
    MOCKER_CPP(&DlHalApi::HalSqCqAllocate, int(*)(uint32_t, struct halSqCqInputInfo *,
        struct halSqCqOutputInfo *)).stubs().will(returnValue(-1));
    ret = stream.Initialize();
    EXPECT_EQ(ret, BM_ERROR);
    ret = stream.GetId();
    EXPECT_EQ(ret, 1);
    GlobalMockObject::verify();

    MOCKER_CPP(&DlHalApi::HalSqCqAllocate, int(*)(uint32_t, struct halSqCqInputInfo *,
        struct halSqCqOutputInfo *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHalApi::HalResourceConfig, int(*)(uint32_t, struct halResourceIdInputInfo *,
        struct halResourceConfigInfo *)).stubs().will(returnValue(-1));
    ret = stream.Initialize();
    EXPECT_EQ(ret, BM_ERROR);
    GlobalMockObject::verify();
    stream.Destroy();

    MOCKER_CPP(&DlHalApi::HalSqCqAllocate, int(*)(uint32_t, struct halSqCqInputInfo *,
        struct halSqCqOutputInfo *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHalApi::HalResourceConfig, int(*)(uint32_t, struct halResourceIdInputInfo *,
        struct halResourceConfigInfo *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHalApi::HalSqCqFree, int(*)(uint32_t, struct halSqCqFreeInfo *))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHalApi::HalResourceIdFree, int(*)(uint32_t, struct halResourceIdInputInfo *))
        .stubs().will(returnValue(-1));
    ret = stream.Initialize();
    EXPECT_EQ(ret, BM_OK);
    stream.Destroy();

    GlobalMockObject::verify();
    MOCKER_CPP(&DlHalApi::HalSqCqFree, int(*)(uint32_t, struct halSqCqFreeInfo *))
        .stubs().will(returnValue(-1));
    stream.Destroy();
}

TEST_F(HybmTsEngineTest, HybmStream_SubmitTasks_Test)
{
    HybmStream stream(id, prio, flags);
    StreamTask tasks;
    tasks.type = STREAM_TASK_TYPE_SDMA;
    int ret = stream.SubmitTasks(tasks);
    EXPECT_EQ(ret, BM_NOT_INITIALIZED);
    ret = stream.Synchronize();
    EXPECT_EQ(ret, BM_NOT_INITIALIZED);

    mock_operation();
    MOCKER_CPP(&DlHalApi::HalSqTaskSend, int(*)(uint32_t, struct halTaskSendInfo *))
        .stubs().will(returnValue(0));
    ret = stream.Initialize();
    EXPECT_EQ(ret, BM_OK);
    ret = stream.SubmitTasks(tasks);
    EXPECT_EQ(ret, BM_OK);

    GlobalMockObject::verify();
    MOCKER_CPP(&DlHalApi::HalSqTaskSend, int(*)(uint32_t, struct halTaskSendInfo *))
        .stubs().will(returnValue(-1));
    MOCKER_CPP(&DlHalApi::HalSqCqFree, int(*)(uint32_t, struct halSqCqFreeInfo *))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHalApi::HalResourceIdFree, int(*)(uint32_t, struct halResourceIdInputInfo *))
        .stubs().will(returnValue(0));
    ret = stream.SubmitTasks(tasks);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    stream.Destroy();
}

TEST_F(HybmTsEngineTest, HybmStream_Synchronize_Test)
{
    HybmStream stream(id, prio, flags);
    StreamTask tasks;
    tasks.type = STREAM_TASK_TYPE_SDMA;
    mock_operation();
    int ret = stream.Initialize();
    EXPECT_EQ(ret, BM_OK);
    ret = stream.Synchronize();
    EXPECT_EQ(ret, BM_OK);
    MOCKER_CPP(&DlHalApi::HalSqTaskSend, int(*)(uint32_t, struct halTaskSendInfo *))
        .stubs().will(returnValue(0));
    MOCKER(&DlHalApi::HalSqCqQuery).stubs().will(invoke(HalSqCqQuery_stub));
    ret = stream.SubmitTasks(tasks);
    EXPECT_EQ(ret, BM_OK);
    ret = stream.Synchronize();
    EXPECT_EQ(ret, BM_OK);
    stream.Destroy();
}

TEST_F(HybmTsEngineTest, HybmStream_Synchronize_else_Branch_Test)
{
    HybmStream stream(id, prio, flags);
    StreamTask tasks;
    tasks.type = STREAM_TASK_TYPE_SDMA;
    mock_operation();
    int ret = stream.Initialize();
    EXPECT_EQ(ret, BM_OK);
    ret = stream.Synchronize();
    EXPECT_EQ(ret, BM_OK);
    MOCKER_CPP(&DlHalApi::HalSqTaskSend, int(*)(uint32_t, struct halTaskSendInfo *))
        .stubs().will(returnValue(0));
    MOCKER(&DlHalApi::HalSqCqQuery).stubs().will(invoke(HalSqCqQueryElse_stub));
    MOCKER(&DlHalApi::HalCqReportRecv).stubs().will(invoke(HalCqReportRecv_stub));
    ret = stream.SubmitTasks(tasks);
    EXPECT_EQ(ret, BM_OK);
    ret = stream.Synchronize();
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    stream.Destroy();
}

TEST_F(HybmTsEngineTest, HybmStreamManager_CreateStream_Test)
{
    HybmStreamManager manager;
    std::shared_ptr<HybmStream> stream = manager.CreateStream(id, prio, flags);
    MOCKER_CPP(&DlHalApi::HalSqCqAllocate, int(*)(uint32_t, struct halSqCqInputInfo *,
        struct halSqCqOutputInfo *)).stubs().will(returnValue(-1));
    int ret = stream->AllocLogicCq();
    EXPECT_EQ(ret, BM_ERROR);
}