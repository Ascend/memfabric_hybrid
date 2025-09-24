/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include <regex>
#include <sstream>
#include <arpa/inet.h>
#include <ifaddrs.h>

#include "hybm_logger.h"
#include "mf_num_util.h"
#define private public
#include "host_hcom_helper.h"
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

TEST_F(HybmHostHcomTest, HostHcomHelper)
{
    std::string protocol;
    std::string ipStr;
    int32_t port;
    std::string nic_error1 = "tcp://0.0.0.0:808080";
    int ret = HostHcomHelper::AnalysisNicWithMask(nic_error1, protocol, ipStr, port);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    std::string nic_error2 = "tcp://0.0.0.0/-1:80";
    ret = HostHcomHelper::AnalysisNicWithMask(nic_error2, protocol, ipStr, port);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    std::string nic_error3 = "tcp://0.0.0.0/40:80";
    ret = HostHcomHelper::AnalysisNicWithMask(nic_error3, protocol, ipStr, port);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    std::string nic_error4 = "tcp://0.0.0.0/1:80";
    ret = HostHcomHelper::AnalysisNicWithMask(nic_error4, protocol, ipStr, port);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    std::string nic_error5 = "tcp://0.0.0.0/1:65536";
    ret = HostHcomHelper::AnalysisNicWithMask(nic_error5, protocol, ipStr, port);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    std::string nic_error6 = "tcp://256.256.256.256/1:8080";
    ret = HostHcomHelper::AnalysisNicWithMask(nic_error6, protocol, ipStr, port);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    MOCKER_CPP(&getifaddrs, int(*)(struct ifaddrs **)).stubs().will(returnValue(0));
    std::string nic = "tcp://0.0.0.1/1:8080";
    ret = HostHcomHelper::AnalysisNicWithMask(nic, protocol, ipStr, port);
    EXPECT_EQ(ret, BM_ERROR);
}