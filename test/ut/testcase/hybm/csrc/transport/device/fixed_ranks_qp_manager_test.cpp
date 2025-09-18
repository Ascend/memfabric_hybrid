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
void *g_ptr1         = malloc(1);
void *g_ptr2         = malloc(sizeof(AiQpRMAQueueInfo));
}

class HybmTransportFixedRanksQpManagerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {}
    static void TearDownTestSuite()
    {
        free(g_ptr1);
        free(g_ptr2);
    }
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

int32_t AclrtMallocStub(void **ptr, size_t count, uint32_t type)
{
    *ptr = g_ptr2;
    return BM_OK;
}

int RaSocketInitStub(HccpNetworkMode mode, const HccpRdev &rdev, void *&socketHandle)
{
    socketHandle = g_ptr1;
    return BM_OK;
}

int32_t AclrtSetDeviceStub(int32_t deviceId)
{
    return BM_OK;
}

TEST_F(HybmTransportFixedRanksQpManagerTest, FixedRanksQpManagerStartupTest)
{
    sockaddr_in deviceAddr;
    deviceAddr.sin_family = AF_INET;
    deviceAddr.sin_port = htons(8080);
    inet_pton(AF_INET, "0.0.0.0", &deviceAddr.sin_addr);
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
    sockaddr_in deviceAddr;
    deviceAddr.sin_family = AF_INET;
    deviceAddr.sin_port = htons(8080);
    inet_pton(AF_INET, "0.0.0.0", &deviceAddr.sin_addr);
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
    sockaddr_in deviceAddr;
    deviceAddr.sin_family = AF_INET;
    deviceAddr.sin_port = htons(8080);
    inet_pton(AF_INET, "0.0.0.0", &deviceAddr.sin_addr);
    FixedRanksQpManager manager(g_deviceId, g_rankId, g_rankCount, deviceAddr);

    int ret = manager.WaitingConnectionReady();
    EXPECT_EQ(ret, BM_ERROR);
    EXPECT_EQ((manager.GetQpHandleWithRankId(0) == nullptr), 1);
    EXPECT_EQ((manager.GetQpHandleWithRankId(1) == nullptr), 1);
}

TEST_F(HybmTransportFixedRanksQpManagerTest, StartSideTest)
{
    sockaddr_in deviceAddr;
    deviceAddr.sin_family = AF_INET;
    deviceAddr.sin_port = htons(8080);
    inet_pton(AF_INET, "0.0.0.0", &deviceAddr.sin_addr);
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