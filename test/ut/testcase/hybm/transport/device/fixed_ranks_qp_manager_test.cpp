/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
* MemFabric_Hybrid is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#include <gtest/gtest.h>

#include "hybm_types.h"

#define private public
#define protected public
#include "device/fixed_ranks_qp_manager.h"
#include "dl_acl_api.h"
#include "dl_hccp_api.h"
#undef private
#undef protected

using namespace ock::mf;
using namespace ock::mf::transport;
using namespace ock::mf::transport::device;

namespace {
struct DlAclApiFnGuard {
    aclrtMallocFunc oldAclrtMalloc{DlAclApi::pAclrtMalloc};
    aclrtFreeFunc oldAclrtFree{DlAclApi::pAclrtFree};
    aclrtMemcpyFunc oldAclrtMemcpy{DlAclApi::pAclrtMemcpy};
    aclrtSetDeviceFunc oldAclrtSetDevice{DlAclApi::pAclrtSetDevice};

    ~DlAclApiFnGuard()
    {
        DlAclApi::pAclrtMalloc = oldAclrtMalloc;
        DlAclApi::pAclrtFree = oldAclrtFree;
        DlAclApi::pAclrtMemcpy = oldAclrtMemcpy;
        DlAclApi::pAclrtSetDevice = oldAclrtSetDevice;
    }
};

struct DlHccpApiFnGuard {
    raSocketInitFunc oldRaSocketInit{DlHccpApi::gRaSocketInit};
    raSocketBatchConnectFunc oldRaSocketBatchConnect{DlHccpApi::gRaSocketBatchConnect};
    raSocketWhiteListAddFunc oldRaSocketWhiteListAdd{DlHccpApi::gRaSocketWhiteListAdd};
    raSocketListenStartFunc oldRaSocketListenStart{DlHccpApi::gRaSocketListenStart};
    raSocketListenStopFunc oldRaSocketListenStop{DlHccpApi::gRaSocketListenStop};
    raGetSocketsFunc oldRaGetSockets{DlHccpApi::gRaGetSockets};
    raQpAiCreateFunc oldRaQpAiCreate{DlHccpApi::gRaQpAiCreate};
    raQpConnectAsyncFunc oldRaQpConnectAsync{DlHccpApi::gRaQpConnectAsync};
    raGetQpStatusFunc oldRaGetQpStatus{DlHccpApi::gRaGetQpStatus};
    raQpDestroyFunc oldRaQpDestroy{DlHccpApi::gRaQpDestroy};
    raSocketBatchCloseFunc oldRaSocketBatchClose{DlHccpApi::gRaSocketBatchClose};
    raSocketDeinitFunc oldRaSocketDeinit{DlHccpApi::gRaSocketDeinit};

    ~DlHccpApiFnGuard()
    {
        DlHccpApi::gRaSocketInit = oldRaSocketInit;
        DlHccpApi::gRaSocketBatchConnect = oldRaSocketBatchConnect;
        DlHccpApi::gRaSocketWhiteListAdd = oldRaSocketWhiteListAdd;
        DlHccpApi::gRaSocketListenStart = oldRaSocketListenStart;
        DlHccpApi::gRaSocketListenStop = oldRaSocketListenStop;
        DlHccpApi::gRaGetSockets = oldRaGetSockets;
        DlHccpApi::gRaQpAiCreate = oldRaQpAiCreate;
        DlHccpApi::gRaQpConnectAsync = oldRaQpConnectAsync;
        DlHccpApi::gRaGetQpStatus = oldRaGetQpStatus;
        DlHccpApi::gRaQpDestroy = oldRaQpDestroy;
        DlHccpApi::gRaSocketBatchClose = oldRaSocketBatchClose;
        DlHccpApi::gRaSocketDeinit = oldRaSocketDeinit;
    }
};

int FakeRaSocketInitOk(HccpNetworkMode mode, HccpRdev rdev, void** socketHandle)
{
    (void)mode;
    (void)rdev;
    if (socketHandle == nullptr) {
        return -1;
    }
    *socketHandle = reinterpret_cast<void*>(0xCAFEUL);
    return 0;
}

int FakeRaSocketInitFail(HccpNetworkMode mode, HccpRdev rdev, void** socketHandle)
{
    (void)mode;
    (void)rdev;
    if (socketHandle != nullptr) {
        *socketHandle = nullptr;
    }
    return -1;
}

int FakeRaSocketListenStartOk(HccpSocketListenInfo infos[], uint32_t count)
{
    (void)infos;
    (void)count;
    return 0;
}

int FakeRaSocketListenStartFail(HccpSocketListenInfo infos[], uint32_t count)
{
    (void)infos;
    (void)count;
    return -1;
}

int FakeRaSocketListenStopOk(HccpSocketListenInfo infos[], uint32_t count)
{
    (void)infos;
    (void)count;
    return 0;
}

int32_t FakeAclrtMallocOk(void** ptr, size_t size, uint32_t flags)
{
    (void)size;
    (void)flags;
    *ptr = reinterpret_cast<void*>(0x1234UL);
    return 0;
}

int32_t FakeAclrtMallocFail(void** ptr, size_t size, uint32_t flags)
{
    (void)size;
    (void)flags;
    *ptr = nullptr;
    return -1;
}

int FakeAclrtFreeOk(void* ptr)
{
    (void)ptr;
    return 0;
}

int32_t FakeAclrtMemcpyOk(void* dst, size_t dstSize, const void* src, size_t srcSize, uint32_t kind)
{
    (void)dst;
    (void)dstSize;
    (void)src;
    (void)srcSize;
    (void)kind;
    return 0;
}

int FakeAclrtSetDeviceOk(uint32_t deviceId)
{
    (void)deviceId;
    return 0;
}

int FakeRaSocketBatchConnectOk(HccpSocketConnectInfo infos[], uint32_t count)
{
    (void)infos;
    (void)count;
    return 0;
}

int FakeRaSocketBatchConnectFail(HccpSocketConnectInfo infos[], uint32_t count)
{
    (void)infos;
    (void)count;
    return -1;
}

int FakeRaSocketWhiteListAddOk(void* handle, const HccpSocketWhiteListInfo* infos, uint32_t count)
{
    (void)handle;
    (void)infos;
    (void)count;
    return 0;
}

int FakeRaSocketWhiteListAddFail(void* handle, const HccpSocketWhiteListInfo* infos, uint32_t count)
{
    (void)handle;
    (void)infos;
    (void)count;
    return -1;
}

int FakeRaGetSocketsOk(uint32_t role, HccpSocketInfo infos[], uint32_t count, uint32_t* successCount)
{
    (void)role;
    if (successCount == nullptr) {
        return -1;
    }
    *successCount = count;
    for (uint32_t i = 0; i < count; i++) {
        infos[i].fd = reinterpret_cast<void*>(0x5678UL + i);
        infos[i].status = 0;
    }
    return 0;
}

int FakeRaGetSocketsFail(uint32_t role, HccpSocketInfo infos[], uint32_t count, uint32_t* successCount)
{
    (void)role;
    (void)infos;
    (void)count;
    if (successCount != nullptr) {
        *successCount = 0;
    }
    return -1;
}

int FakeRaQpAiCreateOk(void* rdmaHandle, const HccpQpExtAttrs* attr, HccpAiQpInfo* aiQpInfo, void** qpHandle)
{
    (void)rdmaHandle;
    (void)attr;
    if (qpHandle == nullptr || aiQpInfo == nullptr) {
        return -1;
    }
    *qpHandle = reinterpret_cast<void*>(0x9ABCUL);
    return 0;
}

int FakeRaQpAiCreateFail(void* rdmaHandle, const HccpQpExtAttrs* attr, HccpAiQpInfo* aiQpInfo, void** qpHandle)
{
    (void)rdmaHandle;
    (void)attr;
    (void)aiQpInfo;
    if (qpHandle != nullptr) {
        *qpHandle = nullptr;
    }
    return -1;
}

int FakeRaQpConnectAsyncOk(void* qpHandle, const void* socketFd)
{
    (void)qpHandle;
    (void)socketFd;
    return 0;
}

int FakeRaQpConnectAsyncFail(void* qpHandle, const void* socketFd)
{
    (void)qpHandle;
    (void)socketFd;
    return -1;
}

int FakeRaGetQpStatusReady(void* qpHandle, int* status)
{
    (void)qpHandle;
    if (status == nullptr) {
        return -1;
    }
    *status = 1;  // ready
    return 0;
}

int FakeRaGetQpStatusNotReady(void* qpHandle, int* status)
{
    (void)qpHandle;
    if (status == nullptr) {
        return -1;
    }
    *status = 0;  // not ready
    return 0;
}

int FakeRaQpDestroyOk(void* qpHandle)
{
    (void)qpHandle;
    return 0;
}

int FakeRaQpDestroyFail(void* qpHandle)
{
    (void)qpHandle;
    return -1;
}

int FakeRaSocketBatchCloseOk(HccpSocketCloseInfo infos[], uint32_t count)
{
    (void)infos;
    (void)count;
    return 0;
}

int FakeRaSocketBatchCloseFail(HccpSocketCloseInfo infos[], uint32_t count)
{
    (void)infos;
    (void)count;
    return -1;
}

int FakeRaSocketDeinitOk(void* handle)
{
    (void)handle;
    return 0;
}

int FakeRaSocketDeinitFail(void* handle)
{
    (void)handle;
    return -1;
}

class FixedRanksQpManagerTest : public FixedRanksQpManager {
public:
    FixedRanksQpManagerTest(uint32_t deviceId, uint32_t rankId, uint32_t rankCount, sockaddr_in devNet)
        : FixedRanksQpManager(deviceId, rankId, rankCount, devNet)
    {
    }

    // Expose protected methods for testing
    using FixedRanksQpManager::ReserveQpInfoSpace;
    using FixedRanksQpManager::ReleaseQpInfoSpace;
    using FixedRanksQpManager::StartServerSide;
    using FixedRanksQpManager::StartClientSide;
    using FixedRanksQpManager::GenerateWhiteList;
    using FixedRanksQpManager::WaitConnectionsReady;
    using FixedRanksQpManager::CheckReadyConnection;
    using FixedRanksQpManager::CreateQpWaitingReady;
    using FixedRanksQpManager::CreateOneQp;
    using FixedRanksQpManager::FillQpInfo;
    using FixedRanksQpManager::CopyAiWQInfo;
    using FixedRanksQpManager::CopyAiCQInfo;
    using FixedRanksQpManager::CloseServices;
    using FixedRanksQpManager::CloseClientConnections;
    using FixedRanksQpManager::CloseServerConnections;
    using FixedRanksQpManager::CloseConnections;
};

}  // namespace

TEST(FixedRanksQpManagerTestTest, ConstructorAndDestructor)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    // Test constructor
    FixedRanksQpManagerTest manager(0, 0, 1, devNet);
    EXPECT_EQ(manager.deviceId_, 0U);
    EXPECT_EQ(manager.rankId_, 0U);
    EXPECT_EQ(manager.rankCount_, 1U);
    EXPECT_FALSE(manager.started_.load());
    EXPECT_EQ(manager.serverConnectResult, -1);
    EXPECT_EQ(manager.clientConnectResult, -1);
    EXPECT_EQ(manager.qpInfoSize_, 0U);
    EXPECT_EQ(manager.rdmaHandle_, nullptr);
    EXPECT_EQ(manager.qpInfo_, nullptr);
    EXPECT_EQ(manager.clientConnectThread_, nullptr);
    EXPECT_EQ(manager.serverConnectThread_, nullptr);
}

TEST(FixedRanksQpManagerTestTest, SetRemoteRankInfoSuccess)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    // Prepare test data
    std::unordered_map<uint32_t, ConnectRankInfo> ranks;
    in_addr ip1, ip2;
    inet_aton("192.168.1.1", &ip1);
    inet_aton("192.168.1.2", &ip2);
    sockaddr_in net1, net2;
    net1.sin_addr = ip1;
    net1.sin_port = htons(12345);
    net1.sin_family = AF_INET;
    net2.sin_addr = ip2;
    net2.sin_port = htons(12346);
    net2.sin_family = AF_INET;
    ranks.emplace(0, ConnectRankInfo(HYBM_ROLE_PEER, net1, std::vector<TransportMemoryKey>{}));
    ranks.emplace(1, ConnectRankInfo(HYBM_ROLE_PEER, net2, std::vector<TransportMemoryKey>{}));

    // Test success case
    EXPECT_EQ(manager.SetRemoteRankInfo(ranks), BM_OK);
    EXPECT_EQ(manager.currentRanksInfo_.size(), 2U);
}

TEST(FixedRanksQpManagerTestTest, SetRemoteRankInfoFailWhenStarted)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);
    manager.started_.store(true);

    std::unordered_map<uint32_t, ConnectRankInfo> ranks;
    in_addr ip;
    inet_aton("192.168.1.1", &ip);
    sockaddr_in net;
    net.sin_addr = ip;
    net.sin_port = htons(12345);
    net.sin_family = AF_INET;
    ranks.emplace(0, ConnectRankInfo(HYBM_ROLE_PEER, net, std::vector<TransportMemoryKey>{}));

    // Test failure case when already started
    EXPECT_EQ(manager.SetRemoteRankInfo(ranks), BM_ERROR);
}

TEST(FixedRanksQpManagerTestTest, ReserveQpInfoSpaceSuccess)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    DlAclApiFnGuard guard;
    DlAclApi::pAclrtMalloc = &FakeAclrtMallocOk;

    // Test success case
    EXPECT_TRUE(manager.ReserveQpInfoSpace());
    EXPECT_NE(manager.qpInfo_, nullptr);
    EXPECT_GT(manager.qpInfoSize_, 0U);
}

TEST(FixedRanksQpManagerTestTest, ReserveQpInfoSpaceFail)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    DlAclApiFnGuard guard;
    DlAclApi::pAclrtMalloc = &FakeAclrtMallocFail;

    // Test failure case
    EXPECT_FALSE(manager.ReserveQpInfoSpace());
    EXPECT_EQ(manager.qpInfo_, nullptr);
    // qpInfoSize_ is computed before malloc; failure should not allocate qpInfo_.
    EXPECT_GT(manager.qpInfoSize_, 0U);
}

TEST(FixedRanksQpManagerTestTest, ReleaseQpInfoSpace)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    DlAclApiFnGuard guard;
    DlAclApi::pAclrtMalloc = &FakeAclrtMallocOk;
    DlAclApi::pAclrtFree = &FakeAclrtFreeOk;

    // Allocate first
    manager.ReserveQpInfoSpace();
    EXPECT_NE(manager.qpInfo_, nullptr);

    // Release
    manager.ReleaseQpInfoSpace();
    EXPECT_EQ(manager.qpInfo_, nullptr);
}

TEST(FixedRanksQpManagerTestTest, GetQpInfoAddress)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    DlAclApiFnGuard guard;
    DlAclApi::pAclrtMalloc = &FakeAclrtMallocOk;

    // Test with null qpInfo_
    EXPECT_EQ(manager.GetQpInfoAddress(), nullptr);

    // Test with allocated qpInfo_
    manager.ReserveQpInfoSpace();
    EXPECT_EQ(manager.GetQpInfoAddress(), manager.qpInfo_);
}

TEST(FixedRanksQpManagerTestTest, GetQpHandleWithRankId)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    // This method always returns nullptr
    EXPECT_EQ(manager.GetQpHandleWithRankId(0), nullptr);
    EXPECT_EQ(manager.GetQpHandleWithRankId(1), nullptr);
}

TEST(FixedRanksQpManagerTestTest, PutQpHandle)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    // This method is a no-op, just verify it doesn't crash
    UserQpInfo qp{};
    manager.PutQpHandle(&qp);
    // No assertion needed, just ensure it completes
}

TEST(FixedRanksQpManagerTestTest, Shutdown)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    DlAclApiFnGuard aclGuard;
    DlAclApi::pAclrtMalloc = &FakeAclrtMallocOk;
    DlAclApi::pAclrtFree = &FakeAclrtFreeOk;

    // Allocate resources
    manager.ReserveQpInfoSpace();
    EXPECT_NE(manager.qpInfo_, nullptr);

    // Shutdown
    manager.Shutdown();
    EXPECT_EQ(manager.qpInfo_, nullptr);
}

TEST(FixedRanksQpManagerTestTest, StartupNullRdmaReturnsInvalidParam)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 1, devNet);
    EXPECT_EQ(manager.Startup(nullptr), BM_INVALID_PARAM);
}

TEST(FixedRanksQpManagerTestTest, StartupAlreadyStartedReturnsOkWithoutOverwritingHandle)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 1, devNet);
    manager.started_.store(true);
    EXPECT_EQ(manager.rdmaHandle_, nullptr);

    int dummy = 0;
    EXPECT_EQ(manager.Startup(&dummy), BM_OK);
    EXPECT_EQ(manager.rdmaHandle_, nullptr);
}

TEST(FixedRanksQpManagerTestTest, StartupReserveQpInfoFailReturnsError)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 1, devNet);

    DlAclApiFnGuard guard;
    DlAclApi::pAclrtMalloc = &FakeAclrtMallocFail;
    DlAclApi::pAclrtFree = &FakeAclrtFreeOk;

    std::unordered_map<uint32_t, ConnectRankInfo> ranks;
    in_addr ip;
    inet_aton("192.168.1.1", &ip);
    sockaddr_in net;
    net.sin_addr = ip;
    net.sin_port = htons(12345);
    net.sin_family = AF_INET;
    ranks.emplace(0, ConnectRankInfo(HYBM_ROLE_PEER, net, std::vector<TransportMemoryKey>{}));
    ASSERT_EQ(manager.SetRemoteRankInfo(ranks), BM_OK);

    int dummy = 0;
    EXPECT_EQ(manager.Startup(&dummy), BM_ERROR);
}

TEST(FixedRanksQpManagerTestTest, StartupRankInfoSizeMismatchReturnsInvalidParam)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    // rankCount_ = 2, but currentRanksInfo_ left empty
    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    DlAclApiFnGuard guard;
    DlAclApi::pAclrtMalloc = &FakeAclrtMallocOk;
    DlAclApi::pAclrtFree = &FakeAclrtFreeOk;

    int dummy = 0;
    EXPECT_EQ(manager.Startup(&dummy), BM_INVALID_PARAM);
}

TEST(FixedRanksQpManagerTestTest, StartupRankInfoContainsInvalidRankIdReturnsInvalidParam)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    DlAclApiFnGuard guard;
    DlAclApi::pAclrtMalloc = &FakeAclrtMallocOk;
    DlAclApi::pAclrtFree = &FakeAclrtFreeOk;

    std::unordered_map<uint32_t, ConnectRankInfo> ranks;
    in_addr ip;
    inet_aton("192.168.1.1", &ip);
    sockaddr_in net;
    net.sin_addr = ip;
    net.sin_port = htons(12345);
    net.sin_family = AF_INET;
    // size == rankCount_ (2), but one key is out of range (2)
    ranks.emplace(0, ConnectRankInfo(HYBM_ROLE_PEER, net, std::vector<TransportMemoryKey>{}));
    ranks.emplace(2, ConnectRankInfo(HYBM_ROLE_PEER, net, std::vector<TransportMemoryKey>{}));
    ASSERT_EQ(manager.SetRemoteRankInfo(ranks), BM_OK);

    int dummy = 0;
    EXPECT_EQ(manager.Startup(&dummy), BM_INVALID_PARAM);
}

TEST(FixedRanksQpManagerTestTest, StartupStartServerSideFailPropagates)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    // rankId=0, rankCount=2 -> StartServerSide will try CreateServerSocket()
    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    DlAclApiFnGuard aclGuard;
    DlAclApi::pAclrtMalloc = &FakeAclrtMallocOk;
    DlAclApi::pAclrtFree = &FakeAclrtFreeOk;

    DlHccpApiFnGuard hccpGuard;
    DlHccpApi::gRaSocketInit = &FakeRaSocketInitOk;
    DlHccpApi::gRaSocketListenStart = &FakeRaSocketListenStartFail;
    DlHccpApi::gRaSocketListenStop = &FakeRaSocketListenStopOk;
    DlHccpApi::gRaSocketDeinit = &FakeRaSocketDeinitOk;

    std::unordered_map<uint32_t, ConnectRankInfo> ranks;
    in_addr ip1, ip2;
    inet_aton("192.168.1.1", &ip1);
    inet_aton("192.168.1.2", &ip2);
    sockaddr_in net1, net2;
    net1.sin_addr = ip1;
    net1.sin_port = htons(12345);
    net1.sin_family = AF_INET;
    net2.sin_addr = ip2;
    net2.sin_port = htons(12346);
    net2.sin_family = AF_INET;
    ranks.emplace(0, ConnectRankInfo(HYBM_ROLE_PEER, net1, std::vector<TransportMemoryKey>{}));
    ranks.emplace(1, ConnectRankInfo(HYBM_ROLE_PEER, net2, std::vector<TransportMemoryKey>{}));
    ASSERT_EQ(manager.SetRemoteRankInfo(ranks), BM_OK);

    int dummy = 0;
    EXPECT_EQ(manager.Startup(&dummy), BM_DL_FUNCTION_FAILED);
}

TEST(FixedRanksQpManagerTestTest, StartupStartClientSideFailPropagates)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    // rankId=1, rankCount=2 -> StartServerSide early returns OK, StartClientSide will try CreateLocalSocket()
    FixedRanksQpManagerTest manager(0, 1, 2, devNet);

    DlAclApiFnGuard aclGuard;
    DlAclApi::pAclrtMalloc = &FakeAclrtMallocOk;
    DlAclApi::pAclrtFree = &FakeAclrtFreeOk;

    DlHccpApiFnGuard hccpGuard;
    DlHccpApi::gRaSocketInit = &FakeRaSocketInitFail;

    std::unordered_map<uint32_t, ConnectRankInfo> ranks;
    in_addr ip1, ip2;
    inet_aton("192.168.1.1", &ip1);
    inet_aton("192.168.1.2", &ip2);
    sockaddr_in net1, net2;
    net1.sin_addr = ip1;
    net1.sin_port = htons(12345);
    net1.sin_family = AF_INET;
    net2.sin_addr = ip2;
    net2.sin_port = htons(12346);
    net2.sin_family = AF_INET;
    ranks.emplace(0, ConnectRankInfo(HYBM_ROLE_PEER, net1, std::vector<TransportMemoryKey>{}));
    ranks.emplace(1, ConnectRankInfo(HYBM_ROLE_PEER, net2, std::vector<TransportMemoryKey>{}));
    ASSERT_EQ(manager.SetRemoteRankInfo(ranks), BM_OK);

    int dummy = 0;
    EXPECT_EQ(manager.Startup(&dummy), BM_DL_FUNCTION_FAILED);
}

TEST(FixedRanksQpManagerTestTest, StartupSuccessSetsStartedAndHandle)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    // rankCount=1 ensures StartServerSide/StartClientSide both early return.
    FixedRanksQpManagerTest manager(0, 0, 1, devNet);

    DlAclApiFnGuard guard;
    DlAclApi::pAclrtMalloc = &FakeAclrtMallocOk;
    DlAclApi::pAclrtFree = &FakeAclrtFreeOk;

    std::unordered_map<uint32_t, ConnectRankInfo> ranks;
    in_addr ip;
    inet_aton("192.168.1.1", &ip);
    sockaddr_in net;
    net.sin_addr = ip;
    net.sin_port = htons(12345);
    net.sin_family = AF_INET;
    ranks.emplace(0, ConnectRankInfo(HYBM_ROLE_PEER, net, std::vector<TransportMemoryKey>{}));
    ASSERT_EQ(manager.SetRemoteRankInfo(ranks), BM_OK);

    int dummy = 0;
    EXPECT_EQ(manager.Startup(&dummy), BM_OK);
    EXPECT_TRUE(manager.started_.load());
    EXPECT_EQ(manager.rdmaHandle_, &dummy);
}

TEST(FixedRanksQpManagerTestTest, CopyAiWQInfo)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    // Prepare test data
    struct ai_data_plane_wq src {};
    src.wqn = 123;
    src.buf_addr = 0x1000;
    src.wqebb_size = 64;
    src.depth = 1024;
    src.head_addr = 0x2000;
    src.tail_addr = 0x3000;
    src.swdb_addr = 0x4000;
    src.db_reg = 0x5000;

    struct AiQpRMAWQ dest {};

    // Test with SW_DB
    manager.CopyAiWQInfo(dest, src, DBMode::SW_DB, 4);
    EXPECT_EQ(dest.wqn, src.wqn);
    EXPECT_EQ(dest.bufAddr, src.buf_addr);
    EXPECT_EQ(dest.wqeSize, src.wqebb_size);
    EXPECT_EQ(dest.depth, src.depth);
    EXPECT_EQ(dest.headAddr, src.head_addr);
    EXPECT_EQ(dest.tailAddr, src.tail_addr);
    EXPECT_EQ(dest.dbMode, DBMode::SW_DB);
    EXPECT_EQ(dest.dbAddr, src.swdb_addr);
    EXPECT_EQ(dest.sl, 4U);

    // Test with HW_DB
    manager.CopyAiWQInfo(dest, src, DBMode::HW_DB, 8);
    EXPECT_EQ(dest.dbMode, DBMode::HW_DB);
    EXPECT_EQ(dest.dbAddr, src.db_reg);
    EXPECT_EQ(dest.sl, 8U);
}

TEST(FixedRanksQpManagerTestTest, CopyAiCQInfo)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    // Prepare test data
    ai_data_plane_cq src{};
    src.cqn = 456;
    src.buf_addr = 0x6000;
    src.cqe_size = 128;
    src.depth = 512;
    src.head_addr = 0x7000;
    src.tail_addr = 0x8000;
    src.swdb_addr = 0x9000;
    src.db_reg = 0xA000;

    struct AiQpRMACQ dest {};

    // Test with SW_DB
    manager.CopyAiCQInfo(dest, src, DBMode::SW_DB);
    EXPECT_EQ(dest.cqn, src.cqn);
    EXPECT_EQ(dest.bufAddr, src.buf_addr);
    EXPECT_EQ(dest.cqeSize, src.cqe_size);
    EXPECT_EQ(dest.depth, src.depth);
    EXPECT_EQ(dest.headAddr, src.head_addr);
    EXPECT_EQ(dest.tailAddr, src.tail_addr);
    EXPECT_EQ(dest.dbMode, DBMode::SW_DB);
    EXPECT_EQ(dest.dbAddr, src.swdb_addr);

    // Test with HW_DB
    manager.CopyAiCQInfo(dest, src, DBMode::HW_DB);
    EXPECT_EQ(dest.dbMode, DBMode::HW_DB);
    EXPECT_EQ(dest.dbAddr, src.db_reg);
}

TEST(FixedRanksQpManagerTestTest, GenerateWhiteListSuccess)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 3, devNet);

    // Prepare test data
    std::unordered_map<uint32_t, ConnectRankInfo> ranks;
    in_addr ip0, ip1, ip2;
    inet_aton("192.168.1.1", &ip0);
    inet_aton("192.168.1.2", &ip1);
    inet_aton("192.168.1.3", &ip2);
    sockaddr_in net0, net1, net2;
    net0.sin_addr = ip0;
    net0.sin_port = htons(12345);
    net0.sin_family = AF_INET;
    net1.sin_addr = ip1;
    net1.sin_port = htons(12346);
    net1.sin_family = AF_INET;
    net2.sin_addr = ip2;
    net2.sin_port = htons(12347);
    net2.sin_family = AF_INET;
    ranks.emplace(0, ConnectRankInfo(HYBM_ROLE_PEER, net0, std::vector<TransportMemoryKey>{}));
    ranks.emplace(1, ConnectRankInfo(HYBM_ROLE_PEER, net1, std::vector<TransportMemoryKey>{}));
    ranks.emplace(2, ConnectRankInfo(HYBM_ROLE_PEER, net2, std::vector<TransportMemoryKey>{}));
    manager.currentRanksInfo_ = ranks;

    // Mock server socket handle
    manager.serverSocketHandle_ = reinterpret_cast<void*>(0x1234UL);

    DlHccpApiFnGuard guard;
    DlHccpApi::gRaSocketWhiteListAdd = &FakeRaSocketWhiteListAddOk;

    // Test success case
    EXPECT_EQ(manager.GenerateWhiteList(), BM_OK);
    EXPECT_EQ(manager.serverConnections_.size(), 2U);  // ranks 1 and 2
}

TEST(FixedRanksQpManagerTestTest, GenerateWhiteListEmpty)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 2, 3, devNet);  // rankId is 2, so no servers to connect to

    // Prepare test data
    std::unordered_map<uint32_t, ConnectRankInfo> ranks;
    in_addr ip0, ip1, ip2;
    inet_aton("192.168.1.1", &ip0);
    inet_aton("192.168.1.2", &ip1);
    inet_aton("192.168.1.3", &ip2);
    sockaddr_in net0, net1, net2;
    net0.sin_addr = ip0;
    net0.sin_port = htons(12345);
    net0.sin_family = AF_INET;
    net1.sin_addr = ip1;
    net1.sin_port = htons(12346);
    net1.sin_family = AF_INET;
    net2.sin_addr = ip2;
    net2.sin_port = htons(12347);
    net2.sin_family = AF_INET;
    ranks.emplace(0, ConnectRankInfo(HYBM_ROLE_PEER, net0, std::vector<TransportMemoryKey>{}));
    ranks.emplace(1, ConnectRankInfo(HYBM_ROLE_PEER, net1, std::vector<TransportMemoryKey>{}));
    ranks.emplace(2, ConnectRankInfo(HYBM_ROLE_PEER, net2, std::vector<TransportMemoryKey>{}));
    manager.currentRanksInfo_ = ranks;

    // Test empty whitelist case
    EXPECT_EQ(manager.GenerateWhiteList(), BM_OK);
    EXPECT_TRUE(manager.serverConnections_.empty());
}

TEST(FixedRanksQpManagerTestTest, GenerateWhiteListFail)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 3, devNet);

    // Prepare test data
    std::unordered_map<uint32_t, ConnectRankInfo> ranks;
    in_addr ip0, ip1, ip2;
    inet_aton("192.168.1.1", &ip0);
    inet_aton("192.168.1.2", &ip1);
    inet_aton("192.168.1.3", &ip2);
    sockaddr_in net0, net1, net2;
    net0.sin_addr = ip0;
    net0.sin_port = htons(12345);
    net0.sin_family = AF_INET;
    net1.sin_addr = ip1;
    net1.sin_port = htons(12346);
    net1.sin_family = AF_INET;
    net2.sin_addr = ip2;
    net2.sin_port = htons(12347);
    net2.sin_family = AF_INET;
    ranks.emplace(0, ConnectRankInfo(HYBM_ROLE_PEER, net0, std::vector<TransportMemoryKey>{}));
    ranks.emplace(1, ConnectRankInfo(HYBM_ROLE_PEER, net1, std::vector<TransportMemoryKey>{}));
    ranks.emplace(2, ConnectRankInfo(HYBM_ROLE_PEER, net2, std::vector<TransportMemoryKey>{}));
    manager.currentRanksInfo_ = ranks;

    // Mock server socket handle
    manager.serverSocketHandle_ = reinterpret_cast<void*>(0x1234UL);

    DlHccpApiFnGuard guard;
    DlHccpApi::gRaSocketWhiteListAdd = &FakeRaSocketWhiteListAddFail;

    // Test failure case
    EXPECT_EQ(manager.GenerateWhiteList(), BM_ERROR);
}

TEST(FixedRanksQpManagerTestTest, CheckReadyConnectionSuccess)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    // Prepare test data
    std::unordered_map<uint32_t, FixedRanksQpManagerTest::AiCoreConnChannel> connections;
    in_addr ip{};
    inet_aton("192.168.1.1", &ip);
    void* socketHandle = reinterpret_cast<void*>(0x1234UL);
    connections.emplace(0, FixedRanksQpManagerTest::AiCoreConnChannel(ip, socketHandle));

    std::unordered_map<in_addr_t, uint32_t> addr2index;
    addr2index[ip.s_addr] = 0;

    HccpSocketInfo socketInfo{};
    socketInfo.handle = socketHandle;
    socketInfo.remoteIp.addr = ip;
    socketInfo.fd = reinterpret_cast<void*>(0x5678UL);

    // Test success case
    EXPECT_EQ(manager.CheckReadyConnection(connections, addr2index, socketInfo), BM_OK);
    auto pos = connections.find(0);
    ASSERT_NE(pos, connections.end());
    EXPECT_EQ(pos->second.socketFd, socketInfo.fd);
}

TEST(FixedRanksQpManagerTestTest, CheckReadyConnectionIpNotInMap)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    // Prepare test data
    std::unordered_map<uint32_t, FixedRanksQpManagerTest::AiCoreConnChannel> connections;
    in_addr ip1{};
    inet_aton("192.168.1.1", &ip1);
    void* socketHandle = reinterpret_cast<void*>(0x1234UL);
    connections.emplace(0, FixedRanksQpManagerTest::AiCoreConnChannel(ip1, socketHandle));

    std::unordered_map<in_addr_t, uint32_t> addr2index;
    // Don't add ip2 to addr2index

    in_addr ip2{};
    inet_aton("192.168.1.2", &ip2);
    HccpSocketInfo socketInfo{};
    socketInfo.handle = socketHandle;
    socketInfo.remoteIp.addr = ip2;
    socketInfo.fd = reinterpret_cast<void*>(0x5678UL);

    // Test failure case: ip not in addr2index
    EXPECT_EQ(manager.CheckReadyConnection(connections, addr2index, socketInfo), BM_DL_FUNCTION_FAILED);
}

TEST(FixedRanksQpManagerTestTest, CheckReadyConnectionRankNotInConnections)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    // Prepare test data
    std::unordered_map<uint32_t, FixedRanksQpManagerTest::AiCoreConnChannel> connections;
    // Don't add rank 1 to connections

    in_addr ip{};
    inet_aton("192.168.1.1", &ip);
    std::unordered_map<in_addr_t, uint32_t> addr2index;
    addr2index[ip.s_addr] = 1;  // Map to rank 1 which is not in connections

    HccpSocketInfo socketInfo{};
    socketInfo.handle = reinterpret_cast<void*>(0x1234UL);
    socketInfo.remoteIp.addr = ip;
    socketInfo.fd = reinterpret_cast<void*>(0x5678UL);

    // Test failure case: rank not in connections
    EXPECT_EQ(manager.CheckReadyConnection(connections, addr2index, socketInfo), BM_DL_FUNCTION_FAILED);
}

TEST(FixedRanksQpManagerTestTest, CheckReadyConnectionSocketFdAlreadySet)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    // Prepare test data
    std::unordered_map<uint32_t, FixedRanksQpManagerTest::AiCoreConnChannel> connections;
    in_addr ip{};
    inet_aton("192.168.1.1", &ip);
    void* socketHandle = reinterpret_cast<void*>(0x1234UL);
    connections.emplace(0, FixedRanksQpManagerTest::AiCoreConnChannel(ip, socketHandle));
    auto pos = connections.find(0);
    ASSERT_NE(pos, connections.end());
    pos->second.socketFd = reinterpret_cast<void*>(0x9ABCUL);  // Already set

    std::unordered_map<in_addr_t, uint32_t> addr2index;
    addr2index[ip.s_addr] = 0;

    HccpSocketInfo socketInfo{};
    socketInfo.handle = socketHandle;
    socketInfo.remoteIp.addr = ip;
    socketInfo.fd = reinterpret_cast<void*>(0x5678UL);

    // Test failure case: socketFd already set
    EXPECT_EQ(manager.CheckReadyConnection(connections, addr2index, socketInfo), BM_DL_FUNCTION_FAILED);
}

TEST(FixedRanksQpManagerTestTest, CheckReadyConnectionHandleMismatch)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    // Prepare test data
    std::unordered_map<uint32_t, FixedRanksQpManagerTest::AiCoreConnChannel> connections;
    in_addr ip{};
    inet_aton("192.168.1.1", &ip);
    void* socketHandle1 = reinterpret_cast<void*>(0x1234UL);
    connections.emplace(0, FixedRanksQpManagerTest::AiCoreConnChannel(ip, socketHandle1));

    std::unordered_map<in_addr_t, uint32_t> addr2index;
    addr2index[ip.s_addr] = 0;

    void* socketHandle2 = reinterpret_cast<void*>(0x5678UL);  // Different handle
    HccpSocketInfo socketInfo{};
    socketInfo.handle = socketHandle2;
    socketInfo.remoteIp.addr = ip;
    socketInfo.fd = reinterpret_cast<void*>(0x9ABCUL);

    // Test failure case: handle mismatch
    EXPECT_EQ(manager.CheckReadyConnection(connections, addr2index, socketInfo), BM_DL_FUNCTION_FAILED);
}

TEST(FixedRanksQpManagerTestTest, CreateOneQpSuccess)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);
    manager.rdmaHandle_ = reinterpret_cast<void*>(0x1234UL);

    DlHccpApiFnGuard guard;
    DlHccpApi::gRaQpAiCreate = &FakeRaQpAiCreateOk;

    // Prepare test data
    in_addr ip{};
    inet_aton("192.168.1.1", &ip);
    FixedRanksQpManagerTest::AiCoreConnChannel channel(ip);

    // Test success case
    EXPECT_EQ(manager.CreateOneQp(channel), 0);
    EXPECT_NE(channel.qpHandle, nullptr);
}

TEST(FixedRanksQpManagerTestTest, CreateOneQpFail)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);
    manager.rdmaHandle_ = reinterpret_cast<void*>(0x1234UL);

    DlHccpApiFnGuard guard;
    DlHccpApi::gRaQpAiCreate = &FakeRaQpAiCreateFail;

    // Prepare test data
    in_addr ip{};
    inet_aton("192.168.1.1", &ip);
    FixedRanksQpManagerTest::AiCoreConnChannel channel(ip);

    // Test failure case
    EXPECT_NE(manager.CreateOneQp(channel), 0);
    EXPECT_EQ(channel.qpHandle, nullptr);
}

TEST(FixedRanksQpManagerTestTest, WaitConnectionsReadySuccess)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    DlHccpApiFnGuard guard;
    DlHccpApi::gRaGetSockets = &FakeRaGetSocketsOk;

    // Prepare connections waiting for ready.
    std::unordered_map<uint32_t, FixedRanksQpManagerTest::AiCoreConnChannel> connections;
    in_addr ip{};
    inet_aton("192.168.1.1", &ip);
    void* socketHandle = reinterpret_cast<void*>(0x1234UL);
    connections.emplace(0, FixedRanksQpManagerTest::AiCoreConnChannel(ip, socketHandle));

    EXPECT_EQ(manager.WaitConnectionsReady(connections), BM_OK);
    auto pos = connections.find(0);
    ASSERT_NE(pos, connections.end());
    EXPECT_NE(pos->second.socketFd, nullptr);
}

TEST(FixedRanksQpManagerTestTest, WaitConnectionsReadyFailWhenRaGetSocketsFail)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    DlHccpApiFnGuard guard;
    DlHccpApi::gRaGetSockets = &FakeRaGetSocketsFail;

    std::unordered_map<uint32_t, FixedRanksQpManagerTest::AiCoreConnChannel> connections;
    in_addr ip{};
    inet_aton("192.168.1.1", &ip);
    connections.emplace(0, FixedRanksQpManagerTest::AiCoreConnChannel(ip, reinterpret_cast<void*>(0x1234UL)));

    EXPECT_EQ(manager.WaitConnectionsReady(connections), BM_DL_FUNCTION_FAILED);
}

TEST(FixedRanksQpManagerTestTest, FillQpInfoOnlySelfMrSuccess)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    DlAclApiFnGuard aclGuard;
    DlAclApi::pAclrtMalloc = &FakeAclrtMallocOk;
    DlAclApi::pAclrtMemcpy = &FakeAclrtMemcpyOk;

    EXPECT_TRUE(manager.ReserveQpInfoSpace());

    // rank0(self): non-empty memoryMap; rank1: empty memoryMap so it will be skipped in FillQpInfo.
    std::unordered_map<uint32_t, ConnectRankInfo> ranks;
    sockaddr_in net0{};
    inet_aton("192.168.1.1", &net0.sin_addr);
    net0.sin_port = htons(12345);
    net0.sin_family = AF_INET;

    RegMemKeyUnion key0{};
    key0.deviceKey = RegMemResult(0x1000, 0x100, nullptr, 11, 22);
    ranks.emplace(0, ConnectRankInfo(HYBM_ROLE_PEER, net0, key0.commonKey));

    sockaddr_in net1{};
    inet_aton("192.168.1.2", &net1.sin_addr);
    net1.sin_port = htons(12346);
    net1.sin_family = AF_INET;
    ranks.emplace(1, ConnectRankInfo(HYBM_ROLE_PEER, net1, std::vector<TransportMemoryKey>{}));

    manager.currentRanksInfo_ = ranks;

    EXPECT_EQ(manager.FillQpInfo(), BM_OK);
}

TEST(FixedRanksQpManagerTestTest, FillQpInfoMissingConnectionForRemoteMr)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    DlAclApiFnGuard aclGuard;
    DlAclApi::pAclrtMalloc = &FakeAclrtMallocOk;
    DlAclApi::pAclrtMemcpy = &FakeAclrtMemcpyOk;

    EXPECT_TRUE(manager.ReserveQpInfoSpace());

    // rank1(remote): non-empty memoryMap but no corresponding connection -> should fail.
    std::unordered_map<uint32_t, ConnectRankInfo> ranks;
    sockaddr_in net0{};
    inet_aton("192.168.1.1", &net0.sin_addr);
    net0.sin_port = htons(12345);
    net0.sin_family = AF_INET;
    ranks.emplace(0, ConnectRankInfo(HYBM_ROLE_PEER, net0, std::vector<TransportMemoryKey>{}));

    sockaddr_in net1{};
    inet_aton("192.168.1.2", &net1.sin_addr);
    net1.sin_port = htons(12346);
    net1.sin_family = AF_INET;
    RegMemKeyUnion key1{};
    key1.deviceKey = RegMemResult(0x2000, 0x100, nullptr, 33, 44);
    ranks.emplace(1, ConnectRankInfo(HYBM_ROLE_PEER, net1, key1.commonKey));

    manager.currentRanksInfo_ = ranks;

    EXPECT_EQ(manager.FillQpInfo(), BM_ERROR);
}

TEST(FixedRanksQpManagerTestTest, CloseConnections)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    DlHccpApiFnGuard guard;
    DlHccpApi::gRaQpDestroy = &FakeRaQpDestroyOk;
    DlHccpApi::gRaSocketBatchClose = &FakeRaSocketBatchCloseOk;
    DlHccpApi::gRaSocketDeinit = &FakeRaSocketDeinitOk;

    // Prepare test data
    std::unordered_map<uint32_t, FixedRanksQpManagerTest::AiCoreConnChannel> connections;
    in_addr ip0{};
    inet_aton("192.168.1.1", &ip0);
    connections.emplace(0, FixedRanksQpManagerTest::AiCoreConnChannel(ip0, reinterpret_cast<void*>(0x1234UL)));
    auto pos0 = connections.find(0);
    ASSERT_NE(pos0, connections.end());
    pos0->second.qpHandle = reinterpret_cast<void*>(0x5678UL);
    pos0->second.socketFd = reinterpret_cast<void*>(0x9ABCUL);

    in_addr ip1{};
    inet_aton("192.168.1.2", &ip1);
    connections.emplace(1, FixedRanksQpManagerTest::AiCoreConnChannel(ip1, reinterpret_cast<void*>(0xDEF0UL)));
    auto pos1 = connections.find(1);
    ASSERT_NE(pos1, connections.end());
    pos1->second.qpHandle = reinterpret_cast<void*>(0x12345UL);
    pos1->second.socketFd = reinterpret_cast<void*>(0x67890UL);

    // Test close connections
    manager.CloseConnections(connections);
    EXPECT_TRUE(connections.empty());
}

TEST(FixedRanksQpManagerTestTest, CloseClientConnections)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    DlHccpApiFnGuard guard;
    DlHccpApi::gRaQpDestroy = &FakeRaQpDestroyOk;
    DlHccpApi::gRaSocketBatchClose = &FakeRaSocketBatchCloseOk;
    DlHccpApi::gRaSocketDeinit = &FakeRaSocketDeinitOk;

    // Prepare test data
    in_addr ip{};
    inet_aton("192.168.1.1", &ip);
    manager.clientConnections_.emplace(
        0, FixedRanksQpManagerTest::AiCoreConnChannel(ip, reinterpret_cast<void*>(0x1234UL)));
    {
        auto pos = manager.clientConnections_.find(0);
        ASSERT_NE(pos, manager.clientConnections_.end());
        pos->second.qpHandle = reinterpret_cast<void*>(0x5678UL);
        pos->second.socketFd = reinterpret_cast<void*>(0x9ABCUL);
    }

    // Test close client connections
    manager.CloseClientConnections();
    EXPECT_TRUE(manager.clientConnections_.empty());
}

TEST(FixedRanksQpManagerTestTest, CloseServerConnections)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    DlHccpApiFnGuard guard;
    DlHccpApi::gRaQpDestroy = &FakeRaQpDestroyOk;
    DlHccpApi::gRaSocketBatchClose = &FakeRaSocketBatchCloseOk;
    DlHccpApi::gRaSocketDeinit = &FakeRaSocketDeinitOk;

    // Prepare test data
    in_addr ip{};
    inet_aton("192.168.1.1", &ip);
    manager.serverConnections_.emplace(
        0, FixedRanksQpManagerTest::AiCoreConnChannel(ip, reinterpret_cast<void*>(0x1234UL)));
    {
        auto pos = manager.serverConnections_.find(0);
        ASSERT_NE(pos, manager.serverConnections_.end());
        pos->second.qpHandle = reinterpret_cast<void*>(0x5678UL);
        pos->second.socketFd = reinterpret_cast<void*>(0x9ABCUL);
    }

    // Test close server connections
    manager.CloseServerConnections();
    EXPECT_TRUE(manager.serverConnections_.empty());
}

TEST(FixedRanksQpManagerTestTest, CloseServices)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    FixedRanksQpManagerTest manager(0, 0, 2, devNet);

    DlHccpApiFnGuard guard;
    DlHccpApi::gRaQpDestroy = &FakeRaQpDestroyOk;
    DlHccpApi::gRaSocketBatchClose = &FakeRaSocketBatchCloseOk;
    DlHccpApi::gRaSocketDeinit = &FakeRaSocketDeinitOk;

    // Prepare test data
    in_addr ip{};
    inet_aton("192.168.1.1", &ip);
    manager.clientConnections_.emplace(
        0, FixedRanksQpManagerTest::AiCoreConnChannel(ip, reinterpret_cast<void*>(0x1234UL)));
    {
        auto pos = manager.clientConnections_.find(0);
        ASSERT_NE(pos, manager.clientConnections_.end());
        pos->second.qpHandle = reinterpret_cast<void*>(0x5678UL);
        pos->second.socketFd = reinterpret_cast<void*>(0x9ABCUL);
    }

    manager.serverConnections_.emplace(
        1, FixedRanksQpManagerTest::AiCoreConnChannel(ip, reinterpret_cast<void*>(0xDEF0UL)));
    {
        auto pos = manager.serverConnections_.find(1);
        ASSERT_NE(pos, manager.serverConnections_.end());
        pos->second.qpHandle = reinterpret_cast<void*>(0x12345UL);
        pos->second.socketFd = reinterpret_cast<void*>(0x67890UL);
    }

    // Test close services
    manager.CloseServices();
    EXPECT_TRUE(manager.clientConnections_.empty());
    EXPECT_TRUE(manager.serverConnections_.empty());
    EXPECT_EQ(manager.clientConnectThread_, nullptr);
    EXPECT_EQ(manager.serverConnectThread_, nullptr);
}
