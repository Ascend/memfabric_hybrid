/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include <regex>
#include <sstream>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <iostream>
#include <string>
#include "dl_hcom_api.h"
#include "host_hcom_common.h"

#include "hybm_types.h"
#include "hybm_logger.h"
#include "mf_num_util.h"
#define private public
#include "host_hcom_helper.h"
#include "host_hcom_transport_manager.h"
#undef private


using namespace ock;
using namespace ock::mf;
using namespace ock::mf::transport;
using namespace ock::mf::transport::host;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
namespace {
const uint32_t g_srcRankId = 0;
const uint32_t g_desRankId = 1;
const uint32_t g_rankCount = 1;

}
class HybmHostHcomTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

/* ================================= AI Create UT ========================================= */

/*** Test that AsyncConnect() returns BM_OK by comparing as integer to avoid Result type ambiguity ***/
TEST_F(HybmHostHcomTest, TestAsyncConnectReturnsOk_UsingIntComparison)
{
    auto manager = HcomTransportManager::GetInstance();
    int result = manager->AsyncConnect();
    EXPECT_EQ(result, BM_OK);
}

TEST_F(HybmHostHcomTest, WaitForConnected_ReturnsBMOK)
{
    auto manager = HcomTransportManager::GetInstance();
    auto result = manager->WaitForConnected(1000);
    EXPECT_EQ(result, BM_OK);
}

TEST_F(HybmHostHcomTest, GetNic_ReturnsSetNic)
{
    auto manager = HcomTransportManager::GetInstance();
    manager->localNic_ = "eth0";
    const std::string& nic = manager->GetNic();
    EXPECT_EQ(nic, "eth0");
}

/*** Test that CheckTransportOptions returns BM_OK when AnalysisNic succeeds ***/
TEST_F(HybmHostHcomTest, CheckTransportOptions_SuccessCase)
{
    std::string protocol = "tcp";
    std::string localIp = "127.0.0.1";
    int localPort = 1234;

    MOCKER(ock::mf::transport::host::HostHcomHelper::AnalysisNic)
        .stubs()
        .with(any(), outBound(protocol), outBound(localIp), outBound(localPort))
        .will(returnValue(static_cast<int>(ock::mf::BM_OK)));

    TransportOptions options;
    options.nic = "test_nic";

    HcomTransportManager manager;
    ock::mf::Result ret = manager.CheckTransportOptions(options);

    EXPECT_EQ(ret, ock::mf::BM_OK);
}

/*** happy path: verify function returns BM_OK when called with valid parameters ***/
TEST_F(HybmHostHcomTest, TransportRpcHcomNewEndPoint_HappyPath)
{
    Hcom_Channel newCh = 0;
    uint64_t usrCtx = 0;
    const char* payLoad = "test";

    EXPECT_EQ(HcomTransportManager::TransportRpcHcomNewEndPoint(newCh, usrCtx, payLoad), BM_OK);
}

/*** Verify that TransportRpcHcomRequestReceived returns BM_OK when called with valid parameters ***/
TEST_F(HybmHostHcomTest, TransportRpcHcomRequestReceived_ReturnsBMOK)
{
    EXPECT_EQ(BM_OK, HcomTransportManager::TransportRpcHcomRequestReceived(0, 0));
}

/*** Test that TransportRpcHcomRequestPosted returns BM_OK when called with valid parameters ***/
TEST_F(HybmHostHcomTest, TransportRpcHcomRequestPosted_HappyPath)
{
    EXPECT_EQ(BM_OK, HcomTransportManager::TransportRpcHcomRequestPosted(0, 0));
}

/*** Test that TransportRpcHcomOneSideDone returns BM_OK with valid parameters ***/
TEST_F(HybmHostHcomTest, TransportRpcHcomOneSideDone_HappyPath)
{
    Service_Context ctx = 0;
    uint64_t usrCtx = 1;
    HcomTransportManager manager;
    EXPECT_EQ(manager.TransportRpcHcomOneSideDone(ctx, usrCtx), BM_OK);
}