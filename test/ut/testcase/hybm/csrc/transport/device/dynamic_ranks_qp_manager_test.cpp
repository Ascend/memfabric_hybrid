/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <chrono>
#include <thread>
#include <algorithm>
#include "hybm_logger.h"
#include "dl_acl_api.h"
#include "dl_hccp_api.h"
#define private public
#include "dynamic_ranks_qp_manager.h"
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
const uint16_t PORT  = 8080;
}
class HybmTransportDynamicRanksQpManagerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}
    void SetUp() override
    {
        g_ptr1 = malloc(1);
    }
    void TearDown() override
    {
        free(g_ptr1);
        GlobalMockObject::verify();
    }
};

static int RaSocketInitStub(HccpNetworkMode mode, const HccpRdev &rdev, void *&socketHandle)
{
    socketHandle = g_ptr1;
    return BM_OK;
}

static int RaGetSocketsStub(uint32_t role, HccpSocketInfo conn[], uint32_t num, uint32_t &connectedNum)
{
    connectedNum   = 1;
    conn[0].status = 1;
    return BM_OK;
}

TEST_F(HybmTransportDynamicRanksQpManagerTest, DynamicRanksQpManagerStartupTest)
{
    void *rdma = malloc(1);
    mf_sockaddr deviceAddr;
    deviceAddr.type = IpV4;
    deviceAddr.ip.ipv4.sin_family = AF_INET;
    deviceAddr.ip.ipv4.sin_port = htons(PORT);
    inet_pton(AF_INET, "0.0.0.0", &deviceAddr.ip.ipv4.sin_addr);
    DynamicRanksQpManager manager(g_deviceId, g_rankId, g_rankCount, deviceAddr, true);

    int ret = manager.Startup(nullptr);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    MOCKER(&DlHccpApi::RaSocketInit).stubs().will(invoke(RaSocketInitStub));
    MOCKER_CPP(&DlHccpApi::RaSocketListenStart, int(*)(HccpSocketListenInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlAclApi::AclrtSetDevice, int32_t(*)(int32_t)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaQpDestroy, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketBatchClose, int(*)(HccpSocketCloseInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketDeinit, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketListenStop, int(*)(HccpSocketListenInfo, uint32_t)).stubs().will(returnValue(0));

    ret = manager.Startup(rdma);
    EXPECT_EQ(ret, BM_OK);

    DynamicRanksQpManager manager_send(g_deviceId, g_rankId, g_rankCount, deviceAddr, false);
    ret = manager_send.Startup(rdma);
    EXPECT_EQ(ret, BM_OK);
    ret = manager_send.Startup(rdma);
    EXPECT_EQ(ret, BM_ERROR);

    manager.Shutdown();
    manager_send.Shutdown();
    free(rdma);
}

TEST_F(HybmTransportDynamicRanksQpManagerTest, DynamicRanksQpManagerSetRemoteRankInfoTest)
{
    void *rdma = malloc(1);
    uint32_t count = 2;
    mf_sockaddr deviceAddr;
    deviceAddr.ip.ipv4.sin_family = AF_INET;
    deviceAddr.ip.ipv4.sin_port = htons(PORT);
    deviceAddr.type = IpV4;
    inet_pton(AF_INET, "0.0.0.0", &deviceAddr.ip.ipv4.sin_addr);
    DynamicRanksQpManager manager(g_deviceId, g_rankId, count, deviceAddr, true);
    uint32_t keys[16] {};
    TransportMemoryKey key;
    std::copy(std::begin(keys), std::end(keys), std::begin(key.keys));
    ConnectRankInfo rankinfo{HYBM_ROLE_BUTT, deviceAddr, key};
    std::unordered_map<uint32_t, ConnectRankInfo> ranks {{0, rankinfo}, {1, rankinfo}};

    int ret = manager.SetRemoteRankInfo(ranks);
    EXPECT_EQ(ret, BM_OK);
    RegMemResult reg{};
    MemoryRegionMap mrs {{0, reg}};
    ret = manager.SetLocalMemories(mrs);
    EXPECT_EQ(ret, BM_OK);

    MOCKER(&DlHccpApi::RaSocketInit).stubs().will(invoke(RaSocketInitStub));
    MOCKER_CPP(&DlHccpApi::RaSocketListenStart, int(*)(HccpSocketListenInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlAclApi::AclrtSetDevice, int32_t(*)(int32_t)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketWhiteListAdd, int(*)(void *, const HccpSocketWhiteListInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketBatchConnect, int(*)(HccpSocketConnectInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER(&DlHccpApi::RaGetSockets).stubs().will(invoke(RaGetSocketsStub));
    MOCKER_CPP(&DlHccpApi::RaQpCreate, int(*)(void *, int, int, void *&))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaQpConnectAsync, int(*)(void *, const void *))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaGetQpStatus, int(*)(void *, int &))
        .stubs().will(returnValue(0));

    ret = manager.Startup(rdma);
    EXPECT_EQ(ret, BM_OK);

    ret = manager.SetRemoteRankInfo(ranks);
    EXPECT_EQ(ret, BM_OK);
    ret = manager.SetLocalMemories(mrs);
    EXPECT_EQ(ret, BM_OK);

    MOCKER_CPP(&DlHccpApi::RaQpDestroy, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketBatchClose, int(*)(HccpSocketCloseInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketDeinit, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketListenStop, int(*)(HccpSocketListenInfo, uint32_t)).stubs().will(returnValue(0));
    free(rdma);
}

TEST_F(HybmTransportDynamicRanksQpManagerTest, SetRemoteRankInfoErrorBranch)
{
    uint32_t hrankid1 = 0;
    uint32_t hrankid2 = 4;
    uint32_t count    = 2;
    mf_sockaddr deviceAddr;
    deviceAddr.ip.ipv4.sin_family = AF_INET;
    deviceAddr.ip.ipv4.sin_port = htons(PORT);
    deviceAddr.type = IpV4;
    inet_pton(AF_INET, "0.0.0.0", &deviceAddr.ip.ipv4.sin_addr);
    DynamicRanksQpManager manager(g_deviceId, g_rankId, count, deviceAddr, true);

    EXPECT_EQ((manager.GetQpHandleWithRankId(hrankid2) == nullptr), 1);
    EXPECT_EQ((manager.GetQpHandleWithRankId(hrankid1) == nullptr), 1);

    uint32_t keys[16] {};
    TransportMemoryKey key;
    std::copy(std::begin(keys), std::end(keys), std::begin(key.keys));
    ConnectRankInfo rankinfo{HYBM_ROLE_RECEIVER, deviceAddr, key};
    std::unordered_map<uint32_t, ConnectRankInfo> ranks {{0, rankinfo}};
    int ret = manager.SetRemoteRankInfo(ranks);
    EXPECT_EQ(ret, BM_OK);

    ConnectRankInfo rankinfo2{HYBM_ROLE_BUTT, deviceAddr, key};
    std::unordered_map<uint32_t, ConnectRankInfo> ranks2 {{10, rankinfo2}};
    ret = manager.SetRemoteRankInfo(ranks2);
    EXPECT_EQ(ret, BM_ERROR);
    
    MOCKER_CPP(&DlHccpApi::RaQpDestroy, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketBatchClose, int(*)(HccpSocketCloseInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketDeinit, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketListenStop, int(*)(HccpSocketListenInfo, uint32_t)).stubs().will(returnValue(0));
}

TEST_F(HybmTransportDynamicRanksQpManagerTest, ProcessClientConnectSocketTaskTest)
{
    mf_sockaddr deviceAddr;
    deviceAddr.type = IpV4;
    deviceAddr.ip.ipv4.sin_family = AF_INET;
    deviceAddr.ip.ipv4.sin_port = htons(PORT);
    inet_pton(AF_INET, "0.0.0.0", &deviceAddr.ip.ipv4.sin_addr);
    DynamicRanksQpManager manager(g_deviceId, g_rankId, g_rankCount, deviceAddr, false);
    
    std::unordered_map<uint32_t, mf_sockaddr> addedRanks{};
    std::unordered_set<uint32_t> addMrRanks{};
    manager.GenTaskFromChangeRanks(addedRanks, addMrRanks);
    int ret = manager.ProcessClientConnectSocketTask();
    EXPECT_EQ(ret, 0);

    MOCKER_CPP(&DlHccpApi::RaSocketInit, int(*)(HccpNetworkMode, const HccpRdev &, void *&))
        .stubs().will(returnValue(0));

    addedRanks.emplace(0, deviceAddr);
    manager.GenTaskFromChangeRanks(addedRanks, addMrRanks);
    ret = manager.ProcessClientConnectSocketTask();
    EXPECT_EQ(ret, 1);

    GlobalMockObject::verify();
    MOCKER(&DlHccpApi::RaSocketInit).stubs().will(invoke(RaSocketInitStub));
    MOCKER_CPP(&DlHccpApi::RaSocketBatchConnect, int(*)(HccpSocketConnectInfo, uint32_t))
        .stubs().will(returnValue(0));

    manager.GenTaskFromChangeRanks(addedRanks, addMrRanks);
    ret = manager.ProcessClientConnectSocketTask();
    EXPECT_EQ(ret, 0);

    ret = manager.ProcessClientConnectSocketTask();
    EXPECT_EQ(ret, 0);

    GlobalMockObject::verify();
    MOCKER(&DlHccpApi::RaSocketInit).stubs().will(invoke(RaSocketInitStub));
    MOCKER_CPP(&DlHccpApi::RaSocketBatchConnect, int(*)(HccpSocketConnectInfo, uint32_t))
        .stubs().will(returnValue(1));

    manager.GenTaskFromChangeRanks(addedRanks, addMrRanks);
    ret = manager.ProcessClientConnectSocketTask();
    EXPECT_EQ(ret, 1);

    MOCKER_CPP(&DlHccpApi::RaQpDestroy, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketBatchClose, int(*)(HccpSocketCloseInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketDeinit, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketListenStop, int(*)(HccpSocketListenInfo, uint32_t)).stubs().will(returnValue(0));
}

TEST_F(HybmTransportDynamicRanksQpManagerTest, ProcessUpdateRemoteMrTaskTest)
{
    mf_sockaddr deviceAddr;
    deviceAddr.type = IpV4;
    deviceAddr.ip.ipv4.sin_family = AF_INET;
    deviceAddr.ip.ipv4.sin_port = htons(PORT);
    inet_pton(AF_INET, "0.0.0.0", &deviceAddr.ip.ipv4.sin_addr);
    DynamicRanksQpManager manager(g_deviceId, g_rankId, g_rankCount, deviceAddr, true);
    std::unordered_map<uint32_t, mf_sockaddr> addedRanks{};
    std::unordered_set<uint32_t> addMrRanks{0};
    manager.GenTaskFromChangeRanks(addedRanks, addMrRanks);
    manager.ProcessUpdateRemoteMrTask();
    
    DynamicRanksQpManager manager2(g_deviceId, g_rankId, g_rankCount, deviceAddr, false);
    MOCKER(&DlHccpApi::RaSocketInit).stubs().will(invoke(RaSocketInitStub));
    MOCKER_CPP(&DlHccpApi::RaSocketBatchConnect, int(*)(HccpSocketConnectInfo, uint32_t))
        .stubs().will(returnValue(0));

    addedRanks.emplace(0, deviceAddr);
    manager2.GenTaskFromChangeRanks(addedRanks, addMrRanks);
    int ret = manager2.ProcessClientConnectSocketTask();
    EXPECT_EQ(ret, 0);
    manager.ProcessUpdateRemoteMrTask();

    MOCKER_CPP(&DlHccpApi::RaQpDestroy, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketBatchClose, int(*)(HccpSocketCloseInfo, uint32_t))
        .stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketDeinit, int(*)(void *)).stubs().will(returnValue(0));
    MOCKER_CPP(&DlHccpApi::RaSocketListenStop, int(*)(HccpSocketListenInfo, uint32_t)).stubs().will(returnValue(0));
}