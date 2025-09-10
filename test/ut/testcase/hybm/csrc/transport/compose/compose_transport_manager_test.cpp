/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "gtest/gtest.h"
#include "dl_acl_api.h"
#include "mockcpp/mockcpp.hpp"

#include "hybm_def.h"
#include "hybm_transport_common.h"
#include "compose_transport_manager.h"
#include "host_hcom_transport_manager.h"
#include "device_rdma_transport_manager.h"

using namespace ock::mf::transport;
using namespace ock::mf;
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
namespace {
    const uint32_t g_srcRankId = 0;
    const uint32_t g_desRankId = 1;
    const uint32_t g_rankCount = 2;
    uint64_t g_size            = 0;
}
class HybmComposeTransManagerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(HybmComposeTransManagerTest, OpenDeviceTrans)
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

    options.nic = "device#8080";
    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_ERROR);
    options.nic = "device#8080";
    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_ERROR);

    TransportMemoryRegion regionparams;
    ret = transmanager.RegisterMemoryRegion(regionparams);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    regionparams.flags = 2;
    MOCKER_CPP(&device::RdmaTransportManager::RegisterMemoryRegion, int (*)(const TransportMemoryRegion &))
              .stubs().will(returnValue(0));
    ret = transmanager.RegisterMemoryRegion(regionparams);
    EXPECT_EQ(ret, BM_OK);
    TransportMemoryKey key;
    MOCKER_CPP(&device::RdmaTransportManager::QueryMemoryKey, int (*)(uint64_t, TransportMemoryKey &))
              .stubs().will(returnValue(0));
    ret = transmanager.QueryMemoryKey(regionparams.addr, key);
    EXPECT_EQ(ret, BM_OK);

    key.keys[0] = TT_HCOM;
    ret = transmanager.ParseMemoryKey(key, regionparams.addr, g_size);
    EXPECT_EQ(ret, BM_ERROR);
    key.keys[0] = TT_HCCP;
    MOCKER_CPP(&device::RdmaTransportManager::ParseMemoryKey, int (*)(const TransportMemoryKey, uint64_t &, uint64_t &))
              .stubs().will(returnValue(0));
    ret = transmanager.ParseMemoryKey(key, regionparams.addr, g_size);
    EXPECT_EQ(ret, BM_OK);

    MOCKER_CPP(&device::RdmaTransportManager::UnregisterMemoryRegion, int (*)(uint64_t))
              .stubs().will(returnValue(0));
    ret = transmanager.UnregisterMemoryRegion(regionparams.addr);
    EXPECT_EQ(ret, BM_OK);

    ret = transmanager.CloseDevice();
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmComposeTransManagerTest, OpenHostTrans)
{
    int ret = 0;
    ComposeTransportManager transmanager;
    TransportOptions options;
    options.rankId = g_srcRankId;
    options.rankCount = g_rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.nic = "host://0.0.0.0:3030";
    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_ERROR);

    MOCKER_CPP(&host::HcomTransportManager::OpenDevice, int (*)(const TransportOptions &))
              .stubs().will(returnValue(0));
    options.nic = "host#3030";
    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);
    options.nic = "host#3030";
    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_ERROR);

    TransportMemoryRegion regionparams;
    ret = transmanager.RegisterMemoryRegion(regionparams);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    regionparams.flags = 1;
    ret = transmanager.RegisterMemoryRegion(regionparams);
    EXPECT_EQ(ret, BM_ERROR);
    MOCKER_CPP(&host::HcomTransportManager::RegisterMemoryRegion, int (*)(const TransportMemoryRegion &))
              .stubs().will(returnValue(0));
    ret = transmanager.RegisterMemoryRegion(regionparams);
    EXPECT_EQ(ret, BM_OK);

    TransportMemoryKey key;
    MOCKER_CPP(&host::HcomTransportManager::QueryMemoryKey, int (*)(uint64_t, TransportMemoryKey &))
              .stubs().will(returnValue(0));
    ret = transmanager.QueryMemoryKey(regionparams.addr, key);
    EXPECT_EQ(ret, BM_OK);

    key.keys[0] = TT_HCCP;
    ret = transmanager.ParseMemoryKey(key, regionparams.addr, g_size);
    EXPECT_EQ(ret, BM_ERROR);
    key.keys[0] = TT_HCOM;
    MOCKER_CPP(&host::HcomTransportManager::ParseMemoryKey, int (*)(const TransportMemoryKey, uint64_t &, uint64_t &))
              .stubs().will(returnValue(0));
    ret = transmanager.ParseMemoryKey(key, regionparams.addr, g_size);
    EXPECT_EQ(ret, BM_OK);

    MOCKER_CPP(&host::HcomTransportManager::UnregisterMemoryRegion, int (*)(uint64_t))
              .stubs().will(returnValue(0));
    ret = transmanager.UnregisterMemoryRegion(regionparams.addr);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.CloseDevice();
    EXPECT_EQ(ret, BM_OK);
}