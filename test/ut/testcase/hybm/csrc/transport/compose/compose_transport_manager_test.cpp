/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "compose_transport_manager.h"
#include "hybm_transport_common.h"
#include "hybm_def.h"

using namespace ock::mf::transport;
using namespace ock::mf;
namespace {
    const uint32_t g_srcRankId = 0;
    const uint32_t g_desRankId = 1;
    const uint32_t g_rankCount = 2;
}
class HybmComposeTransManagerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(HybmComposeTransManagerTest, OpenDeviceAndHostTrans)
{
    int ret = 0;
    ComposeTransportManager transmanager;
    TransportOptions options;
    options.rankId = g_srcRankId;
    options.rankCount = g_rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.nic = "device://0.0.0.0:8080";
    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_ERROR);
}