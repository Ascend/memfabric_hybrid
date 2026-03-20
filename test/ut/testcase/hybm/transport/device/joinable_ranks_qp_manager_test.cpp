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
#include "hybm_transport_common.h"
#include "device_rdma_common.h"
#include "hybm_types.h"

#define private public
#define protected public
#include "joinable_ranks_qp_manager.h"
#undef private
#undef protected

static constexpr uint16_t K_PORT_8000 = 8000;
static constexpr uint16_t K_PORT_8001 = 8001;
static constexpr uint32_t K_RANK_COUNT = 4;
static constexpr uint32_t K_RANK_ID_0 = 0;
static constexpr uint32_t K_RANK_ID_1 = 1;
static constexpr uint32_t K_RANK_ID_2 = 2;
static constexpr uint32_t K_RANK_ID_3 = 3;
static constexpr uint32_t K_OUT_OF_RANGE_RANK = 100;
static constexpr uint32_t K_OUT_OF_RANGE_INDEX = 10;
static constexpr uint32_t K_DEFAULT_SLEEP_MS = 2000;

static void *ToVoidPtr(uintptr_t i)
{
    return reinterpret_cast<void *>(i);
}

using namespace ock::mf;
using namespace ock::mf::transport::device;

// Testable subclass to mock external dependencies like CreateServerSocket().
class TestableJoinableRanksQpManager : public JoinableRanksQpManager {
public:
    using JoinableRanksQpManager::JoinableRanksQpManager;

public:
    int CreateServerSocket() noexcept { return BM_OK; }
    void *CreateLocalSocket() noexcept { return ToVoidPtr(0x1234); }
};

// 测试夹具
class JoinableRanksQpManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        static constexpr uint16_t kPort8002 = 8002;
        sockaddr_in devNet{};
        devNet.sin_family = AF_INET;
        devNet.sin_port = htons(kPort8002);
        inet_pton(AF_INET, "127.0.0.1", &devNet.sin_addr);
        // uint32_t userDeviceId, uint32_t deviceId, uint32_t rankId, uint32_t rankCount, sockaddr_in devNet
        manager = std::make_unique<TestableJoinableRanksQpManager>(0, 0, K_RANK_ID_0, K_RANK_COUNT, devNet);
    }

    void TearDown() override
    {
        if (manager) {
            manager->Shutdown();
        }
    }

    std::unique_ptr<TestableJoinableRanksQpManager> manager;
};

// 测试构造函数
TEST_F(JoinableRanksQpManagerTest, Constructor)
{
    EXPECT_TRUE(manager != nullptr);
    EXPECT_EQ(manager->GetQpHandleWithRankId(0), nullptr);
}

// 测试 SetRemoteRankInfo 添加远程排名、空参数、越界分支
TEST_F(JoinableRanksQpManagerTest, SetRemoteRankInfo)
{
    std::unordered_map<uint32_t, ock::mf::transport::device::ConnectRankInfo> ranks;
    sockaddr_in net{};
    net.sin_family = AF_INET;
    net.sin_port = htons(K_PORT_8001);
    inet_pton(AF_INET, "192.168.1.1", &net.sin_addr);
    ock::mf::transport::TransportMemoryKey mk{};
    ranks.emplace(K_RANK_ID_1, ock::mf::transport::device::ConnectRankInfo(HYBM_ROLE_PEER, net, mk));
    int ret = manager->SetRemoteRankInfo(ranks);
    EXPECT_EQ(ret, BM_OK);
    // 空 ranks
    std::unordered_map<uint32_t, ock::mf::transport::device::ConnectRankInfo> emptyRanks;
    ret = manager->SetRemoteRankInfo(emptyRanks);
    EXPECT_EQ(ret, BM_OK);
    // 越界 rankId
    ranks.clear();
    ranks.emplace(K_OUT_OF_RANGE_RANK, ock::mf::transport::device::ConnectRankInfo(HYBM_ROLE_PEER, net, mk));
    ret = manager->SetRemoteRankInfo(ranks);
    EXPECT_EQ(ret, BM_OK);
}

// SetRemoteRankInfoOutOfRange 已被上面覆盖，可省略

// 测试 RemoveRanks 正常、空参数、越界分支
TEST_F(JoinableRanksQpManagerTest, RemoveRanks)
{
    std::unordered_set<uint32_t> toRemove{K_RANK_ID_1, K_RANK_ID_3};
    int ret = manager->RemoveRanks(toRemove);
    EXPECT_EQ(ret, BM_OK);
    // 空 ranks
    std::unordered_set<uint32_t> emptyRemove;
    ret = manager->RemoveRanks(emptyRemove);
    EXPECT_EQ(ret, BM_OK);
    // 越界 rankId
    std::unordered_set<uint32_t> outOfRange{K_OUT_OF_RANGE_RANK};
    ret = manager->RemoveRanks(outOfRange);
    EXPECT_EQ(ret, BM_OK);
}

// 测试 Startup 和 Shutdown，空参数、重复调用分支
TEST_F(JoinableRanksQpManagerTest, StartupShutdown)
{
    void *fakeRdma = ToVoidPtr(0x1234);
    int ret = manager->Startup(fakeRdma);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    manager->Shutdown();
    // 多次 Shutdown
    manager->Shutdown();
}

// 测试 Startup 使用空 rdma 句柄
TEST_F(JoinableRanksQpManagerTest, StartupWithNullRdma)
{
    int ret = manager->Startup(nullptr);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

// 测试 GetQpHandleWithRankId 边界和异常分支
TEST_F(JoinableRanksQpManagerTest, GetQpHandleWithRankIdEdgeCases)
{
    // rankId 超出 rankCount_
    auto *qp = manager->GetQpHandleWithRankId(K_OUT_OF_RANGE_RANK);
    EXPECT_EQ(qp, nullptr);
    // qpArray_[rankId] == nullptr
    qp = manager->GetQpHandleWithRankId(K_RANK_ID_2);
    EXPECT_EQ(qp, nullptr);
}

// 测试 GetQpHandleWithRankId 在超出范围时返回 nullptr
TEST_F(JoinableRanksQpManagerTest, GetQpHandleWithRankIdOutOfRange)
{
    auto* qp = manager->GetQpHandleWithRankId(K_OUT_OF_RANGE_INDEX);
    EXPECT_EQ(qp, nullptr);
}

// 测试 PutQpHandle 边界分支
TEST_F(JoinableRanksQpManagerTest, PutQpHandle)
{
    auto *qp = new ock::mf::transport::device::UserQpInfo;
    qp->qpHandle = ToVoidPtr(0x5555);
    qp->ref.store(1);
    manager->PutQpHandle(qp);
    // ref > 1
    qp = new ock::mf::transport::device::UserQpInfo;
    qp->qpHandle = ToVoidPtr(0x5555);
    qp->ref.store(K_RANK_ID_2);
    manager->PutQpHandle(qp);
    delete qp;
}

// 测试 CheckQpReady 边界和异常分支
TEST_F(JoinableRanksQpManagerTest, CheckQpReadyEdgeCases)
{
    // 没有连接
    std::vector<uint32_t> rankIds{1, 2};
    bool ready = manager->CheckQpReady(rankIds);
    EXPECT_FALSE(ready);
    // 超出范围
    std::vector<uint32_t> outOfRange{K_OUT_OF_RANGE_INDEX};
    ready = manager->CheckQpReady(outOfRange);
    EXPECT_FALSE(ready);
    // 空 rankIds
    std::vector<uint32_t> emptyIds;
    ready = manager->CheckQpReady(emptyIds);
    EXPECT_TRUE(ready);
    // 忽略自身排名
    std::vector<uint32_t> selfIds{0};
    ready = manager->CheckQpReady(selfIds);
    EXPECT_TRUE(ready);
}

// 测试线程循环在运行标志为 false 时退出
TEST_F(JoinableRanksQpManagerTest, StartupClient)
{
    void *fakeRdma = ToVoidPtr(0x1234);
     // 初始化 manager【1卡】
    sockaddr_in devNet{};
    devNet.sin_family = AF_INET;
    devNet.sin_port = htons(K_PORT_8000);
    inet_pton(AF_INET, "127.0.0.1", &devNet.sin_addr);
    // uint32_t userDeviceId, uint32_t deviceId, uint32_t rankId, uint32_t rankCount, sockaddr_in devNet
    auto clientManager = std::make_unique<TestableJoinableRanksQpManager>(1, 1, 1, K_RANK_COUNT, devNet);
    clientManager->Startup(fakeRdma);
    std::this_thread::sleep_for(std::chrono::milliseconds(K_OUT_OF_RANGE_RANK));
    clientManager->Shutdown();
}

// 测试线程循环在运行标志为 false 时退出
TEST_F(JoinableRanksQpManagerTest, ThreadLoopExit)
{
    void *fakeRdma = ToVoidPtr(0x1234);
    manager->Startup(fakeRdma);
    std::this_thread::sleep_for(std::chrono::milliseconds(K_OUT_OF_RANGE_RANK));
    manager->Shutdown();
}

TEST_F(JoinableRanksQpManagerTest, StartClientSide)
{
    int ret  = manager->StartClientSide();
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(JoinableRanksQpManagerTest, ServerSideHandleNewClients)
{
    std::set<uint32_t> newRanks = {K_RANK_ID_0, K_RANK_ID_1, K_RANK_ID_2, K_RANK_ID_3};
    manager->ServerSideHandleNewClients(newRanks);

    // 初始化 manager【1卡】
    sockaddr_in devNet{};
    devNet.sin_family = AF_INET;
    devNet.sin_port = htons(K_PORT_8000);
    inet_pton(AF_INET, "127.0.0.1", &devNet.sin_addr);
    // uint32_t userDeviceId, uint32_t deviceId, uint32_t rankId, uint32_t rankCount, sockaddr_in devNet
    auto clientManager = std::make_unique<TestableJoinableRanksQpManager>(0, 0, K_RANK_ID_0, K_RANK_COUNT, devNet);

    // 设置remoet 连接信息【2，3】
    std::unordered_map<uint32_t, ock::mf::transport::device::ConnectRankInfo> ranks;
    sockaddr_in net{};
    net.sin_family = AF_INET;
    net.sin_port = htons(K_PORT_8001);
    inet_pton(AF_INET, "192.168.1.1", &net.sin_addr);
    ock::mf::transport::TransportMemoryKey mk{};
    ranks.emplace(K_RANK_ID_2, ock::mf::transport::device::ConnectRankInfo(HYBM_ROLE_PEER, net, mk));
    ranks.emplace(K_RANK_ID_3, ock::mf::transport::device::ConnectRankInfo(HYBM_ROLE_PEER, net, mk));
    clientManager->connections_.resize(K_RANK_COUNT); // 模拟已有连接状态
    clientManager->SetRemoteRankInfo(ranks);

    std::set<uint32_t> newClients = {K_RANK_ID_2, K_RANK_ID_3};
    clientManager->connections_[K_RANK_ID_2].socketHandle = ToVoidPtr(0x1111);
    clientManager->connections_[K_RANK_ID_3].socketHandle = ToVoidPtr(0x2222);
    int ret = clientManager->GenerateWhiteList(newClients);
    // RaSocketWhiteListAdd 时失败
    EXPECT_EQ(ret, BM_OK);

    clientManager->ServerSideHandleNewClients(newClients);
}

TEST_F(JoinableRanksQpManagerTest, ServerSideRunLoop)
{
    // 设置运行标志
    manager->running_.store(true);
    // 添加一些新客户端以触发处理分支
    manager->newClients_ = {K_RANK_ID_1, K_RANK_ID_2};
    // 在单独线程中运行循环
    std::thread loopThread([this]() {
        manager->ServerSideRunLoop();
    });
    // 等待一小段时间让循环开始
    std::this_thread::sleep_for(std::chrono::milliseconds(K_DEFAULT_SLEEP_MS));
    // 停止循环
    manager->running_.store(false);
    manager->cond_.notify_all();
    // 等待线程结束
    loopThread.join();
    // 验证循环已停止（无崩溃）
    EXPECT_FALSE(manager->running_.load());
}

TEST_F(JoinableRanksQpManagerTest, ClientSideRunLoop)
{
    // 设置运行标志
    manager->running_.store(true);
    // 添加一些新服务器以触发处理分支（假设 rankId_ = 0，服务器是较低的 ranks）
    manager->newServers_ = {K_RANK_ID_1, K_RANK_ID_2};  // 模拟新服务器连接
    // 在单独线程中运行循环
    std::thread loopThread([this]() {
        manager->ClientSideRunLoop();
    });
    // 等待一小段时间让循环开始
    std::this_thread::sleep_for(std::chrono::milliseconds(K_DEFAULT_SLEEP_MS));
    // 停止循环
    manager->running_.store(false);
    manager->cond_.notify_all();
    // 等待线程结束
    loopThread.join();
    // 验证循环已停止（无崩溃）
    EXPECT_FALSE(manager->running_.load());
}

TEST_F(JoinableRanksQpManagerTest, WaitSocketConnections)
{
    // 测试空 newRanks
    std::set<uint32_t> emptyRanks;
    int ret = manager->WaitSocketConnections(emptyRanks);
    EXPECT_EQ(ret, BM_OK);
    // 测试非空 newRanks
    std::set<uint32_t> newRanks = {0, 1, 2};

    sockaddr_in devNet{};
    devNet.sin_family = AF_INET;
    devNet.sin_port = htons(K_PORT_8000);
    inet_pton(AF_INET, "127.0.0.1", &devNet.sin_addr);
    // uint32_t userDeviceId, uint32_t deviceId, uint32_t rankId, uint32_t rankCount, sockaddr_in devNet
    auto clientManager = std::make_unique<JoinableRanksQpManager>(0, 0, 0, K_RANK_COUNT, devNet);

    // 设置remoet 连接信息【1，1】
    std::unordered_map<uint32_t, ock::mf::transport::device::ConnectRankInfo> ranks;
    sockaddr_in net{};
    net.sin_family = AF_INET;
    net.sin_port = htons(K_PORT_8001);
    inet_pton(AF_INET, "192.168.1.1", &net.sin_addr);
    ock::mf::transport::TransportMemoryKey mk{};
    ranks.emplace(1, ock::mf::transport::device::ConnectRankInfo(HYBM_ROLE_PEER, net, mk));
    ranks.emplace(K_RANK_ID_2, ock::mf::transport::device::ConnectRankInfo(HYBM_ROLE_PEER, net, mk));
    clientManager->SetRemoteRankInfo(ranks);

    clientManager->connections_.resize(K_RANK_COUNT); // 模拟已有连接状态
    clientManager->connections_[1].socketHandle = ToVoidPtr(0x1111);
    clientManager->connections_[1].qpStatus = 1; // 模拟连接成功
    ret = clientManager->WaitSocketConnections(newRanks);
    // DlHccpApi::RaGetSockets 失败
    EXPECT_EQ(ret, 1);
}

TEST_F(JoinableRanksQpManagerTest, MakeQpConnections)
{
    // 空集合分支
    std::set<uint32_t> emptyRanks = {};
    manager->MakeQpConnections(emptyRanks);

    sockaddr_in devNet{};
    devNet.sin_family = AF_INET;
    devNet.sin_port = htons(K_PORT_8000);
    inet_pton(AF_INET, "127.0.0.1", &devNet.sin_addr);
    // uint32_t userDeviceId, uint32_t deviceId, uint32_t rankId, uint32_t rankCount, sockaddr_in devNet
    auto clientManager = std::make_unique<JoinableRanksQpManager>(0, 0, 0, K_RANK_COUNT, devNet);
    std::set<uint32_t> newRanks = {K_RANK_ID_0, K_RANK_ID_1, K_RANK_ID_2};

    // 设置remoet 连接信息【1，1】
    std::unordered_map<uint32_t, ock::mf::transport::device::ConnectRankInfo> ranks;
    sockaddr_in net{};
    net.sin_family = AF_INET;
    net.sin_port = htons(K_PORT_8001);
    inet_pton(AF_INET, "192.168.1.1", &net.sin_addr);
    ock::mf::transport::TransportMemoryKey mk{};
    ranks.emplace(1, ock::mf::transport::device::ConnectRankInfo(HYBM_ROLE_PEER, net, mk));
    ranks.emplace(K_RANK_ID_2, ock::mf::transport::device::ConnectRankInfo(HYBM_ROLE_PEER, net, mk));
    clientManager->SetRemoteRankInfo(ranks);

    clientManager->connections_.resize(K_RANK_COUNT); // 模拟已有连接状态
    clientManager->connections_[1].socketFd = ToVoidPtr(0x1111);
    clientManager->connections_[1].qpHandle = ToVoidPtr(0x2222); // 模拟连接未完成
    clientManager->connections_[1].qpConnectCalled = false;

    clientManager->connections_[K_RANK_ID_2].socketFd = ToVoidPtr(0x1111);
    clientManager->connections_[K_RANK_ID_2].qpStatus = 1; // 模拟连接成功
    clientManager->MakeQpConnections(newRanks);
}

TEST_F(JoinableRanksQpManagerTest, WaitQpConnections)
{
    // 空集合分支
    std::set<uint32_t> emptyRanks = {};
    manager->WaitQpConnections(emptyRanks);

    // 初始化 manager【1卡】
    sockaddr_in devNet{};
    devNet.sin_family = AF_INET;
    devNet.sin_port = htons(K_PORT_8000);
    inet_pton(AF_INET, "127.0.0.1", &devNet.sin_addr);
    // uint32_t userDeviceId, uint32_t deviceId, uint32_t rankId, uint32_t rankCount, sockaddr_in devNet
    auto clientManager = std::make_unique<TestableJoinableRanksQpManager>(1, 1, 1, K_RANK_COUNT, devNet);

    clientManager->connections_.resize(K_RANK_COUNT); // 模拟已有连接状态
    clientManager->connections_[K_RANK_ID_3].qpHandle = ToVoidPtr(0x2222);
    clientManager->connections_[K_RANK_ID_3].qpConnectCalled = true;
    clientManager->connections_[K_RANK_ID_3].qpStatus = 1;  // 模拟连接完成

    clientManager->connections_[K_RANK_ID_2].qpHandle = ToVoidPtr(0x1111);
    clientManager->connections_[K_RANK_ID_2].qpConnectCalled = true;

    // 非空集合，代码会走完的,K_RANK_ID_0 会加入server, K_RANK_ID_2,kRankId3为client, 但是没有设置status,不会进入
    std::set<uint32_t> newRanks = {K_RANK_ID_0, K_RANK_ID_2, K_RANK_ID_3};
    clientManager->WaitQpConnections(newRanks);
}

TEST_F(JoinableRanksQpManagerTest, GenerateWhiteList)
{
    // 初始化 manager【1卡】
    sockaddr_in devNet{};
    devNet.sin_family = AF_INET;
    devNet.sin_port = htons(K_PORT_8000);
    inet_pton(AF_INET, "127.0.0.1", &devNet.sin_addr);
    // uint32_t userDeviceId, uint32_t deviceId, uint32_t rankId, uint32_t rankCount, sockaddr_in devNet
    auto clientManager = std::make_unique<TestableJoinableRanksQpManager>(1, 1, 1, K_RANK_COUNT, devNet);

    // 设置remoet 连接信息【2，3】
    std::unordered_map<uint32_t, ock::mf::transport::device::ConnectRankInfo> ranks;
    sockaddr_in net{};
    net.sin_family = AF_INET;
    net.sin_port = htons(K_PORT_8001);
    inet_pton(AF_INET, "192.168.1.1", &net.sin_addr);
    ock::mf::transport::TransportMemoryKey mk{};
    ranks.emplace(K_RANK_ID_2, ock::mf::transport::device::ConnectRankInfo(HYBM_ROLE_PEER, net, mk));
    ranks.emplace(K_RANK_ID_3, ock::mf::transport::device::ConnectRankInfo(HYBM_ROLE_PEER, net, mk));
    clientManager->SetRemoteRankInfo(ranks);

    std::set<uint32_t> newClients = {K_RANK_ID_2, K_RANK_ID_3};
    int ret = clientManager->GenerateWhiteList(newClients);
    // RaSocketWhiteListAdd 时失败
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(JoinableRanksQpManagerTest, GenerateWhiteListEmpty)
{
    // 初始化 manager【1卡】
    sockaddr_in devNet{};
    devNet.sin_family = AF_INET;
    devNet.sin_port = htons(K_PORT_8000);
    inet_pton(AF_INET, "127.0.0.1", &devNet.sin_addr);
    // uint32_t userDeviceId, uint32_t deviceId, uint32_t rankId, uint32_t rankCount, sockaddr_in devNet
    auto clientManager = std::make_unique<TestableJoinableRanksQpManager>(1, 1, 1, K_RANK_COUNT, devNet);

    // 设置remoet 连接信息【2，3】
    std::unordered_map<uint32_t, ock::mf::transport::device::ConnectRankInfo> ranks;
    sockaddr_in net{};
    net.sin_family = AF_INET;
    net.sin_port = htons(K_PORT_8001);
    inet_pton(AF_INET, "192.168.1.1", &net.sin_addr);
    ock::mf::transport::TransportMemoryKey mk{};
    ranks.emplace(K_RANK_ID_2, ock::mf::transport::device::ConnectRankInfo(HYBM_ROLE_PEER, net, mk));
    ranks.emplace(K_RANK_ID_3, ock::mf::transport::device::ConnectRankInfo(HYBM_ROLE_PEER, net, mk));
    clientManager->connections_.resize(K_RANK_COUNT); // 模拟已有连接状态
    clientManager->SetRemoteRankInfo(ranks);

    std::set<uint32_t> newClients = {K_RANK_ID_2, K_RANK_ID_3};
    clientManager->connections_[K_RANK_ID_2].socketHandle = ToVoidPtr(0x1111);
    clientManager->connections_[K_RANK_ID_3].socketHandle = ToVoidPtr(0x2222);
    int ret = clientManager->GenerateWhiteList(newClients);
    // RaSocketWhiteListAdd 时失败
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(JoinableRanksQpManagerTest, CreateConnectionToServers)
{
    // 初始化 manager【2卡是client】
    sockaddr_in devNet{};
    devNet.sin_family = AF_INET;
    devNet.sin_port = htons(K_PORT_8000);
    inet_pton(AF_INET, "127.0.0.1", &devNet.sin_addr);
    // uint32_t userDeviceId, uint32_t deviceId, uint32_t rankId, uint32_t rankCount, sockaddr_in devNet
    auto clientManager = std::make_unique<TestableJoinableRanksQpManager>(2, 2, 2, K_RANK_COUNT, devNet);

    // 设置server连接信息【0卡是server】
    std::unordered_map<uint32_t, ock::mf::transport::device::ConnectRankInfo> ranks;
    sockaddr_in net{};
    net.sin_family = AF_INET;
    net.sin_port = htons(K_PORT_8001);
    inet_pton(AF_INET, "192.168.1.1", &net.sin_addr);
    ock::mf::transport::TransportMemoryKey mk{};
    ranks.emplace(0, ock::mf::transport::device::ConnectRankInfo(HYBM_ROLE_PEER, net, mk));
    clientManager->SetRemoteRankInfo(ranks);

    // client客户端【2卡】连接 server【0卡】
    std::set<uint32_t> newServers = {0};
    int ret = clientManager->CreateConnectionToServers(newServers);
    // CreateLocalSocket依赖动态库接口，ut环境无法打通，肯定失败
    EXPECT_EQ(ret, BM_ERROR);
}
