/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <chrono>
#include "hybm_logger.h"
#include "dl_acl_api.h"
#include "dl_hccp_api.h"
#define private public
#include "fixed_ranks_qp_manager.h"
#undef private

using namespace ock;
using namespace ock::mf;
using namespace ock::mf::transport;
using namespace ock::mf::transport::device;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
namespace {
uint32_t g_deviceId  = 0;
uint32_t g_rankId    = 0;
uint32_t g_rankCount = 1;
void *g_ptr1         = nullptr;
void *g_ptr2         = nullptr;
const uint16_t PORT  = 8080;
}

class HybmTransportFixedRanksQpManagerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}
    void SetUp() override
    {
        g_ptr1 = malloc(1);
        g_ptr2 = malloc(sizeof(AiQpRMAQueueInfo));
    }
    void TearDown() override
    {
        free(g_ptr1);
        free(g_ptr2);
        GlobalMockObject::verify();
    }
};

static int32_t AclrtMallocStub(void **ptr, size_t count, uint32_t type)
{
    *ptr = g_ptr2;
    return BM_OK;
}

static int RaSocketInitStub(HccpNetworkMode mode, const HccpRdev &rdev, void *&socketHandle)
{
    socketHandle = g_ptr1;
    return BM_OK;
}

static int32_t AclrtSetDeviceStub(int32_t deviceId)
{
    return BM_OK;
}

static int RaGetQpStatusStub(void *qpHandle, int &status)
{
    status = 1;
    return BM_OK;
}

static int RaGetSocketsStub(uint32_t role, HccpSocketInfo conn[], uint32_t num, uint32_t &connectedNum)
{
    connectedNum = 1;
    return BM_OK;
}

TEST_F(HybmTransportFixedRanksQpManagerTest, FixedRanksQpManagerStartupTest)
{
    mf_sockaddr deviceAddr;
    deviceAddr.type = IpV4;
    deviceAddr.ip.ipv4.sin_family = AF_INET;
    deviceAddr.ip.ipv4.sin_port = htons(PORT);
    inet_pton(AF_INET, "0.0.0.0", &deviceAddr.ip.ipv4.sin_addr);
    FixedRanksQpManager manager(g_deviceId, g_rankId, g_rankCount, deviceAddr);
    uint32_t keys[16] {};
    TransportMemoryKey key;
    std::copy(std::begin(keys), std::end(keys), std::begin(key.keys));
    ConnectRankInfo rankinfo{HYBM_ROLE_BUTT, deviceAddr, key};
    std::unordered_map<uint32_t, ConnectRankInfo> ranks {{0, rankinfo}};
    int ret = manager.SetRemoteRankInfo(ranks);
    EXPECT_EQ(ret, BM_OK);   // currentRanksInfo_ = ranks

    RegMemResult reg{};
    MemoryRegionMap mrs {{0, reg}};
    ret = manager.SetLocalMemories(mrs);
    EXPECT_EQ(ret, BM_OK);    // currentLocalMrs_ = mrs

    ret = manager.Startup(nullptr);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    MOCKER(&DlAclApi::AclrtMalloc).stubs().will(invoke(AclrtMallocStub));
    void *rdma = malloc(1);
    ret = manager.Startup(rdma);
    EXPECT_EQ(ret, BM_OK);

    ret = manager.Startup(rdma);
    EXPECT_EQ(ret, BM_ERROR);
    ret = manager.SetRemoteRankInfo(ranks);
    EXPECT_EQ(ret, BM_ERROR);
    ret = manager.SetLocalMemories(mrs);
    EXPECT_EQ(ret, BM_OK);
    manager.Shutdown();
    free(rdma);
}

TEST_F(HybmTransportFixedRanksQpManagerTest, FixedRanksQpManagerStartupReturnError)
{
    mf_sockaddr deviceAddr;
    deviceAddr.type = IpV4;
    deviceAddr.ip.ipv4.sin_family = AF_INET;
    deviceAddr.ip.ipv4.sin_port = htons(PORT);
    inet_pton(AF_INET, "0.0.0.0", &deviceAddr.ip.ipv4.sin_addr);
    FixedRanksQpManager manager(g_deviceId, g_rankId, g_rankCount, deviceAddr);
    void *rdma = malloc(1);

    MOCKER_CPP(&DlAclApi::AclrtMalloc, int32_t(*)(void **, size_t, uint32_t))
        .stubs().will(returnValue(-1));
    int ret = manager.Startup(rdma);
    EXPECT_EQ(ret, BM_ERROR);
    GlobalMockObject::verify();

    MOCKER_CPP(&DlAclApi::AclrtMalloc, int32_t(*)(void **, size_t, uint32_t))
        .stubs().will(returnValue(0));
    ret = manager.Startup(rdma);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    uint32_t keys[16] {};
    TransportMemoryKey key;
    std::copy(std::begin(keys), std::end(keys), std::begin(key.keys));
    ConnectRankInfo rankinfo{HYBM_ROLE_BUTT, deviceAddr, key};
    std::unordered_map<uint32_t, ConnectRankInfo> ranks {{10, rankinfo}};
    ret = manager.SetRemoteRankInfo(ranks);
    EXPECT_EQ(ret, BM_OK);
    ret = manager.Startup(rdma);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    std::unordered_map<uint32_t, ConnectRankInfo> ranks2 {{0, rankinfo}};
    ret = manager.SetRemoteRankInfo(ranks2);
    EXPECT_EQ(ret, BM_OK);
    MOCKER_CPP(&FixedRanksQpManager::StartClientSide, int(*)()).stubs().will(returnValue(-1));
    ret = manager.Startup(rdma);
    EXPECT_EQ(ret, BM_ERROR);

    MOCKER_CPP(&FixedRanksQpManager::StartServerSide, int(*)()).stubs().will(returnValue(-1));
    ret = manager.Startup(rdma);
    EXPECT_EQ(ret, BM_ERROR);
    manager.Shutdown();
    free(rdma);
}

TEST_F(HybmTransportFixedRanksQpManagerTest, WaitingConnectionReadyTest)
{
    uint32_t count    = 4;
    uint32_t rankid   = 1;
    uint32_t hrankid1 = 0;
    uint32_t hrankid2 = 2;
    mf_sockaddr deviceAddr;
    deviceAddr.type = IpV4;
    deviceAddr.ip.ipv4.sin_family = AF_INET;
    deviceAddr.ip.ipv4.sin_port = htons(PORT);
    inet_pton(AF_INET, "0.0.0.0", &deviceAddr.ip.ipv4.sin_addr);
    FixedRanksQpManager manager(g_deviceId, rankid, count, deviceAddr);
    uint32_t keys[16] {};
    TransportMemoryKey key;
    std::copy(std::begin(keys), std::end(keys), std::begin(key.keys));
    ConnectRankInfo rankinfo{HYBM_ROLE_BUTT, deviceAddr, key};
    std::unordered_map<uint32_t, ConnectRankInfo> ranks {{0, rankinfo}, {2, rankinfo}};
    int ret = manager.SetRemoteRankInfo(ranks);
    EXPECT_EQ(ret, BM_OK);
    ret = manager.WaitingConnectionReady();
    EXPECT_EQ(ret, BM_ERROR);
    EXPECT_EQ((manager.GetQpHandleWithRankId(hrankid1) == nullptr), 1);
    EXPECT_EQ((manager.GetQpHandleWithRankId(hrankid2) == nullptr), 1);

    MOCKER_CPP(&DlHccpApi::RaSocketWhiteListAdd, int(*)(void *, const HccpSocketWhiteListInfo, uint32_t))
        .stubs().will(returnValue(0));
    ret = manager.GenerateWhiteList();
    EXPECT_EQ(ret, BM_OK);

    MOCKER(&DlHccpApi::RaSocketInit).stubs().will(invoke(RaSocketInitStub));
    MOCKER_CPP(&DlHccpApi::RaSocketBatchConnect, int(*)(HccpSocketConnectInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER(&DlAclApi::AclrtSetDevice).stubs().will(invoke(AclrtSetDeviceStub));
    MOCKER_CPP(&FixedRanksQpManager::WaitConnectionsReady, int(*)(std::unordered_map<uint32_t,
        FixedRanksQpManager::ConnectionChannel> &)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaQpAiCreate, int(*)(void *, const HccpQpExtAttrs &, HccpAiQpInfo &i, void *&))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaQpCreate, int(*)(void *, int, int, void *&))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaMrReg, int(*)(void *, HccpMrInfo &)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaQpConnectAsync, int(*)(void *, const void *)).stubs().will(returnValue(0));
    MOCKER(&DlHccpApi::RaGetQpStatus).stubs().will(invoke(RaGetQpStatusStub));
    MOCKER(&DlAclApi::AclrtMalloc).stubs().will(invoke(AclrtMallocStub));
    MOCKER_CPP(&DlAclApi::AclrtMemcpy, int32_t(*)(void *, size_t, const void *, size_t, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaQpDestroy, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketBatchClose, int(*)(HccpSocketCloseInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketDeinit, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketListenStop, int(*)(HccpSocketListenInfo, uint32_t)).stubs().will(returnValue(0));

    RegMemResult reg{};
    MemoryRegionMap mrs {{0, reg}};
    ret = manager.SetLocalMemories(mrs);
    EXPECT_EQ(ret, BM_OK);
    manager.ReserveQpInfoSpace();
    ret = manager.StartClientSide();
    EXPECT_EQ(ret, BM_OK);
    
    EXPECT_EQ((manager.GetQpHandleWithRankId(0) == nullptr), 1);
    EXPECT_EQ((manager.GetQpHandleWithRankId(2) == nullptr), 1);
}

TEST_F(HybmTransportFixedRanksQpManagerTest, StartSideTest)
{
    mf_sockaddr deviceAddr;
    deviceAddr.type = IpV4;
    deviceAddr.ip.ipv4.sin_family = AF_INET;
    deviceAddr.ip.ipv4.sin_port = htons(PORT);
    inet_pton(AF_INET, "0.0.0.0", &deviceAddr.ip.ipv4.sin_addr);
    uint32_t id    = 1;
    uint32_t count = 3;
    FixedRanksQpManager manager(g_deviceId, id, count, deviceAddr);
    MOCKER(&DlHccpApi::RaSocketInit).stubs().will(invoke(RaSocketInitStub));
    MOCKER_CPP(&DlHccpApi::RaSocketListenStart, int(*)(HccpSocketListenInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketWhiteListAdd, int(*)(void *, const HccpSocketWhiteListInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&FixedRanksQpManager::WaitConnectionsReady, int(*)(std::unordered_map<uint32_t,
        FixedRanksQpManager::ConnectionChannel> &)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlAclApi::AclrtMemcpy, int32_t(*)(void *, size_t, const void *, size_t, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER(&DlAclApi::AclrtSetDevice).stubs().will(invoke(AclrtSetDeviceStub));
    MOCKER(&DlAclApi::AclrtMalloc).stubs().will(invoke(AclrtMallocStub));
    MOCKER_CPP(&DlHccpApi::RaQpDestroy, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketBatchClose, int(*)(HccpSocketCloseInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketDeinit, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketListenStop, int(*)(HccpSocketListenInfo, uint32_t)).stubs().will(returnValue(0));
    
    manager.ReserveQpInfoSpace();
    int ret = manager.StartServerSide();
    EXPECT_EQ(ret, BM_OK);

    ret = manager.WaitingConnectionReady();
    EXPECT_EQ(ret, BM_ERROR);

    MOCKER_CPP(&DlHccpApi::RaSocketBatchConnect, int(*)(HccpSocketConnectInfo, uint32_t))
        .stubs().will(returnValue(0));
    ret = manager.StartClientSide();
    EXPECT_EQ(ret, BM_OK);

    ret = manager.WaitingConnectionReady();
    EXPECT_EQ(ret, BM_OK);

    manager.CloseServices();
}

TEST_F(HybmTransportFixedRanksQpManagerTest, WaitConnectionsReadyTest)
{
    uint32_t count  = 3;
    uint32_t rankid = 1;
    mf_sockaddr deviceAddr;
    deviceAddr.type = IpV4;
    deviceAddr.ip.ipv4.sin_family = AF_INET;
    deviceAddr.ip.ipv4.sin_port = htons(PORT);
    inet_pton(AF_INET, "0.0.0.0", &deviceAddr.ip.ipv4.sin_addr);
    FixedRanksQpManager manager(g_deviceId, rankid, count, deviceAddr);
    net_addr_t ipConnection;
    ipConnection.type = IpV4;
    ipConnection.ip.ipv4 = deviceAddr.ip.ipv4.sin_addr;
    std::unordered_map<uint32_t, FixedRanksQpManager::ConnectionChannel> connections;
    connections.emplace(0, FixedRanksQpManager::ConnectionChannel(ipConnection, nullptr));

    MOCKER(&DlHccpApi::RaGetSockets).stubs().will(invoke(RaGetSocketsStub));
    int ret = manager.WaitConnectionsReady(connections);
    EXPECT_EQ(ret, BM_OK);
}