/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "hybm_define.h"
#include "hybm_logger.h"
#include "dl_acl_api.h"
#include "dl_hal_api.h"
#include "dl_hccp_api.h"
#include "hybm_stream.h"
#include "device_rdma_common.h"
#include "device_rdma_helper.h"
#include "dynamic_ranks_qp_manager.h"
#define private public
#include "fixed_ranks_qp_manager.h"
#include "device_rdma_transport_manager.h"
#undef private

using namespace ock;
using namespace ock::mf;
using namespace ock::mf::transport;
using namespace ock::mf::transport::device;
using ock::mf::transport::device::FixedRanksQpManager;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
namespace {
const uint32_t g_srcRankId = 0;
const uint32_t g_desRankId = 1;
const uint32_t g_rankCount = 1;
void* g_ptr  = nullptr;
void* g_ptr2 = nullptr;
}
class HybmRdmaTransportManagerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}
    void SetUp() override
    {
        g_ptr = malloc(1);
        g_ptr2 = malloc(sizeof(AiQpRMAQueueInfo));
    }
    void TearDown() override
    {
        free(g_ptr);
        free(g_ptr2);
        GlobalMockObject::verify();
    }
};

static int32_t AclrtGetDeviceStub(int32_t *deviceId)
{
    *deviceId = 1;
    return BM_OK;
}

static int32_t AclrtGetDeviceStubError(int32_t *deviceId)
{
    *deviceId = 1;
    return BM_ERROR;
}

static int RaGetIfNumStub(const HccpRaGetIfAttr &config, uint32_t &num)
{
    num = 1;
    return BM_OK;
}

static int RaGetIfAddrsStub(const HccpRaGetIfAttr &config, HccpInterfaceInfo infos[], uint32_t &num)
{
    infos[0].family = AF_INET;
    inet_pton(AF_INET, "0.0.0.0", &infos[0].ifaddr.ip.addr);
    return BM_OK;
}

static int RaRdevInitV2Stub(const HccpRdevInitInfo &info, const HccpRdev &rdev, void *&rdmaHandle)
{
    rdmaHandle = g_ptr;
    return BM_OK;
}

static int32_t AclrtMallocStub(void **ptr, size_t count, uint32_t type)
{
    *ptr = g_ptr2;
    return BM_OK;
}

static int RaSocketInitStub(HccpNetworkMode mode, const HccpRdev &rdev, void *&socketHandle)
{
    socketHandle = g_ptr;
    return BM_OK;
}

static int RaGetSocketsStub(uint32_t role, HccpSocketInfo conn[], uint32_t num, uint32_t &connectedNum)
{
    connectedNum = 2;
    return BM_OK;
}

static int RaGetQpStatusStub(void *qpHandle, int &status)
{
    status = 1;
    return BM_OK;
}

static int RaSendWrStub(void *qp_handle, struct send_wr *wr, struct send_wr_rsp *op_rsp)
{
    op_rsp->db.db_info = 0;
    return BM_OK;
}

static int32_t RtGetDeviceInfoStub1(uint32_t deviceId, int32_t moduleType, int32_t infoType, int64_t *val)
{
    if (infoType == INFO_TYPE_ADDR_MODE) return BM_ERROR;
    return BM_OK;
}

static int32_t RtGetDeviceInfoStub2(uint32_t deviceId, int32_t moduleType, int32_t infoType, int64_t *val)
{
    if (infoType == INFO_TYPE_PHY_DIE_ID) return BM_ERROR;
    return BM_OK;
}

static int32_t RtGetDeviceInfoStub3(uint32_t deviceId, int32_t moduleType, int32_t infoType, int64_t *val)
{
    if (infoType == INFO_TYPE_PHY_CHIP_ID) return BM_ERROR;
    return BM_OK;
}

/* ======================================  TEST  ====================================== */

TEST_F(HybmRdmaTransportManagerTest, PrepareThreadLocalStreamTest)
{
    int ret = 0;
    RdmaTransportManager managerError;
    MOCKER_CPP(&DlHalApi::HalResourceIdAlloc, int(*)(uint32_t, struct halResourceIdInputInfo *,
        struct halResourceIdOutputInfo *)).stubs().will(returnValue(-1));
    ret = managerError.PrepareThreadLocalStream();
    EXPECT_EQ(ret, BM_ERROR);
    GlobalMockObject::verify();

    RdmaTransportManager manager;
    MOCKER_CPP(&DlHalApi::HalResourceIdAlloc, int(*)(uint32_t, struct halResourceIdInputInfo *,
        struct halResourceIdOutputInfo *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHalApi::HalSqCqAllocate, int(*)(uint32_t, struct halSqCqInputInfo *, struct halSqCqOutputInfo *))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHalApi::HalResourceConfig, int(*)(uint32_t, struct halResourceIdInputInfo *,
        struct halResourceConfigInfo *)).stubs().will(returnValue(0));
    ret = manager.PrepareThreadLocalStream();
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmRdmaTransportManagerTest, ParseMemoryKeyTest)
{
    RdmaTransportManager manager;
    TransportMemoryKey key;
    key.keys[0] = 0;
    uint64_t addr;
    uint64_t size;
    int ret = manager.ParseMemoryKey(key, addr, size);
    EXPECT_EQ(ret, BM_OK);
    ret = manager.AsyncConnect();
    EXPECT_EQ(ret, BM_OK);

    key.keys[0] = 1;
    ret = manager.ParseMemoryKey(key, addr, size);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmRdmaTransportManagerTest, PrepareTest)
{
    int ret = 0;
    RdmaTransportManager manager;
    TransportRankPrepareInfo prepareInfo;
    prepareInfo.nic = "tcp://0.0.0.0:10002";
    HybmTransPrepareOptions options;
    options.options.emplace(0, prepareInfo);

    TransportOptions options2;
    options2.rankId = g_srcRankId;
    options2.rankCount = g_rankCount;
    options2.nic = "10002";
    options2.role = HYBM_ROLE_PEER;
    MOCKER(&DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDeviceStub));
    MOCKER_CPP(&DlHccpApi::RaRdevGetHandle, int(*)(uint32_t, void *&)).stubs().will(returnValue(1));
    MOCKER_CPP(&DlHccpApi::TsdOpen, uint32_t(*)(uint32_t, uint32_t)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaInit, int(*)(const HccpRaInitConfig &)).stubs().will(returnValue(0));
    MOCKER(&DlHccpApi::RaGetIfNum).stubs().will(invoke(RaGetIfNumStub));
    MOCKER(&DlHccpApi::RaGetIfAddrs).stubs().will(invoke(RaGetIfAddrsStub));
    MOCKER(&DlHccpApi::RaRdevInitV2).stubs().will(invoke(RaRdevInitV2Stub));
    MOCKER_CPP(&DlAclApi::RtGetDeviceInfo, int32_t(*)(uint32_t, int32_t, int32_t, int64_t *))
        .stubs().will(returnValue(0));
    ret = manager.OpenDevice(options2);
    EXPECT_EQ(ret, BM_OK);
    
    MOCKER(&DlAclApi::AclrtMalloc).stubs().will(invoke(AclrtMallocStub));
    ret = manager.Prepare(options);
    EXPECT_EQ(ret, BM_OK);

    ret = manager.Connect();
    EXPECT_EQ(ret, BM_OK);
    ret = manager.UpdateRankOptions(options);
    EXPECT_EQ(ret, BM_ERROR);
    EXPECT_EQ(manager.GetNic(), prepareInfo.nic);
    ret = manager.RemoteIO(0, 0, 0, 0, true);
    EXPECT_EQ(ret, BM_ERROR);

    MOCKER_CPP(&DlHccpApi::RaQpDestroy, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketBatchClose, int(*)(HccpSocketCloseInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketDeinit, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketListenStop, int(*)(HccpSocketListenInfo, uint32_t)).stubs().will(returnValue(0));
    ret = manager.CloseDevice();
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmRdmaTransportManagerTest, RemoteIOTest)
{
    int ret = 0;
    RdmaTransportManager manager;
    TransportOptions options;
    options.rankId = g_srcRankId;
    options.rankCount = 2;
    options.nic = "10002";
    options.role = HYBM_ROLE_PEER;
    MOCKER(&DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDeviceStub));
    MOCKER_CPP(&DlHccpApi::RaRdevGetHandle, int(*)(uint32_t, void *&)).stubs().will(returnValue(1));
    MOCKER_CPP(&DlHccpApi::TsdOpen, uint32_t(*)(uint32_t, uint32_t)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaInit, int(*)(const HccpRaInitConfig &)).stubs().will(returnValue(0));
    MOCKER(&DlHccpApi::RaGetIfNum).stubs().will(invoke(RaGetIfNumStub));
    MOCKER(&DlHccpApi::RaGetIfAddrs).stubs().will(invoke(RaGetIfAddrsStub));
    MOCKER(&DlHccpApi::RaRdevInitV2).stubs().will(invoke(RaRdevInitV2Stub));
    MOCKER_CPP(&DlAclApi::RtGetDeviceInfo, int32_t(*)(uint32_t, int32_t, int32_t, int64_t *))
        .stubs().will(returnValue(0));
    ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);

    TransportRankPrepareInfo prepareInfo;
    prepareInfo.nic = "tcp://0.0.0.0:10002";
    HybmTransPrepareOptions options2;
    options2.options.emplace(0, prepareInfo);
    options2.options.emplace(1, prepareInfo);
    MOCKER(&DlAclApi::AclrtMalloc).stubs().will(invoke(AclrtMallocStub));

    MOCKER(&DlHccpApi::RaSocketInit).stubs().will(invoke(RaSocketInitStub));
    MOCKER_CPP(&DlHccpApi::RaSocketListenStart, int(*)(HccpSocketListenInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketWhiteListAdd, int(*)(void *, const HccpSocketWhiteListInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&FixedRanksQpManager::WaitConnectionsReady, int(*)(std::unordered_map<uint32_t,
        FixedRanksQpManager::ConnectionChannel> &)).stubs().will(returnValue(0));
    MOCKER(&DlHccpApi::RaGetSockets).stubs().will(invoke(RaGetSocketsStub));
    MOCKER_CPP(&DlHccpApi::RaQpAiCreate, int(*)(void *, const HccpQpExtAttrs &, HccpAiQpInfo &i, void *&))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaQpCreate, int(*)(void *, int, int, void *&))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaMrReg, int(*)(void *, HccpMrInfo &)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaQpConnectAsync, int(*)(void *, const void *)).stubs().will(returnValue(0));
    MOCKER(&DlHccpApi::RaGetQpStatus).stubs().will(invoke(RaGetQpStatusStub));
    MOCKER_CPP(&DlAclApi::AclrtMemcpy, int32_t(*)(void *, size_t, const void *, size_t, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlAclApi::AclrtSetDevice, int32_t(*)(int32_t)).stubs().will(returnValue(0));

    ret = manager.Prepare(options2);
    EXPECT_EQ(ret, BM_OK);

    ret = manager.ReadRemote(0, 0, 0, 0);
    EXPECT_EQ(ret, BM_ERROR);

    MOCKER_CPP(&FixedRanksQpManager::GetQpHandleWithRankId, void*(*)(uint32_t)).stubs().will(returnValue(g_ptr));
    MOCKER_CPP(&HybmStream::Initialize, int(*)()).stubs().will(returnValue(0));
    MOCKER(&DlHccpApi::RaSendWr).stubs().will(invoke(RaSendWrStub));
    MOCKER_CPP(&HybmStream::SubmitTasks, int32_t(*)(const StreamTask &)).stubs().will(returnValue(0));
    MOCKER_CPP(&HybmStream::Synchronize, int(*)()).stubs().will(returnValue(0));
    ret = manager.ReadRemote(0, 0, 0, 0);
    EXPECT_EQ(ret, BM_OK);

    MOCKER_CPP(&DlHccpApi::RaQpDestroy, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketBatchClose, int(*)(HccpSocketCloseInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketDeinit, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketListenStop, int(*)(HccpSocketListenInfo, uint32_t)).stubs().will(returnValue(0));
    ret = manager.CloseDevice();
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmRdmaTransportManagerTest, OpenDeviceErrorBranchTest)
{
    RdmaTransportManager manager;
    TransportOptions options;
    MOCKER_CPP(&DlAclApi::AclrtGetDevice, int32_t(*)(int32_t *)).stubs().will(returnValue(1));
    int ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);

    GlobalMockObject::verify();
    MOCKER_CPP(&DlAclApi::AclrtGetDevice, int32_t(*)(int32_t *)).stubs().will(returnValue(0));
    ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);

    GlobalMockObject::verify();
    MOCKER(&DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDeviceStubError));
    ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);

    GlobalMockObject::verify();
    MOCKER(&DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDeviceStub));
    options.nic = "tcp://0.0.0.0:90";
    ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    MOCKER_CPP(&RdmaTransportManager::PrepareOpenDevice, bool(*)(uint32_t, uint32_t,
        in_addr &, void *&)).stubs().will(returnValue(false));
    ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    GlobalMockObject::verify();
    options.nic = "90";
    options.role = HYBM_ROLE_RECEIVER;
    MOCKER(&DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDeviceStub));
    MOCKER_CPP(&RdmaTransportManager::PrepareOpenDevice, bool(*)(uint32_t, uint32_t,
        in_addr &, void *&)).stubs().will(returnValue(true));
    MOCKER_CPP(&DeviceChipInfo::Init, int(*)()).stubs().will(returnValue(-1));
    ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_ERROR);

    MOCKER_CPP(&DlHccpApi::RaQpDestroy, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketBatchClose, int(*)(HccpSocketCloseInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketDeinit, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketListenStop, int(*)(HccpSocketListenInfo, uint32_t)).stubs().will(returnValue(0));
    ret = manager.CloseDevice();
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmRdmaTransportManagerTest, RegisterMemoryRegionErrorBranchTest)
{
    RdmaTransportManager manager;
    TransportMemoryRegion regionparams;
    TransportOptions options;
    MOCKER_CPP(&DlHccpApi::RaRegisterMR, int(*)(const void *, HccpMrInfo *, void *&)).stubs().will(returnValue(1));
    int ret = manager.RegisterMemoryRegion(regionparams);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);

    GlobalMockObject::verify();
    MOCKER(&DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDeviceStub));
    options.nic = "90";
    options.role = HYBM_ROLE_PEER;
    MOCKER_CPP(&RdmaTransportManager::PrepareOpenDevice, bool(*)(uint32_t, uint32_t,
        in_addr &, void *&)).stubs().will(returnValue(true));
    MOCKER_CPP(&DeviceChipInfo::Init, int(*)()).stubs().will(returnValue(0));
    ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);
    MOCKER_CPP(&DlHccpApi::RaRegisterMR, int(*)(const void *, HccpMrInfo *, void *&)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaDeregisterMR, int(*)(const void *, void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&FixedRanksQpManager::SetLocalMemories, int(*)(const MemoryRegionMap &)).stubs().will(returnValue(-1));
    ret = manager.RegisterMemoryRegion(regionparams);
    EXPECT_EQ(ret, BM_ERROR);

    ret = manager.UnregisterMemoryRegion(-1);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = manager.UnregisterMemoryRegion(0);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmRdmaTransportManagerTest, QueryMemoryKeyErrorBranchTest)
{
    RdmaTransportManager manager;
    TransportMemoryRegion regionparams;
    TransportOptions options;
    TransportMemoryKey key;
    regionparams.addr = 0;
    regionparams.size = 1;
    regionparams.access = REG_MR_ACCESS_FLAG_BOTH_READ_WRITE;
    options.nic = "90";
    options.role = HYBM_ROLE_PEER;
    int ret = manager.QueryMemoryKey(-1, key);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    MOCKER_CPP(&DlHccpApi::RaRegisterMR, int(*)(const void *, HccpMrInfo *, void *&)).stubs().will(returnValue(0));
    MOCKER(&DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDeviceStub));
    MOCKER_CPP(&RdmaTransportManager::PrepareOpenDevice, bool(*)(uint32_t, uint32_t,
        in_addr &, void *&)).stubs().will(returnValue(true));
    MOCKER_CPP(&DeviceChipInfo::Init, int(*)()).stubs().will(returnValue(0));
    ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);
    MOCKER_CPP(&FixedRanksQpManager::SetLocalMemories, int(*)(const MemoryRegionMap &)).stubs().will(returnValue(0));
    ret = manager.RegisterMemoryRegion(regionparams);
    EXPECT_EQ(ret, BM_OK);
    MOCKER_CPP(&DlHccpApi::RaDeregisterMR, int(*)(const void *, void *)).stubs().will(returnValue(0));
}

TEST_F(HybmRdmaTransportManagerTest, PrepareErrorBranchTest)
{
    int ret = 0;
    RdmaTransportManager manager;
    TransportOptions options;
    options.nic = "90";
    options.role = HYBM_ROLE_PEER;
    options.rankId = 0;
    options.rankCount = 1;

    ret = manager.Connect();
    EXPECT_EQ(ret, BM_ERROR);

    MOCKER(&DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDeviceStub));
    MOCKER_CPP(&RdmaTransportManager::PrepareOpenDevice, bool(*)(uint32_t, uint32_t,
        in_addr &, void *&)).stubs().will(returnValue(true));
    MOCKER_CPP(&DeviceChipInfo::Init, int(*)()).stubs().will(returnValue(0));
    MOCKER_CPP(&FixedRanksQpManager::WaitingConnectionReady, int(*)()).stubs().will(returnValue(-1));
    ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);
    ret = manager.Connect();
    EXPECT_EQ(ret, BM_ERROR);
    
    MOCKER_CPP(&RdmaTransportManager::AsyncConnect, int32_t(*)()).stubs().will(returnValue(-1));
    ret = manager.Connect();
    EXPECT_EQ(ret, BM_ERROR);

    TransportRankPrepareInfo prepareInfo;
    prepareInfo.nic = "tcp://0.0.0.0:90";
    HybmTransPrepareOptions hoptions;
    hoptions.options.emplace(0, prepareInfo);
    ret = manager.Prepare(hoptions);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    MOCKER_CPP(&FixedRanksQpManager::SetRemoteRankInfo, int(*)(const std::unordered_map<uint32_t, ConnectRankInfo> &))
        .stubs().will(returnValue(-1));
    ret = manager.Prepare(hoptions);
    EXPECT_EQ(ret, BM_ERROR);

    prepareInfo.nic = "90";
    hoptions.options.erase(0);
    hoptions.options.emplace(0, prepareInfo);
    ret = manager.Prepare(hoptions);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    hoptions.options.emplace(10, prepareInfo);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    MOCKER_CPP(&DlHccpApi::RaQpDestroy, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketDeinit, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketBatchClose, int(*)(HccpSocketCloseInfo, uint32_t)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketListenStop, int(*)(HccpSocketListenInfo, uint32_t)).stubs().will(returnValue(0));
    ret = manager.CloseDevice();
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmRdmaTransportManagerTest, UpdateRankOptionsErrorBranchTest)
{
    int ret = 0;
    RdmaTransportManager manager;
    TransportOptions options;
    options.nic = "90";
    options.role = HYBM_ROLE_PEER;
    options.rankId = 0;
    options.rankCount = 1;
    TransportRankPrepareInfo prepareInfo;
    prepareInfo.nic = "90";
    HybmTransPrepareOptions hoptions;
    hoptions.options.emplace(0, prepareInfo);

    ret = manager.UpdateRankOptions(hoptions);
    EXPECT_EQ(ret, BM_ERROR);
    EXPECT_EQ((manager.GetQpInfo() == nullptr), 1);

    MOCKER(&DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDeviceStub));
    MOCKER_CPP(&RdmaTransportManager::PrepareOpenDevice, bool(*)(uint32_t, uint32_t,
        in_addr &, void *&)).stubs().will(returnValue(true));
    MOCKER_CPP(&DeviceChipInfo::Init, int(*)()).stubs().will(returnValue(0));
    MOCKER_CPP(&FixedRanksQpManager::WaitingConnectionReady, int(*)()).stubs().will(returnValue(-1));
    ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);

    ret = manager.UpdateRankOptions(hoptions);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    prepareInfo.nic = "tcp://0.0.0.0:90";
    hoptions.options.erase(0);
    hoptions.options.emplace(0, prepareInfo);
    ret = manager.UpdateRankOptions(hoptions);
    EXPECT_EQ(ret, BM_OK);

    MOCKER_CPP(&DlHccpApi::RaQpDestroy, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketDeinit, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketBatchClose, int(*)(HccpSocketCloseInfo, uint32_t)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketListenStop, int(*)(HccpSocketListenInfo, uint32_t)).stubs().will(returnValue(0));
    ret = manager.CloseDevice();
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmRdmaTransportManagerTest, PrepareOpenDeviceErrorBranchTest)
{
    bool ret = true;
    RdmaTransportManager manager;
    in_addr deviceIp;
    void *rdmaHandle = nullptr;
    MOCKER_CPP(&DlHccpApi::RaRdevGetHandle, int(*)(uint32_t, void *&)).stubs().will(returnValue(1));
    MOCKER_CPP(&DlHccpApi::TsdOpen, uint32_t(*)(uint32_t, uint32_t)).stubs().will(returnValue(0));
    MOCKER_CPP(&RdmaTransportManager::RaInit, bool(*)(uint32_t)).stubs().will(returnValue(false));
    ret = manager.PrepareOpenDevice(1, 1, deviceIp, rdmaHandle);
    EXPECT_EQ(ret, false);

    ret = manager.OpenTsd(1, 1);
    EXPECT_EQ(ret, true);

    GlobalMockObject::verify();
    MOCKER_CPP(&DlHccpApi::RaRdevGetHandle, int(*)(uint32_t, void *&)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaGetIfNum, int(*)(const HccpRaGetIfAttr &, uint32_t &)).stubs().will(returnValue(-1));
    ret = manager.PrepareOpenDevice(1, 1, deviceIp, g_ptr);
    EXPECT_EQ(ret, false);
}

TEST_F(HybmRdmaTransportManagerTest, CheckPrepareOptionsErrorBranchTest)
{
    int ret = 0;
    RdmaTransportManager manager;
    TransportOptions options;
    options.role = HYBM_ROLE_SENDER;
    options.nic = "90";
    HybmTransPrepareOptions hoptions;

    MOCKER(&DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDeviceStub));
    MOCKER_CPP(&RdmaTransportManager::PrepareOpenDevice, bool(*)(uint32_t, uint32_t,
        in_addr &, void *&)).stubs().will(returnValue(false));
    ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_ERROR);
    ret = manager.CheckPrepareOptions(hoptions);
    EXPECT_EQ(ret, BM_OK);

    options.role = HYBM_ROLE_PEER;
    options.rankId = 0;
    options.rankCount = 1;
    TransportRankPrepareInfo prepareInfo;
    hoptions.options.emplace(0, prepareInfo);
    hoptions.options.emplace(1, prepareInfo);
    ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_ERROR);
    ret = manager.CheckPrepareOptions(hoptions);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    hoptions.options.erase(0);
    ret = manager.CheckPrepareOptions(hoptions);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    options.rankCount = 2;
    hoptions.options.erase(1);
    hoptions.options.emplace(0, prepareInfo);
    hoptions.options.emplace(4, prepareInfo);
    ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_ERROR);
    ret = manager.CheckPrepareOptions(hoptions);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    GlobalMockObject::verify();
    MOCKER(&DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDeviceStub));
    MOCKER_CPP(&RdmaTransportManager::PrepareOpenDevice, bool(*)(uint32_t, uint32_t,
        in_addr &, void *&)).stubs().will(returnValue(true));
    MOCKER(&DlAclApi::RtGetDeviceInfo).stubs().will(invoke(RtGetDeviceInfoStub1));
    ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);

    GlobalMockObject::verify();
    MOCKER(&DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDeviceStub));
    MOCKER_CPP(&RdmaTransportManager::PrepareOpenDevice, bool(*)(uint32_t, uint32_t,
        in_addr &, void *&)).stubs().will(returnValue(true));
    MOCKER(&DlAclApi::RtGetDeviceInfo).stubs().will(invoke(RtGetDeviceInfoStub2));
    ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);

    GlobalMockObject::verify();
    MOCKER(&DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDeviceStub));
    MOCKER_CPP(&RdmaTransportManager::PrepareOpenDevice, bool(*)(uint32_t, uint32_t,
        in_addr &, void *&)).stubs().will(returnValue(true));
    MOCKER(&DlAclApi::RtGetDeviceInfo).stubs().will(invoke(RtGetDeviceInfoStub3));
    ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);

    GlobalMockObject::verify();
    MOCKER(&DlAclApi::AclrtGetDevice).stubs().will(invoke(AclrtGetDeviceStub));
    MOCKER_CPP(&RdmaTransportManager::PrepareOpenDevice, bool(*)(uint32_t, uint32_t,
        in_addr &, void *&)).stubs().will(returnValue(true));
    MOCKER_CPP(&DlAclApi::RtGetDeviceInfo, int32_t(*)(uint32_t, int32_t, int32_t,
        int64_t *)).stubs().will(returnValue(0));
    ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);
    ret = manager.OpenDevice(options);
    EXPECT_EQ(ret, BM_OK);
}