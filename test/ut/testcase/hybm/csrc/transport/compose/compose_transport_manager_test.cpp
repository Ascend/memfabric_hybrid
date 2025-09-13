/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "gtest/gtest.h"
#include "dl_acl_api.h"
#include "dl_hcom_api.h"
#include "dl_hccp_api.h"
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
        GlobalMockObject::reset();
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

void host_trans_stub()
{
    MOCKER(DlHcomApi::ServiceCreate).expects(once()).will(returnValue(-1));
}

TEST_F(HybmComposeTransManagerTest, OpenHostTrans_ServiceCreateFail)
{
    int ret = 0;
    ComposeTransportManager transmanager;
    TransportOptions options;
    options.rankId = g_srcRankId;
    options.rankCount = g_rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.nic = "host#tcp://0.0.0.0:3030";

    MOCKER(DlHcomApi::ServiceCreate).expects(once()).will(returnValue(-1));

    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmComposeTransManagerTest, OpenHostTrans_ServiceStartFail)
{
    int ret = 0;
    ComposeTransportManager transmanager;
    TransportOptions options;
    options.rankId = g_srcRankId;
    options.rankCount = g_rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.nic = "host#tcp://0.0.0.0:3030";
    const int registerTimes = 3;

    MOCKER(DlHcomApi::ServiceCreate).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetSendQueueSize).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetRecvQueueSize).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetQueuePrePostSize).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceRegisterChannelBrokerHandler).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceRegisterHandler).expects(exactly(registerTimes)).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetDeviceIpMask).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceBind).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceStart).expects(once()).will(returnValue(-1));
    MOCKER(DlHcomApi::ServiceDestroy).expects(once()).will(returnValue(0));

    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmComposeTransManagerTest, OpenHostTrans_OpenDeviceOK)
{
    int ret = 0;
    ComposeTransportManager transmanager;
    TransportOptions options;
    options.rankId = g_srcRankId;
    options.rankCount = g_rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.nic = "host#tcp://0.0.0.0:3030";
    const int registerTimes = 3;

    MOCKER(DlHcomApi::ServiceCreate).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetSendQueueSize).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetRecvQueueSize).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetQueuePrePostSize).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceStart).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceRegisterChannelBrokerHandler).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceRegisterHandler).expects(exactly(registerTimes)).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetDeviceIpMask).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceBind).expects(once()).will(returnValue(0));

    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);
}

namespace {
int RaRdevGetHandle_stub(uint32_t deviceId, void *&rdmaHandle)
{
    rdmaHandle = (void *)&g_srcRankId;
    return 0;
}

int32_t AclrtGetDevice_stub(int32_t *deviceId)
{
    *deviceId = 1;
    return 0;
}
}

TEST_F(HybmComposeTransManagerTest, OpenDeviceTrans_RaGetIfNumFail)
{
    int ret = 0;
    ComposeTransportManager transmanager;
    TransportOptions options;
    options.rankId = g_srcRankId;
    options.rankCount = g_rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.nic = "device#3030";

    MOCKER(DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDevice_stub));
    MOCKER(DlHccpApi::RaGetIfNum).expects(once()).will(returnValue(-1));
    MOCKER(DlHccpApi::RaRdevGetHandle).stubs().will(invoke(RaRdevGetHandle_stub));

    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_ERROR);
}

int RaGetIfNum_stub(const HccpRaGetIfAttr &config, uint32_t &num)
{
    num = 1;
    return 0;
}

TEST_F(HybmComposeTransManagerTest, OpenDeviceTrans_RaGetIfAddrsFail)
{
    int ret = 0;
    ComposeTransportManager transmanager;
    TransportOptions options;
    options.rankId = g_srcRankId;
    options.rankCount = g_rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.nic = "device#3030";

    MOCKER(DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDevice_stub));
    MOCKER(DlHccpApi::RaRdevGetHandle).stubs().will(invoke(RaRdevGetHandle_stub));
    MOCKER(DlHccpApi::RaGetIfNum).stubs().will(invoke(RaGetIfNum_stub));
    MOCKER(DlHccpApi::RaGetIfAddrs).expects(once()).will(returnValue(-1));

    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmComposeTransManagerTest, OpenDeviceTrans_TsdOpenFail)
{
    int ret = 0;
    ComposeTransportManager transmanager;
    TransportOptions options;
    options.rankId = g_srcRankId;
    options.rankCount = g_rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.nic = "device#3030";

    MOCKER(DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDevice_stub));
    MOCKER(DlHccpApi::RaRdevGetHandle).expects(once()).will(returnValue(-1));
    MOCKER(DlHccpApi::TsdOpen).expects(once()).will(returnValue(1));

    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmComposeTransManagerTest, OpenDeviceTrans_RaInitFail)
{
    int ret = 0;
    ComposeTransportManager transmanager;
    TransportOptions options;
    options.rankId = g_srcRankId;
    options.rankCount = g_rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.nic = "device#3030";

    MOCKER(DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDevice_stub));
    MOCKER(DlHccpApi::RaRdevGetHandle).stubs().will(returnValue(-1));
    MOCKER(DlHccpApi::TsdOpen).expects(once()).will(returnValue(0));
    MOCKER(DlHccpApi::RaGetIfNum).stubs().will(invoke(RaGetIfNum_stub));
    MOCKER(DlHccpApi::RaGetIfAddrs).expects(once()).will(returnValue(-1));
    MOCKER(DlHccpApi::RaInit).expects(once()).will(returnValue(-1));

    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_ERROR);
}

int RaGetIfAddrs_stub(const HccpRaGetIfAttr &config, HccpInterfaceInfo infos[], uint32_t &num)
{
    infos[0].family = AF_INET;
    return 0;
}

TEST_F(HybmComposeTransManagerTest, OpenDeviceTrans_RaRdevInitV2Fail)
{
    int ret = 0;
    ComposeTransportManager transmanager;
    TransportOptions options;
    options.rankId = g_srcRankId;
    options.rankCount = g_rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.nic = "device#3030";

    MOCKER(DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDevice_stub));
    MOCKER(DlHccpApi::RaRdevGetHandle).stubs().will(returnValue(-1));
    MOCKER(DlHccpApi::TsdOpen).expects(once()).will(returnValue(0));
    MOCKER(DlHccpApi::RaGetIfNum).stubs().will(invoke(RaGetIfNum_stub));
    MOCKER(DlHccpApi::RaGetIfAddrs).stubs().will(invoke(RaGetIfAddrs_stub));
    MOCKER(DlHccpApi::RaInit).expects(once()).will(returnValue(0));
    MOCKER(DlHccpApi::RaRdevInitV2).stubs().will(returnValue(-1));

    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_ERROR);
}