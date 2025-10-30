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
#define private public
#include "host_hcom_transport_manager.h"
#include "device_rdma_transport_manager.h"
#include "compose_transport_manager.h"
#undef private

using namespace ock::mf::transport::host;
using namespace ock::mf::transport::device;
using namespace ock::mf::transport;
using namespace ock::mf;
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
namespace {
    const uint32_t g_srcRankId = 0;
    const uint32_t g_desRankId = 1;
    const uint32_t g_rankCount = 2;
    uint64_t g_size            = 0;
    void* g_ptr                = nullptr;
}
class HybmComposeTransManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        g_ptr = malloc(1);
    }
    void TearDown() override
    {
        free(g_ptr);
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
    options.nic = "host://[::]:3030";
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

TEST_F(HybmComposeTransManagerTest, OpenHostTransIpv6_ServiceCreateFail)
{
    int ret = 0;
    ComposeTransportManager transmanager;
    TransportOptions options;
    options.rankId = g_srcRankId;
    options.rankCount = g_rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.nic = "host#tcp6://[::]:3030";

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

static int32_t AclrtGetDeviceStub(int32_t *deviceId)
{
    *deviceId = 1;
    return BM_OK;
}

bool PrepareOpenDeviceStub(uint32_t device, uint32_t rankCount, net_addr_t &deviceIp, void *&rdmaHandle)
{
    rdmaHandle = g_ptr;
    return true;
}

TEST_F(HybmComposeTransManagerTest, PrepareDevice_Test)
{
    int ret = 0;
    uint32_t rankCount = 1;
    ComposeTransportManager transmanager;
    TransportRankPrepareInfo prepareInfo;
    HybmTransPrepareOptions hoptions;
    TransportOptions options;
    options.rankId = g_srcRankId;
    options.rankCount = rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.role = HYBM_ROLE_PEER;
    options.nic = "device#8080";

    prepareInfo.nic = "device#tcp://0.0.0.0:8080";
    hoptions.options.emplace(g_srcRankId, prepareInfo);

    ret = transmanager.Prepare(hoptions);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.Connect();
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.AsyncConnect();
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.WaitForConnected(-1);
    EXPECT_EQ(ret, BM_OK);

    MOCKER(&DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDeviceStub));
    MOCKER(&RdmaTransportManager::PrepareOpenDevice).stubs().will(invoke(PrepareOpenDeviceStub));
    MOCKER_CPP(&DlAclApi::RtGetDeviceInfo, int32_t(*)(uint32_t, int32_t, int32_t, int64_t *))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlAclApi::AclrtMalloc, int32_t(*)(void **ptr, size_t count, uint32_t)).stubs().will(returnValue(0));
    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);

    ret = transmanager.Prepare(hoptions);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ((transmanager.GetNic() == "device#tcp://0.0.0.0:8080;"), 1);
}

TEST_F(HybmComposeTransManagerTest, PrepareDeviceIpv6_Test)
{
    int ret = 0;
    uint32_t rankCount = 1;
    ComposeTransportManager transmanager;
    TransportRankPrepareInfo prepareInfo;
    HybmTransPrepareOptions hoptions;
    TransportOptions options;
    options.type = IpV6;
    options.rankId = g_srcRankId;
    options.rankCount = rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.role = HYBM_ROLE_PEER;
    options.nic = "device#8080";

    prepareInfo.nic = "device#tcp6://[::]:8080";
    hoptions.options.emplace(g_srcRankId, prepareInfo);

    ret = transmanager.Prepare(hoptions);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.Connect();
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.AsyncConnect();
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.WaitForConnected(-1);
    EXPECT_EQ(ret, BM_OK);

    MOCKER(&DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDeviceStub));
    MOCKER(&RdmaTransportManager::PrepareOpenDevice).stubs().will(invoke(PrepareOpenDeviceStub));
    MOCKER_CPP(&DlAclApi::RtGetDeviceInfo, int32_t(*)(uint32_t, int32_t, int32_t, int64_t *))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlAclApi::AclrtMalloc, int32_t(*)(void **ptr, size_t count, uint32_t)).stubs().will(returnValue(0));
    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);

    ret = transmanager.Prepare(hoptions);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ((transmanager.GetNic() == "device#tcp6://[::]:8080;"), 1); // error
}

TEST_F(HybmComposeTransManagerTest, PrepareHost_Test)
{
    int ret = 0;
    uint32_t rankCount = 1;
    ComposeTransportManager transmanager;
    TransportRankPrepareInfo prepareInfo;
    HybmTransPrepareOptions hoptions;
    prepareInfo.nic = "host#tcp://0.0.0.0:3030";
    hoptions.options.emplace(g_srcRankId, prepareInfo);

    ret = transmanager.Prepare(hoptions);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.Connect();
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.AsyncConnect();
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.WaitForConnected(-1);
    EXPECT_EQ(ret, BM_OK);

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
    MOCKER_CPP(&HcomTransportManager::Prepare, int32_t(*)(const HybmTransPrepareOptions &))
        .stubs().will(returnValue(0));

    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);

    ret = transmanager.Prepare(hoptions);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ((transmanager.GetNic() == "host#tcp://0.0.0.0:3030;"), 1);
}

TEST_F(HybmComposeTransManagerTest, PrepareHostIpv6_Test)
{
    int ret = 0;
    uint32_t rankCount = 1;
    ComposeTransportManager transmanager;
    TransportRankPrepareInfo prepareInfo;
    HybmTransPrepareOptions hoptions;
    prepareInfo.nic = "host#tcp6://[::]:3030";
    hoptions.options.emplace(g_srcRankId, prepareInfo);

    ret = transmanager.Prepare(hoptions);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.Connect();
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.AsyncConnect();
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.WaitForConnected(-1);
    EXPECT_EQ(ret, BM_OK);

    TransportOptions options;
    options.type = IpV6;
    options.rankId = g_srcRankId;
    options.rankCount = g_rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.nic = "host#tcp6://[::]:3030";
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
    MOCKER_CPP(&HcomTransportManager::Prepare, int32_t(*)(const HybmTransPrepareOptions &))
        .stubs().will(returnValue(0));

    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);

    ret = transmanager.Prepare(hoptions);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ((transmanager.GetNic() == "host#tcp6://[::]:3030;"), 1);
}

TEST_F(HybmComposeTransManagerTest, ReadRemoteDevice_Test)
{
    int ret = 0;
    uint64_t addr = 0;
    uint32_t rankCount = 1;
    ComposeTransportManager transmanager;
    TransportRankPrepareInfo prepareInfo;
    HybmTransPrepareOptions hoptions;
    ret = transmanager.ReadRemote(g_srcRankId, addr, addr, addr);
    EXPECT_EQ(ret, BM_ERROR);
    ret = transmanager.WriteRemote(g_srcRankId, addr, addr, addr);
    EXPECT_EQ(ret, BM_ERROR);

    TransportMemoryRegion mr;
    mr.flags = 2;
    mr.addr = 0;
    mr.size = 1;
    TransportOptions options;
    options.rankId = g_srcRankId;
    options.rankCount = rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.role = HYBM_ROLE_PEER;
    options.nic = "device#8080";
    prepareInfo.nic = "device#tcp://0.0.0.0:8080";
    hoptions.options.emplace(0, prepareInfo);
    std::cout << mr << std::endl;
    MOCKER(&DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDeviceStub));
    MOCKER(&RdmaTransportManager::PrepareOpenDevice).stubs().will(invoke(PrepareOpenDeviceStub));
    MOCKER_CPP(&DlAclApi::RtGetDeviceInfo, int32_t(*)(uint32_t, int32_t, int32_t, int64_t *))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlAclApi::AclrtMalloc, int32_t(*)(void **ptr, size_t count, uint32_t)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaRegisterMR, int(*)(const void *, HccpMrInfo *, void *&))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&RdmaTransportManager::RemoteIO, int(*)(uint32_t, uint64_t, uint64_t, uint64_t, bool))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaDeregisterMR, int(*)(const void *, void *)).stubs().will(returnValue(0));
    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.RegisterMemoryRegion(mr);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.ReadRemote(g_srcRankId, addr, addr, addr);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.WriteRemote(g_srcRankId, addr, addr, addr);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.UpdateRankOptions(hoptions);
    EXPECT_EQ(ret, BM_OK);

    ret = transmanager.UnregisterMemoryRegion(mr.addr);
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmComposeTransManagerTest, ReadRemoteDeviceIpv6_Test)
{
    int ret = 0;
    uint64_t addr = 0;
    uint32_t rankCount = 1;
    ComposeTransportManager transmanager;
    TransportRankPrepareInfo prepareInfo;
    HybmTransPrepareOptions hoptions;
    ret = transmanager.ReadRemote(g_srcRankId, addr, addr, addr);
    EXPECT_EQ(ret, BM_ERROR);
    ret = transmanager.WriteRemote(g_srcRankId, addr, addr, addr);
    EXPECT_EQ(ret, BM_ERROR);

    TransportMemoryRegion mr;
    mr.flags = 2;
    mr.addr = 0;
    mr.size = 1;
    TransportOptions options;
    options.type = IpV6;
    options.rankId = g_srcRankId;
    options.rankCount = rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.role = HYBM_ROLE_PEER;
    options.nic = "device#8080";
    prepareInfo.nic = "device#tcp6://[::]:8080";
    hoptions.options.emplace(0, prepareInfo);
    std::cout << mr << std::endl;
    MOCKER(&DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDeviceStub));
    MOCKER(&RdmaTransportManager::PrepareOpenDevice).stubs().will(invoke(PrepareOpenDeviceStub));
    MOCKER_CPP(&DlAclApi::RtGetDeviceInfo, int32_t(*)(uint32_t, int32_t, int32_t, int64_t *))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlAclApi::AclrtMalloc, int32_t(*)(void **ptr, size_t count, uint32_t)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaRegisterMR, int(*)(const void *, HccpMrInfo *, void *&))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&RdmaTransportManager::RemoteIO, int(*)(uint32_t, uint64_t, uint64_t, uint64_t, bool))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaDeregisterMR, int(*)(const void *, void *)).stubs().will(returnValue(0));
    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.RegisterMemoryRegion(mr);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.ReadRemote(g_srcRankId, addr, addr, addr);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.WriteRemote(g_srcRankId, addr, addr, addr);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.UpdateRankOptions(hoptions);
    EXPECT_EQ(ret, BM_OK);

    ret = transmanager.UnregisterMemoryRegion(mr.addr);
    EXPECT_EQ(ret, BM_OK);
}

static int ServiceCreateStub(Service_Type t, const char *name, Service_Options options, Hcom_Service *service)
{
    *service = 1;
    return BM_OK;
}

static int ServiceConnectStub(Hcom_Service service, const char *serverUrl, Hcom_Channel *channel,
                              Service_ConnectOptions options)
{
    *channel = 1;
    return BM_OK;
}

TEST_F(HybmComposeTransManagerTest, ReadRemoteHost_Test)
{
    int ret = 0;
    uint64_t addr = 1;
    uint32_t rankCount = 1;
    int flag = 1;
    ComposeTransportManager transmanager;
    TransportRankPrepareInfo prepareInfo;
    HybmTransPrepareOptions hoptions;
    TransportMemoryRegion mr;
    mr.flags = flag;
    mr.addr  = flag;
    mr.size  = flag;
    TransportOptions options;
    options.rankId = g_srcRankId;
    options.rankCount = g_rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.nic = "host#tcp://0.0.0.0:3030";
    uint32_t keys[16] {};
    TransportMemoryKey key;
    std::copy(std::begin(keys), std::end(keys), std::begin(key.keys));
    prepareInfo.memKeys.push_back(key);
    prepareInfo.nic = "host#tcp://0.0.0.0:3030";
    hoptions.options.emplace(0, prepareInfo);
    const int registerTimes = 3;
    std::cout << key << " " << prepareInfo << hoptions << std::endl;
    
    MOCKER(DlHcomApi::ServiceCreate).stubs().will(invoke(ServiceCreateStub));
    MOCKER(DlHcomApi::ServiceSetSendQueueSize).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetRecvQueueSize).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetQueuePrePostSize).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceStart).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceRegisterChannelBrokerHandler).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceRegisterHandler).expects(exactly(registerTimes)).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetDeviceIpMask).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceBind).expects(once()).will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaRegisterMR, int(*)(const void *, HccpMrInfo *, void *&))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaDeregisterMR, int(*)(const void *, void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHcomApi::ServiceRegisterAssignMemoryRegion, int(*)(Hcom_Service, uintptr_t, uint64_t,
        Service_MemoryRegion *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHcomApi::ServiceGetMemoryRegionInfo, int(*)(Service_MemoryRegion, Service_MemoryRegionInfo *))
        .stubs().will(returnValue(0));
    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.RegisterMemoryRegion(mr);
    EXPECT_EQ(ret, BM_OK);

    MOCKER_CPP(&HcomTransportManager::ReadRemote, int32_t(*)(uint32_t, uint64_t, uint64_t, uint64_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&HcomTransportManager::WriteRemote, int32_t(*)(uint32_t, uint64_t, uint64_t, uint64_t))
        .stubs().will(returnValue(0));
    ret = transmanager.ReadRemote(g_srcRankId, addr, addr, addr);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.WriteRemote(g_srcRankId, addr, addr, addr);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.UpdateRankOptions(hoptions);
    EXPECT_EQ(ret, BM_OK);

    ret = transmanager.UnregisterMemoryRegion(mr.addr);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.CloseDevice();
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmComposeTransManagerTest, ReadRemoteHostIpv6_Test)
{
    GlobalMockObject::verify();
    int ret = 0;
    uint64_t addr = 1;
    uint32_t rankCount = 1;
    int flag = 1;
    ComposeTransportManager transmanager;
    TransportRankPrepareInfo prepareInfo;
    HybmTransPrepareOptions hoptions;
    TransportMemoryRegion mr;
    mr.flags = flag;
    mr.addr  = flag;
    mr.size  = flag;
    TransportOptions options;
    options.type = IpV6;
    options.rankId = g_srcRankId;
    options.rankCount = g_rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.nic = "host#tcp6://[::]:3030";
    uint32_t keys[16] {};
    TransportMemoryKey key;
    std::copy(std::begin(keys), std::end(keys), std::begin(key.keys));
    prepareInfo.memKeys.push_back(key);
    prepareInfo.nic = "host#tcp6://[::]:3030";
    hoptions.options.emplace(0, prepareInfo);
    const int registerTimes = 3;
    std::cout << key << " " << prepareInfo << hoptions << std::endl;
    
    MOCKER(DlHcomApi::ServiceCreate).stubs().will(invoke(ServiceCreateStub));
    MOCKER(DlHcomApi::ServiceSetSendQueueSize).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetRecvQueueSize).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetQueuePrePostSize).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceStart).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceRegisterChannelBrokerHandler).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceRegisterHandler).expects(exactly(registerTimes)).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetDeviceIpMask).expects(once()).will(returnValue(0));
    MOCKER(DlHcomApi::ServiceBind).expects(once()).will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaRegisterMR, int(*)(const void *, HccpMrInfo *, void *&))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaDeregisterMR, int(*)(const void *, void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHcomApi::ServiceRegisterAssignMemoryRegion, int(*)(Hcom_Service, uintptr_t, uint64_t,
        Service_MemoryRegion *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHcomApi::ServiceGetMemoryRegionInfo, int(*)(Service_MemoryRegion, Service_MemoryRegionInfo *))
        .stubs().will(returnValue(0));
    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.RegisterMemoryRegion(mr);
    EXPECT_EQ(ret, BM_OK);

    MOCKER_CPP(&HcomTransportManager::ReadRemote, int32_t(*)(uint32_t, uint64_t, uint64_t, uint64_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&HcomTransportManager::WriteRemote, int32_t(*)(uint32_t, uint64_t, uint64_t, uint64_t))
        .stubs().will(returnValue(0));
    ret = transmanager.ReadRemote(g_srcRankId, addr, addr, addr);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.WriteRemote(g_srcRankId, addr, addr, addr);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.UpdateRankOptions(hoptions);
    EXPECT_EQ(ret, BM_OK);

    ret = transmanager.UnregisterMemoryRegion(mr.addr);
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmComposeTransManagerTest, Connect_Test)
{
    int ret = 0;
    ComposeTransportManager transmanager;
    TransportOptions options;
    options.rankId = g_srcRankId;
    options.rankCount = g_rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.nic = "host#tcp://0.0.0.0:3030";

    MOCKER(DlHcomApi::ServiceCreate).stubs().will(invoke(ServiceCreateStub));
    MOCKER(DlHcomApi::ServiceSetSendQueueSize).stubs().will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetRecvQueueSize).stubs().will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetQueuePrePostSize).stubs().will(returnValue(0));
    MOCKER(DlHcomApi::ServiceStart).stubs().will(returnValue(0));
    MOCKER(DlHcomApi::ServiceRegisterChannelBrokerHandler).stubs().will(returnValue(0));
    MOCKER(DlHcomApi::ServiceRegisterHandler).stubs().will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetDeviceIpMask).stubs().will(returnValue(0));
    MOCKER(DlHcomApi::ServiceBind).stubs().will(returnValue(0));
    MOCKER_CPP(&HcomTransportManager::Prepare, int32_t(*)(const HybmTransPrepareOptions &))
        .stubs().will(returnValue(0));

    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.AsyncConnect();
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.WaitForConnected(-1);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.Connect();
    EXPECT_EQ(ret, BM_OK);

    options.nic = "device#tcp://0.0.0.0:8080";
    MOCKER_CPP(&RdmaTransportManager::OpenDevice, int32_t(*)(const TransportOptions &))
        .stubs().will(returnValue(0));
    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);

    ret = transmanager.AsyncConnect();
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.WaitForConnected(-1);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.Connect();
    EXPECT_EQ(ret, BM_OK);

    MOCKER_CPP(&HcomTransportManager::Connect, int32_t(*)()).stubs().will(returnValue(-1));
    MOCKER_CPP(&HcomTransportManager::AsyncConnect, int32_t(*)()).stubs().will(returnValue(-1));
    MOCKER_CPP(&HcomTransportManager::WaitForConnected, int32_t(*)(int64_t)).stubs().will(returnValue(-1));
    ret = transmanager.AsyncConnect();
    EXPECT_EQ(ret, BM_ERROR);
    ret = transmanager.WaitForConnected(-1);
    EXPECT_EQ(ret, BM_ERROR);
    ret = transmanager.Connect();
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmComposeTransManagerTest, ConnectIpv6_Test)
{
    int ret = 0;
    ComposeTransportManager transmanager;
    TransportOptions options;
    options.type = IpV6;
    options.rankId = g_srcRankId;
    options.rankCount = g_rankCount;
    options.protocol = HYBM_DOP_TYPE_BUTT;
    options.nic = "host#tcp6://[::]:8080";

    MOCKER(DlHcomApi::ServiceCreate).stubs().will(invoke(ServiceCreateStub));
    MOCKER(DlHcomApi::ServiceSetSendQueueSize).stubs().will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetRecvQueueSize).stubs().will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetQueuePrePostSize).stubs().will(returnValue(0));
    MOCKER(DlHcomApi::ServiceStart).stubs().will(returnValue(0));
    MOCKER(DlHcomApi::ServiceRegisterChannelBrokerHandler).stubs().will(returnValue(0));
    MOCKER(DlHcomApi::ServiceRegisterHandler).stubs().will(returnValue(0));
    MOCKER(DlHcomApi::ServiceSetDeviceIpMask).stubs().will(returnValue(0));
    MOCKER(DlHcomApi::ServiceBind).stubs().will(returnValue(0));
    MOCKER_CPP(&HcomTransportManager::Prepare, int32_t(*)(const HybmTransPrepareOptions &))
        .stubs().will(returnValue(0));

    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.AsyncConnect();
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.WaitForConnected(-1);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.Connect();
    EXPECT_EQ(ret, BM_OK);

    options.nic = "device#tcp6://[::]:8080";
    MOCKER_CPP(&RdmaTransportManager::OpenDevice, int32_t(*)(const TransportOptions &))
        .stubs().will(returnValue(0));
    ret = transmanager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);

    ret = transmanager.AsyncConnect();
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.WaitForConnected(-1);
    EXPECT_EQ(ret, BM_OK);
    ret = transmanager.Connect();
    EXPECT_EQ(ret, BM_OK);

    MOCKER_CPP(&HcomTransportManager::Connect, int32_t(*)()).stubs().will(returnValue(-1));
    MOCKER_CPP(&HcomTransportManager::AsyncConnect, int32_t(*)()).stubs().will(returnValue(-1));
    MOCKER_CPP(&HcomTransportManager::WaitForConnected, int32_t(*)(int64_t)).stubs().will(returnValue(-1));
    ret = transmanager.AsyncConnect();
    EXPECT_EQ(ret, BM_ERROR);
    ret = transmanager.WaitForConnected(-1);
    EXPECT_EQ(ret, BM_ERROR);
    ret = transmanager.Connect();
    EXPECT_EQ(ret, BM_ERROR);
}


TEST_F(HybmComposeTransManagerTest, CreateCOMPOSETest)
{
    std::shared_ptr<TransportManager> manager = TransportManager::Create(TT_COMPOSE);
    HybmTransPrepareOptions hoptions;
    MOCKER_CPP(&ComposeTransportManager::Connect, int32_t(*)()).stubs().will(returnValue(-1));
    int ret = manager->ConnectWithOptions(hoptions);
    EXPECT_EQ(ret, BM_ERROR);
    GlobalMockObject::verify();
    MOCKER_CPP(&ComposeTransportManager::Prepare, int32_t(*)(const HybmTransPrepareOptions &))
        .stubs().will(returnValue(-1));
    ret = manager->ConnectWithOptions(hoptions);
    EXPECT_EQ(ret, BM_ERROR);
    EXPECT_EQ(TransportManager::Create(TT_BUTT), nullptr);
}