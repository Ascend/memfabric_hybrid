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
#include <thread>
#include <chrono>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "hybm_transport_common.h"

#define private public
#define protected public
#include "bipartite_ranks_qp_manager.h"
#include "dl_acl_api.h"
#include "dl_hccp_api.h"
#undef private
#undef protected

using namespace ock::mf;
using namespace ock::mf::transport::device;

namespace {
static constexpr uint16_t K_PORT_9000 = 9000;
static constexpr uint16_t K_PORT_9001 = 9001;
static constexpr uint16_t K_PORT_9002 = 9002;
static constexpr uint16_t K_PORT_9003 = 9003;
static constexpr uint16_t K_PORT_9004 = 9004;
static constexpr uint16_t K_PORT_9005 = 9005;
static constexpr uint32_t K_RANK_COUNT = 4;
static constexpr uint32_t K_RANK_0 = 0;
static constexpr uint32_t K_RANK_1 = 1;
static constexpr uint32_t K_RANK_2 = 2;
static constexpr uint32_t K_RANK_3 = 3;
static constexpr uint32_t K_RANK_5 = 5;
static constexpr uint32_t K_RANK_6 = 6;
static constexpr uint32_t K_RANK_10 = 10;
static constexpr uint32_t K_OUTOF_RANGE_RANK = 100;

static void *ToVoidPtr(uintptr_t i)
{
    return reinterpret_cast<void *>(i);
}

struct DlAclApiFnGuard {
    aclrtSetDeviceFunc oldAclrtSetDevice{DlAclApi::pAclrtSetDevice};

    ~DlAclApiFnGuard()
    {
        DlAclApi::pAclrtSetDevice = oldAclrtSetDevice;
    }
};

int FakeAclrtSetDeviceOk(uint32_t deviceId)
{
    (void)deviceId;
    return 0;
}

class BipartiteRanksQpManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        sockaddr_in devNet{};
        devNet.sin_family = AF_INET;
        devNet.sin_port = htons(K_PORT_9000);
        inet_pton(AF_INET, "127.0.0.1", &devNet.sin_addr);
        manager = std::make_unique<BipartiteRanksQpManager>(1, K_RANK_2, 0, K_RANK_COUNT, devNet, true);
    }
    void TearDown() override
    {
        if (manager) {
            manager->Shutdown();
        }
    }
    std::unique_ptr<BipartiteRanksQpManager> manager;
};
}  // namespace

TEST_F(BipartiteRanksQpManagerTest, Constructor)
{
    EXPECT_TRUE(manager != nullptr);
    EXPECT_EQ(manager->GetQpHandleWithRankId(0), nullptr);
}

TEST_F(BipartiteRanksQpManagerTest, SetRemoteRankInfo)
{
    // 正常分支
    std::unordered_map<uint32_t, ock::mf::transport::device::ConnectRankInfo> ranks;
    sockaddr_in net{};
    net.sin_family = AF_INET;
    net.sin_port = htons(K_PORT_9001);
    inet_pton(AF_INET, "192.168.1.2", &net.sin_addr);
    ock::mf::transport::TransportMemoryKey mk{};
    ranks.emplace(K_RANK_1, ock::mf::transport::device::ConnectRankInfo(HYBM_ROLE_SENDER, net, mk));
    int ret = manager->SetRemoteRankInfo(ranks);
    EXPECT_EQ(ret, BM_OK);
    // 空 ranks
    std::unordered_map<uint32_t, ock::mf::transport::device::ConnectRankInfo> emptyRanks;
    ret = manager->SetRemoteRankInfo(emptyRanks);
    EXPECT_EQ(ret, BM_OK);
    // role == rankRole_ 分支
    ranks.clear();
    ranks.emplace(K_RANK_2, ock::mf::transport::device::ConnectRankInfo(HYBM_ROLE_RECEIVER, net, mk));
    ret = manager->SetRemoteRankInfo(ranks);
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(BipartiteRanksQpManagerTest, SetRemoteRankInfoOutOfRange)
{
    std::unordered_map<uint32_t, ock::mf::transport::device::ConnectRankInfo> ranks;
    sockaddr_in net{};
    net.sin_family = AF_INET;
    net.sin_port = htons(K_PORT_9005);
    inet_pton(AF_INET, "192.168.1.5", &net.sin_addr);
    ock::mf::transport::TransportMemoryKey mk{};

    ranks.emplace(K_RANK_10, ock::mf::transport::device::ConnectRankInfo(HYBM_ROLE_SENDER, net, mk));
    int ret = manager->SetRemoteRankInfo(ranks);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(BipartiteRanksQpManagerTest, RemoveRanks)
{
    // 空 ranks
    std::unordered_set<uint32_t> emptyRemove;
    int ret = manager->RemoveRanks(emptyRemove);
    EXPECT_EQ(ret, BM_OK);

    // 正常分支
    sockaddr_in ip1 = Ip2Net({htonl(INADDR_LOOPBACK)}); // 127.0.0.1
    sockaddr_in ip2 = Ip2Net({htonl(INADDR_LOOPBACK)});
    ip2.sin_port = htons(K_PORT_9001); // 如果需要不同端口
    
    ConnectionChannel ch1{ip1, ToVoidPtr(0x1000)};
    ch1.socketFd = ToVoidPtr(0x2000);
    ch1.qpHandle = ToVoidPtr(0x3000);
    ch1.qpConnectCalled = true;
    ch1.qpStatus = 0;
    
    ConnectionChannel ch2{ip2, ToVoidPtr(0x1100)};
    ch2.socketFd = ToVoidPtr(0x2100);
    ch2.qpHandle = ToVoidPtr(0x3100);
    ch2.qpConnectCalled = false;
    ch2.qpStatus = -1;

    sockaddr_in devNet{};
    devNet.sin_family = AF_INET;
    devNet.sin_port = htons(K_PORT_9000);
    inet_pton(AF_INET, "127.0.0.1", &devNet.sin_addr);
    auto clientManager = std::make_unique<BipartiteRanksQpManager>(1, K_RANK_2, 0, K_RANK_COUNT, devNet, false);
    clientManager->connections_.clear();
    clientManager->connections_.emplace(K_RANK_0, std::move(ch1));
    clientManager->connections_.emplace(K_RANK_1, std::move(ch2));

    std::unordered_set<uint32_t> toRemove{K_RANK_1};
    ret = clientManager->RemoveRanks(toRemove);
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(BipartiteRanksQpManagerTest, StartupShutdown)
{
    void *fakeRdma = ToVoidPtr(0x1234);
    int ret = manager->Startup(fakeRdma);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    // managerRunning_ 已经为 true
    ret = manager->Startup(fakeRdma);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    manager->Shutdown();
    // 多次 Shutdown
    manager->Shutdown();
}
// GetQpHandleWithRankId 边界和异常分支
TEST_F(BipartiteRanksQpManagerTest, GetQpHandleWithRankIdEdgeCases)
{
    // rankId 超出 userQpInfo_ 范围
    auto* qp = manager->GetQpHandleWithRankId(K_OUTOF_RANGE_RANK);
    EXPECT_EQ(qp, nullptr);
    // userQpInfo_[rankId].qpHandle == nullptr
    qp = manager->GetQpHandleWithRankId(K_RANK_2);
    EXPECT_EQ(qp, nullptr);
}

TEST_F(BipartiteRanksQpManagerTest, StartupWithNullRdma)
{
    int ret = manager->Startup(nullptr);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

TEST_F(BipartiteRanksQpManagerTest, GetQpHandleWithRankIdNoQp)
{
    auto* qp = manager->GetQpHandleWithRankId(1);
    EXPECT_EQ(qp, nullptr);
}

TEST_F(BipartiteRanksQpManagerTest, GetQpHandleWithRankIdOutOfRange)
{
    auto* qp = manager->GetQpHandleWithRankId(K_RANK_10);
    EXPECT_EQ(qp, nullptr);
}

TEST_F(BipartiteRanksQpManagerTest, PutQpHandle)
{
    auto *qp = new ock::mf::transport::device::UserQpInfo;
    qp->qpHandle = ToVoidPtr(0x5555);
    qp->ref.store(1);
    manager->PutQpHandle(qp);
    delete qp;
}

TEST_F(BipartiteRanksQpManagerTest, ThreadLoopExit)
{
    void *fakeRdma = ToVoidPtr(0x1234);
    manager->Startup(fakeRdma);
    std::this_thread::sleep_for(std::chrono::milliseconds(K_OUTOF_RANGE_RANK));
    manager->Shutdown();
}

TEST_F(BipartiteRanksQpManagerTest, BackgroundProcess)
{
    void *fakeRdma = ToVoidPtr(0x1234);
    manager->Startup(fakeRdma);
    manager->managerRunning_.store(false); // 让循环快速退出
    manager->BackgroundProcess();
    std::this_thread::sleep_for(std::chrono::milliseconds(K_OUTOF_RANGE_RANK));
    manager->Shutdown();
}

TEST_F(BipartiteRanksQpManagerTest, ProcessServerAddWhitelistTask)
{
    in_addr remoteIp{};
    inet_pton(AF_INET, "192.168.1.10", &remoteIp);
    manager->connectionTasks_.whitelistTask.status.exist = true;
    manager->connectionTasks_.whitelistTask.remoteIps.emplace(1, remoteIp);

    int ret = manager->ProcessServerAddWhitelistTask();
    EXPECT_NE(ret, BM_OK);
    EXPECT_EQ(manager->connections_.count(1), 1u);
}

TEST_F(BipartiteRanksQpManagerTest, ProcessClientConnectSocketTask)
{
    sockaddr_in devNet{};
    devNet.sin_family = AF_INET;
    devNet.sin_port = htons(K_PORT_9000);
    inet_pton(AF_INET, "127.0.0.1", &devNet.sin_addr);
    auto clientManager = std::make_unique<BipartiteRanksQpManager>(1, K_RANK_2, 0, K_RANK_COUNT, devNet, false);
    clientManager->connectionTasks_.clientConnectTask.status.exist = true;
    sockaddr_in nw{};
    nw.sin_family = AF_INET;
    nw.sin_port = htons(K_PORT_9001);
    inet_pton(AF_INET, "192.168.1.11", &nw.sin_addr);
    clientManager->connectionTasks_.clientConnectTask.remoteAddress.emplace(K_RANK_1, nw);

    int ret = clientManager->ProcessClientConnectSocketTask();
    EXPECT_EQ(ret, 1); // receiver side does not execute client connect path
}

TEST_F(BipartiteRanksQpManagerTest, ProcessQueryConnectionStateTask)
{
    manager->connectionTasks_.queryConnectTask.status.exist = true;
    manager->connectionTasks_.queryConnectTask.ip2rank.emplace(inet_addr("192.168.1.12"), 1);

    int ret = manager->ProcessQueryConnectionStateTask();
    EXPECT_NE(ret, 0);
}

TEST_F(BipartiteRanksQpManagerTest, ProcessConnectQpTask)
{
    manager->connectionTasks_.connectQpTask.status.exist = true;
    manager->connectionTasks_.connectQpTask.ranks.insert(1);

    int ret = manager->ProcessConnectQpTask();
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(manager->connectionTasks_.connectQpTask.status.exist);
}

TEST_F(BipartiteRanksQpManagerTest, ProcessQueryQpStateTask)
{
    manager->connectionTasks_.queryQpStateTask.status.exist = true;
    manager->connectionTasks_.queryQpStateTask.ranks.insert(1);
    manager->connections_.emplace(1, ConnectionChannel{});
    manager->connections_.at(1).qpHandle = ToVoidPtr(0x1234);

    int ret = manager->ProcessQueryQpStateTask();
    EXPECT_NE(ret, 0);
    EXPECT_TRUE(manager->connectionTasks_.queryQpStateTask.status.exist);
}

TEST_F(BipartiteRanksQpManagerTest, ProcessUpdateLocalMrTask)
{
    manager->connectionTasks_.updateMrTask.status.exist = true;
    manager->ProcessUpdateLocalMrTask();
    EXPECT_FALSE(manager->connectionTasks_.updateMrTask.status.exist);
}

TEST_F(BipartiteRanksQpManagerTest, ProcessUpdateRemoteMrTask)
{
    manager->connectionTasks_.updateRemoteMrTask.status.exist = true;
    manager->connectionTasks_.updateRemoteMrTask.addedMrRanks.insert(K_RANK_2);
    manager->ProcessUpdateRemoteMrTask();
    EXPECT_FALSE(manager->connectionTasks_.updateRemoteMrTask.status.exist);
}

TEST_F(BipartiteRanksQpManagerTest, CloseServices)
{
    manager->CloseServices();
    EXPECT_EQ(manager->managerRunning_.load(), false);
}

TEST_F(BipartiteRanksQpManagerTest, CreateConnectInfos)
{
    std::unordered_map<uint32_t, sockaddr_in> remotes;
    sockaddr_in nw{};
    nw.sin_family = AF_INET;
    nw.sin_port = htons(K_PORT_9002);
    inet_pton(AF_INET, "192.168.1.13", &nw.sin_addr);
    remotes.emplace(1, nw);

    std::vector<HccpSocketConnectInfo> connectInfos;
    int ret = manager->CreateConnectInfos(remotes, connectInfos, manager->connectionTasks_.clientConnectTask);
    EXPECT_NE(ret, BM_OK);
}

TEST_F(BipartiteRanksQpManagerTest, Parse2SocketInfo)
{
    in_addr remoteIp{};
    inet_pton(AF_INET, "192.168.1.14", &remoteIp);
    manager->connections_.emplace(1, ConnectionChannel{Ip2Net(remoteIp), ToVoidPtr(0x1234)});

    std::unordered_map<in_addr_t, uint32_t> ip2rank;
    ip2rank.emplace(remoteIp.s_addr, 1);
    std::vector<HccpSocketInfo> socketInfos;
    std::vector<IpType> types;

    manager->Parse2SocketInfo(ip2rank, socketInfos, types);
    EXPECT_EQ(socketInfos.size(), 1u);
    EXPECT_EQ(types.size(), 1u);
}

TEST_F(BipartiteRanksQpManagerTest, GetSocketConn)
{
    std::vector<HccpSocketInfo> socketInfos(1);
    socketInfos[0].remoteIp.addr.s_addr = inet_addr("192.168.1.15");
    socketInfos[0].status = 0;

    QueryConnectionStateTask task;
    std::unordered_map<in_addr_t, uint32_t> ip2rank;
    ip2rank.emplace(inet_addr("192.168.1.15"), 1);
    std::unordered_set<uint32_t> connectedRanks;
    std::vector<IpType> types;

    int ret = manager->GetSocketConn(socketInfos, task, ip2rank, connectedRanks, types);
    EXPECT_NE(ret, 0);
}

TEST_F(BipartiteRanksQpManagerTest, GenerateLocalLiteMrs)
{
    uint32_t lkey = 123;
    uint32_t rkey = 456;
    RegMemResult region(0x1000, 0x1000, nullptr, lkey, rkey);
    manager->currentLocalMrs_.emplace(0x1000, region);

    auto localMrs = manager->GenerateLocalLiteMrs();
    EXPECT_EQ(localMrs.size(), 1u);
    EXPECT_EQ(localMrs[0].key, 123u);
}

TEST_F(BipartiteRanksQpManagerTest, GenerateRemoteLiteMrs)
{
    sockaddr_in nw{};
    nw.sin_family = AF_INET;
    nw.sin_port = htons(K_PORT_9003);
    inet_pton(AF_INET, "192.168.1.16", &nw.sin_addr);
    ock::mf::transport::TransportMemoryKey key{};
    manager->currentRanksInfo_.emplace(1, ConnectRankInfo(HYBM_ROLE_SENDER, nw, key));

    auto remoteMrs = manager->GenerateRemoteLiteMrs(1);
    EXPECT_GE(remoteMrs.size(), 0u);
}

TEST_F(BipartiteRanksQpManagerTest, GenDiffInfoChangeRanks)
{
    std::unordered_map<uint32_t, ConnectRankInfo> last;
    sockaddr_in nw{};
    nw.sin_family = AF_INET;
    nw.sin_port = htons(K_PORT_9004);
    inet_pton(AF_INET, "192.168.1.17", &nw.sin_addr);
    ock::mf::transport::TransportMemoryKey key{};
    manager->currentRanksInfo_.emplace(K_RANK_2, ConnectRankInfo(HYBM_ROLE_SENDER, nw, key));

    std::unordered_map<uint32_t, sockaddr_in> addedRanks;
    std::unordered_set<uint32_t> addMrRanks;

    manager->GenDiffInfoChangeRanks(last, addedRanks, addMrRanks);
    EXPECT_EQ(addedRanks.count(K_RANK_2), 1u);
}

TEST_F(BipartiteRanksQpManagerTest, GenTaskFromChangeRanks)
{
    std::unordered_map<uint32_t, sockaddr_in> addedRanks;
    std::unordered_set<uint32_t> addMrRanks;
    in_addr ip{};
    inet_pton(AF_INET, "192.168.1.18", &ip);
    sockaddr_in nw = Ip2Net(ip);
    addedRanks.emplace(K_RANK_3, nw);
    addMrRanks.insert(K_RANK_3);

    manager->GenTaskFromChangeRanks(addedRanks, addMrRanks);
    EXPECT_TRUE(manager->connectionTasks_.whitelistTask.status.exist);
    EXPECT_TRUE(manager->connectionTasks_.updateRemoteMrTask.status.exist);
}

TEST_F(BipartiteRanksQpManagerTest, GenTaskFromChangeRanksClient)
{
    std::unordered_map<uint32_t, sockaddr_in> addedRanks;
    std::unordered_set<uint32_t> addMrRanks;
    in_addr ip{};
    inet_pton(AF_INET, "192.168.1.18", &ip);
    sockaddr_in nw = Ip2Net(ip);
    addedRanks.emplace(K_RANK_3, nw);
    addMrRanks.insert(K_RANK_3);

    sockaddr_in devNet{};
    devNet.sin_family = AF_INET;
    devNet.sin_port = htons(K_PORT_9000);
    inet_pton(AF_INET, "127.0.0.1", &devNet.sin_addr);
    auto clientManager = std::make_unique<BipartiteRanksQpManager>(1, K_RANK_2, 0, K_RANK_COUNT, devNet, false);
    clientManager->GenTaskFromChangeRanks(addedRanks, addMrRanks);
    EXPECT_TRUE(clientManager->connectionTasks_.clientConnectTask.status.exist);
    EXPECT_TRUE(clientManager->connectionTasks_.updateRemoteMrTask.status.exist);
}

TEST_F(BipartiteRanksQpManagerTest, SetQpHandleRegisterMr)
{
    ra_qp_handle qp{};
    pthread_mutex_init(&qp.qp_mutex, nullptr);

    std::vector<lite_mr_info> mrs;
    mrs.push_back({1, 0xabc, K_OUTOF_RANGE_RANK});
    mrs.push_back({K_RANK_2, 0xdef, 200});

    manager->SetQpHandleRegisterMr(&qp, mrs, true);
    EXPECT_EQ(qp.local_mr[1].key, 1u);
    EXPECT_EQ(qp.local_mr[K_RANK_2].key, 2u);

    manager->SetQpHandleRegisterMr(&qp, mrs, false);
    EXPECT_EQ(qp.rem_mr[1].key, 1u);
    EXPECT_EQ(qp.rem_mr[K_RANK_2].key, 2u);
}

TEST_F(BipartiteRanksQpManagerTest, BatchConnectWithRetry)
{
    std::vector<HccpSocketConnectInfo> connectInfos(1);
    connectInfos[0].handle = ToVoidPtr(0x1234);
    manager->connectionTasks_.clientConnectTask.status.exist = true;

    int ret = manager->BatchConnectWithRetry(connectInfos, manager->connectionTasks_.clientConnectTask,
                                             manager->connectionTasks_.clientConnectTask.remoteAddress);
    EXPECT_NE(ret, 0);
}

TEST_F(BipartiteRanksQpManagerTest, ProcessSocketConnectionsByIP)
{
    std::vector<HccpSocketInfo> socketInfos(1);
    in_addr ip{};
    inet_pton(AF_INET, "192.168.1.20", &ip);
    socketInfos[0].remoteIp.addr.s_addr = ip.s_addr;
    socketInfos[0].fd = ToVoidPtr(0x9876);
    socketInfos[0].status = 1;

    manager->connections_.emplace(K_RANK_5, ConnectionChannel{Ip2Net(ip), ToVoidPtr(0x5678)});
    std::unordered_map<in_addr_t, uint32_t> ip2rank;
    ip2rank.emplace(ip.s_addr, K_RANK_5);

    std::unordered_set<uint32_t> connectedRanks;
    std::vector<IpType> types;
    uint32_t successCount = 0;

    manager->ProcessSocketConnectionsByIP(1, socketInfos, ip2rank, types, connectedRanks, successCount);
    EXPECT_EQ(successCount, 1u);
    EXPECT_EQ(connectedRanks.size(), 1u);
    EXPECT_TRUE(ip2rank.empty());
}

TEST_F(BipartiteRanksQpManagerTest, ProcessRankRemoval)
{
    manager->connections_.emplace(K_RANK_6, ConnectionChannel{Ip2Net({htonl(INADDR_LOOPBACK)}), ToVoidPtr(0x1111)});
    manager->connections_.at(K_RANK_6).qpHandle = ToVoidPtr(0x2222);
    manager->connections_.at(K_RANK_6).socketFd = ToVoidPtr(0x3333);

    std::vector<HccpSocketCloseInfo> socketCloseInfos;
    std::vector<HccpSocketWhiteListInfo> whitelist;

    manager->ProcessRankRemoval(K_RANK_6, socketCloseInfos, whitelist);
    EXPECT_EQ(manager->connections_.count(K_RANK_6), 1u);
}
