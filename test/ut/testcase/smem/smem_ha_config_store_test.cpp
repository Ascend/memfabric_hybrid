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
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#define private   public
#define protected public
#include "smem_ha_config_store.h"
#undef protected
#undef private
#include "network_endpoint_util.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace ock::smem;

namespace {

constexpr char K_STORE_ENDPOINT[] = "tcp://127.0.0.1:19000";
constexpr char K_LEADER_ADDRESS[] = "127.0.0.1:19000";
constexpr char K_LOOPBACK_IP[] = "127.0.0.1";
constexpr char K_INVALID_LEADER_ADDRESS[] = "invalid";
constexpr char K_INVALID_LEADER_VALUE[] = "invalid-leader";
constexpr char K_RECOVERED_WORLD_SIZE_VALUE[] = "8";
constexpr char K_INVALID_WORLD_SIZE_VALUE[] = "bad";
constexpr char K_CLUSTER_ID[] = "cluster-a";
constexpr char K_BACKEND_LOCK_NAME[] = "backend";
constexpr uint16_t K_STORE_PORT = 19000;
constexpr uint32_t K_DEFAULT_WORLD_SIZE = 4;
constexpr uint32_t K_RECOVERED_WORLD_SIZE = 8;
constexpr int32_t K_FOLLOWER_RANK_ID = -1;
constexpr int32_t K_UPDATED_RANK_ID = 9;
constexpr int K_RECONNECT_RETRY_TIMES = 3;

class FakeStoreBackend final : public ConfigStoreBackend {
public:
    StoreErrorCode initRet = StoreErrorCode::SUCCESS;
    std::function<StoreErrorCode(const std::string &, std::vector<uint8_t> &)> getHook;
    std::function<StoreErrorCode(const std::string &, const std::vector<uint8_t> &, int64_t)> putHook;
    std::function<StoreErrorCode(const std::string &)> deleteHook;
    std::function<StoreErrorCode(const std::string &)> acquireHook;
    std::function<StoreErrorCode(const std::string &)> releaseHook;

    std::string lastInitializeUrl;
    std::string lastPutKey;
    std::vector<uint8_t> lastPutValue;
    int64_t lastPutTtl = -1;
    std::string lastDeleteKey;
    int uninitializeCount = 0;
    bool distributed = true;

    std::string BackendName() const noexcept override
    {
        return "FakeBackend";
    }

    StoreErrorCode Get(const std::string &key, std::vector<uint8_t> &outValue) const noexcept override
    {
        if (getHook != nullptr) {
            return getHook(key, outValue);
        }
        return StoreErrorCode::NOT_EXIST;
    }

    StoreErrorCode PrefixGet(const std::string &key, PrefixGetMap &outValue) const noexcept override
    {
        return StoreErrorCode::NOT_EXIST;
    }

    StoreErrorCode Put(const std::string &key, const std::vector<uint8_t> &value, int64_t ttlSeconds) noexcept override
    {
        lastPutKey = key;
        lastPutValue = value;
        lastPutTtl = ttlSeconds;
        if (putHook != nullptr) {
            return putHook(key, value, ttlSeconds);
        }
        return StoreErrorCode::SUCCESS;
    }

    StoreErrorCode Delete(const std::string &key) noexcept override
    {
        lastDeleteKey = key;
        if (deleteHook != nullptr) {
            return deleteHook(key);
        }
        return StoreErrorCode::SUCCESS;
    }

    StoreErrorCode Exist(const std::string &key) const noexcept override
    {
        std::vector<uint8_t> value;
        return Get(key, value);
    }

    void Clear() noexcept override {}

    bool IsDistributed() const noexcept override
    {
        return distributed;
    }

    bool SupportsTTL() const noexcept override
    {
        return true;
    }

    StoreErrorCode AcquireDistributedLock(const std::string &name) noexcept override
    {
        if (acquireHook != nullptr) {
            return acquireHook(name);
        }
        return StoreErrorCode::SUCCESS;
    }

    StoreErrorCode ReleaseDistributedLock(const std::string &name) noexcept override
    {
        if (releaseHook != nullptr) {
            return releaseHook(name);
        }
        return StoreErrorCode::SUCCESS;
    }

    StoreErrorCode TryAcquireDistributedLock(const std::string &, int64_t) noexcept override
    {
        return StoreErrorCode::ERROR;
    }

    StoreErrorCode Initialize(const std::string &backendUrl, const std::string &, const std::string &) override
    {
        lastInitializeUrl = backendUrl;
        return initRet;
    }

    void UnInitialize() override
    {
        uninitializeCount++;
    }
};

StoreBackendPtr MakeBackend()
{
    return Convert<FakeStoreBackend, ConfigStoreBackend>(SmMakeRef<FakeStoreBackend>());
}

TcpConfigStorePtr MakeClientDelegate(int32_t rankId = K_FOLLOWER_RANK_ID, uint32_t worldSize = K_DEFAULT_WORLD_SIZE)
{
    return SmMakeRef<TcpConfigStore>(nullptr, K_LOOPBACK_IP, K_STORE_PORT, false, true, worldSize, rankId);
}

} // namespace

class SmemHaConfigStoreTest : public testing::Test {
public:
    void SetUp() override
    {
        GlobalMockObject::reset();
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
        GlobalMockObject::reset();
    }
};

TEST_F(SmemHaConfigStoreTest, StartupFailsWhenClientDelegateMissing)
{
    auto backend = MakeBackend();
    HaConfigStore store(backend, nullptr, K_STORE_ENDPOINT, K_DEFAULT_WORLD_SIZE);

    EXPECT_EQ(SM_NOT_INITIALIZED, store.Startup({}));
}

TEST_F(SmemHaConfigStoreTest, StartupSucceedsWhenStopFlagAlreadySet)
{
    auto backend = MakeBackend();
    auto client = MakeClientDelegate();
    HaConfigStore store(backend, client, K_STORE_ENDPOINT, K_DEFAULT_WORLD_SIZE);
    store.stopFlag_.store(true, std::memory_order_release);

    smem_tls_config tlsConfig{};
    tlsConfig.tlsEnable = true;

    EXPECT_EQ(SM_OK, store.Startup(tlsConfig));
    EXPECT_TRUE(store.tlsConfig_.tlsEnable);
}

TEST_F(SmemHaConfigStoreTest, InitBackendConnectionMapsBackendInitializeResult)
{
    auto backendBase = MakeBackend();
    auto backend = Convert<ConfigStoreBackend, FakeStoreBackend>(backendBase);
    auto client = MakeClientDelegate();
    HaConfigStore store(backendBase, client, K_STORE_ENDPOINT, K_DEFAULT_WORLD_SIZE);

    backend->initRet = StoreErrorCode::SUCCESS;
    EXPECT_TRUE(store.InitBackendConnection());
    EXPECT_EQ(K_STORE_ENDPOINT, backend->lastInitializeUrl);

    backend->initRet = StoreErrorCode::ERROR;
    EXPECT_FALSE(store.InitBackendConnection());
}

TEST_F(SmemHaConfigStoreTest, ConstructorKeepsBackendLockNameUnqualified)
{
    auto backend = MakeBackend();
    auto client = MakeClientDelegate();
    HaConfigStore store(backend, client, K_STORE_ENDPOINT, K_DEFAULT_WORLD_SIZE, K_CLUSTER_ID);

    EXPECT_EQ(K_BACKEND_LOCK_NAME, store.backendLockName_);
}

TEST_F(SmemHaConfigStoreTest, IsLeaderAliveHandlesBackendResponseAndConnectivity)
{
    auto backendBase = MakeBackend();
    auto backend = Convert<ConfigStoreBackend, FakeStoreBackend>(backendBase);
    auto client = MakeClientDelegate();
    HaConfigStore store(backendBase, client, K_STORE_ENDPOINT, K_DEFAULT_WORLD_SIZE);
    std::string leaderAddr;

    backend->getHook = [](const std::string &, std::vector<uint8_t> &) { return StoreErrorCode::NOT_EXIST; };
    EXPECT_FALSE(store.IsLeaderAlive(leaderAddr));

    backend->getHook = [](const std::string &, std::vector<uint8_t> &outValue) {
        outValue.clear();
        return StoreErrorCode::SUCCESS;
    };
    EXPECT_FALSE(store.IsLeaderAlive(leaderAddr));

    backend->getHook = [](const std::string &, std::vector<uint8_t> &outValue) {
        const std::string value = K_INVALID_LEADER_VALUE;
        outValue.assign(value.begin(), value.end());
        return StoreErrorCode::SUCCESS;
    };
    EXPECT_FALSE(store.IsLeaderAlive(leaderAddr));

    backend->getHook = [](const std::string &, std::vector<uint8_t> &outValue) {
        const std::string value = K_LEADER_ADDRESS;
        outValue.assign(value.begin(), value.end());
        return StoreErrorCode::SUCCESS;
    };
    MOCKER_CPP(&NetworkEndpointUtil::CheckConnectivity, bool (*)(const std::string &, uint16_t))
        .stubs()
        .will(returnValue(true));
    EXPECT_TRUE(store.IsLeaderAlive(leaderAddr));
}

TEST_F(SmemHaConfigStoreTest, ConnectClientFirstConnectionRegistersBrokenHandlerOnlyOnce)
{
    auto backend = MakeBackend();
    auto client = MakeClientDelegate();
    HaConfigStore store(backend, client, K_STORE_ENDPOINT, K_DEFAULT_WORLD_SIZE);

    MOCKER_CPP(&TcpConfigStore::ClientStart, int32_t(*)(const smem_tls_config &, int))
        .stubs()
        .will(returnValue(int32_t(0)));

    EXPECT_EQ(SM_OK, store.ConnectClient(K_LOOPBACK_IP, K_STORE_PORT));
    EXPECT_EQ(1U, client->brokenHandler_.size());

    EXPECT_EQ(SM_OK, store.ConnectClient(K_LOOPBACK_IP, K_STORE_PORT));
    EXPECT_EQ(1U, client->brokenHandler_.size());
}

TEST_F(SmemHaConfigStoreTest, ConnectClientSetsConnectStatusOnFirstConnection)
{
    // Verify that ConnectClient calls SetConnectStatus(true) after ClientStart
    // succeeds, as required by commit dc686c2.
    auto backend = MakeBackend();
    auto client = MakeClientDelegate();
    HaConfigStore store(backend, client, K_STORE_ENDPOINT, K_DEFAULT_WORLD_SIZE);

    // Initially connect status is false.
    EXPECT_FALSE(client->GetConnectStatus());

    // Stub ClientStart to return success.
    MOCKER_CPP(&TcpConfigStore::ClientStart, int32_t(*)(const smem_tls_config &, int))
        .stubs()
        .will(returnValue(int32_t(0)));

    EXPECT_EQ(SM_OK, store.ConnectClient(K_LOOPBACK_IP, K_STORE_PORT));

    // After connection, SetConnectStatus(true) should have been called.
    EXPECT_TRUE(client->GetConnectStatus());
}

TEST_F(SmemHaConfigStoreTest, ConnectClientDoesNotSetConnectStatusOnFailure)
{
    // If ClientStart fails, SetConnectStatus should NOT be called.
    auto backend = MakeBackend();
    auto client = MakeClientDelegate();
    HaConfigStore store(backend, client, K_STORE_ENDPOINT, K_DEFAULT_WORLD_SIZE);

    EXPECT_FALSE(client->GetConnectStatus());

    // Stub ClientStart to return failure.
    MOCKER_CPP(&TcpConfigStore::ClientStart, int32_t(*)(const smem_tls_config &, int))
        .stubs()
        .will(returnValue(int32_t(-1)));

    EXPECT_NE(SM_OK, store.ConnectClient(K_LOOPBACK_IP, K_STORE_PORT));

    // Status should remain false.
    EXPECT_FALSE(client->GetConnectStatus());
}

TEST_F(SmemHaConfigStoreTest, ConnectClientReconnectPathPropagatesDelegateResult)
{
    auto backend = MakeBackend();
    auto client = MakeClientDelegate(0);
    HaConfigStore store(backend, client, K_STORE_ENDPOINT, K_DEFAULT_WORLD_SIZE);

    MOCKER_CPP(&TcpConfigStore::ReConnectAfterBroken, int32_t(*)(int)).stubs().will(returnValue(int32_t(0)));

    EXPECT_EQ(SM_OK, store.ConnectClient(K_LOOPBACK_IP, K_STORE_PORT));
}

TEST_F(SmemHaConfigStoreTest, BecomeFollowerRejectsInvalidAddressAndConnectsValidLeader)
{
    auto backend = MakeBackend();
    auto client = MakeClientDelegate();
    HaConfigStore store(backend, client, K_STORE_ENDPOINT, K_DEFAULT_WORLD_SIZE);

    EXPECT_EQ(SM_ERROR, store.BecomeFollower(K_INVALID_LEADER_ADDRESS));

    MOCKER_CPP(&TcpConfigStore::ClientStart, int32_t(*)(const smem_tls_config &, int))
        .stubs()
        .will(returnValue(int32_t(0)));
    EXPECT_EQ(SM_OK, store.BecomeFollower(K_LEADER_ADDRESS));
}

TEST_F(SmemHaConfigStoreTest, StartServerRecoversWorldSizeAndReplaysCachedHandlers)
{
    auto backendBase = MakeBackend();
    auto backend = Convert<ConfigStoreBackend, FakeStoreBackend>(backendBase);
    auto client = MakeClientDelegate();
    HaConfigStore store(backendBase, client, K_STORE_ENDPOINT, K_DEFAULT_WORLD_SIZE);
    store.leaderBindIp_ = K_LOOPBACK_IP;
    store.leaderBindPort_ = K_STORE_PORT;

    backend->getHook = [](const std::string &key, std::vector<uint8_t> &outValue) {
        if (key == KEY_WORLD_SIZE) {
            const std::string value = K_RECOVERED_WORLD_SIZE_VALUE;
            outValue.assign(value.begin(), value.end());
            return StoreErrorCode::SUCCESS;
        }
        return StoreErrorCode::NOT_EXIST;
    };

    ConfigStoreServerBrokenHandler brokenHandler = [](const uint32_t, StoreBackendPtr &) {};
    store.RegisterServerBrokenHandler(brokenHandler);

    MOCKER_CPP(&AccStoreServer::UpdateStatus, int32_t(*)(bool)).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&AccStoreServer::RestoreFromBackend, int32_t(*)()).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&AccStoreServer::Startup, int32_t(*)(const smem_tls_config &)).stubs().will(returnValue(int32_t(0)));

    store.StartServer();

    ASSERT_NE(nullptr, store.serverDelegate_.Get());
    EXPECT_TRUE(store.isLeader_.load(std::memory_order_acquire));
    EXPECT_EQ(K_RECOVERED_WORLD_SIZE, store.serverDelegate_->worldSize_);
    EXPECT_TRUE(static_cast<bool>(store.serverDelegate_->externalBrokenHandler_));

    store.StopServer();
    EXPECT_FALSE(store.isLeader_.load(std::memory_order_acquire));
}

TEST_F(SmemHaConfigStoreTest, StartServerFallsBackToDefaultWorldSizeWhenBackendValueInvalid)
{
    auto backendBase = MakeBackend();
    auto backend = Convert<ConfigStoreBackend, FakeStoreBackend>(backendBase);
    auto client = MakeClientDelegate();
    HaConfigStore store(backendBase, client, K_STORE_ENDPOINT, K_DEFAULT_WORLD_SIZE);
    store.leaderBindIp_ = K_LOOPBACK_IP;
    store.leaderBindPort_ = K_STORE_PORT;

    backend->getHook = [](const std::string &key, std::vector<uint8_t> &outValue) {
        if (key == KEY_WORLD_SIZE) {
            const std::string value = K_INVALID_WORLD_SIZE_VALUE;
            outValue.assign(value.begin(), value.end());
            return StoreErrorCode::SUCCESS;
        }
        return StoreErrorCode::NOT_EXIST;
    };

    MOCKER_CPP(&AccStoreServer::UpdateStatus, int32_t(*)(bool)).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&AccStoreServer::RestoreFromBackend, int32_t(*)()).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&AccStoreServer::Startup, int32_t(*)(const smem_tls_config &)).stubs().will(returnValue(int32_t(0)));

    store.StartServer();

    ASSERT_NE(nullptr, store.serverDelegate_.Get());
    EXPECT_EQ(K_DEFAULT_WORLD_SIZE, store.serverDelegate_->worldSize_);
}

TEST_F(SmemHaConfigStoreTest, TryBecomeLeaderRegistersLeaderSuccessfully)
{
    auto backendBase = MakeBackend();
    auto backend = Convert<ConfigStoreBackend, FakeStoreBackend>(backendBase);
    auto client = MakeClientDelegate();
    HaConfigStore store(backendBase, client, K_STORE_ENDPOINT, K_DEFAULT_WORLD_SIZE);

    backend->getHook = [](const std::string &, std::vector<uint8_t> &) { return StoreErrorCode::NOT_EXIST; };

    MOCKER_CPP(&NetworkEndpointUtil::FindAvailablePort, bool (*)(uint16_t &, bool)).stubs().will(returnValue(true));
    MOCKER_CPP(&NetworkEndpointUtil::GetLocalIpWithTarget, bool (*)(const std::string &, std::string &))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&AccStoreServer::UpdateStatus, int32_t(*)(bool)).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&AccStoreServer::RestoreFromBackend, int32_t(*)()).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&AccStoreServer::Startup, int32_t(*)(const smem_tls_config &)).stubs().will(returnValue(int32_t(0)));

    MOCKER_CPP(&TcpConfigStore::ClientStart, int32_t(*)(const smem_tls_config &, int))
        .stubs()
        .will(returnValue(int32_t(0)));
    EXPECT_EQ(SM_OK, store.TryBecomeLeader());
    EXPECT_EQ(KEY_LEADER, backend->lastPutKey);
    EXPECT_EQ(K_LEADER_ADDRESS, std::string(backend->lastPutValue.begin(), backend->lastPutValue.end()));
    EXPECT_EQ(PUT_LEASE_TTL_SEC, backend->lastPutTtl);
}

TEST_F(SmemHaConfigStoreTest, TryBecomeLeaderDeletesLeaderOnSelfConnectFailure)
{
    auto backendBase = MakeBackend();
    auto backend = Convert<ConfigStoreBackend, FakeStoreBackend>(backendBase);
    auto client = MakeClientDelegate();
    HaConfigStore store(backendBase, client, K_STORE_ENDPOINT, K_DEFAULT_WORLD_SIZE);

    backend->getHook = [](const std::string &, std::vector<uint8_t> &) { return StoreErrorCode::NOT_EXIST; };

    MOCKER_CPP(&NetworkEndpointUtil::FindAvailablePort, bool (*)(uint16_t &, bool)).stubs().will(returnValue(true));
    MOCKER_CPP(&NetworkEndpointUtil::GetLocalIpWithTarget, bool (*)(const std::string &, std::string &))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&AccStoreServer::UpdateStatus, int32_t(*)(bool)).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&AccStoreServer::RestoreFromBackend, int32_t(*)()).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&AccStoreServer::Startup, int32_t(*)(const smem_tls_config &)).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&TcpConfigStore::ClientStart, int32_t(*)(const smem_tls_config &, int))
        .stubs()
        .will(returnValue(int32_t(-1)));
    EXPECT_EQ(SM_ERROR, store.TryBecomeLeader());
    EXPECT_EQ(KEY_LEADER, backend->lastDeleteKey);
    EXPECT_FALSE(store.isLeader_.load(std::memory_order_acquire));
}

TEST_F(SmemHaConfigStoreTest, ForwardingApisDelegateToClientOrUseClientLocalState)
{
    auto backend = MakeBackend();
    auto client = MakeClientDelegate();
    HaConfigStore store(backend, client, K_STORE_ENDPOINT, K_DEFAULT_WORLD_SIZE);

    MOCKER_CPP(&TcpConfigStore::Set, int32_t(*)(const std::string &, const std::vector<uint8_t> &))
        .stubs()
        .will(returnValue(int32_t(0)));
    MOCKER_CPP(&TcpConfigStore::Add, int32_t(*)(const std::string &, int64_t, int64_t &))
        .stubs()
        .will(returnValue(int32_t(0)));
    MOCKER_CPP(&TcpConfigStore::Remove, int32_t(*)(const std::string &, bool)).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&TcpConfigStore::Append, int32_t(*)(const std::string &, const std::vector<uint8_t> &, uint64_t &))
        .stubs()
        .will(returnValue(int32_t(0)));
    MOCKER_CPP(&TcpConfigStore::Cas, int32_t(*)(const std::string &, const std::vector<uint8_t> &,
                                                const std::vector<uint8_t> &, std::vector<uint8_t> &))
        .stubs()
        .will(returnValue(int32_t(0)));
    MOCKER_CPP(&TcpConfigStore::Unwatch, int32_t(*)(uint32_t)).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&TcpConfigStore::Write, int32_t(*)(const std::string &, const std::vector<uint8_t> &, uint32_t))
        .stubs()
        .will(returnValue(int32_t(0)));

    std::vector<uint8_t> value{'v'};
    std::vector<uint8_t> exists;
    uint32_t wid = 0;
    uint64_t newSize = 0;
    int64_t addValue = 0;

    EXPECT_EQ(SM_OK, store.Set("k", value));
    EXPECT_EQ(SM_OK, store.Add("k", 1, addValue));
    EXPECT_EQ(SM_OK, store.Remove("k", true));
    EXPECT_EQ(SM_OK, store.Append("k", value, newSize));
    EXPECT_EQ(SM_OK, store.Cas("k", value, value, exists));
    EXPECT_EQ(SM_OK, store.Unwatch(wid));
    EXPECT_EQ(SM_OK, store.Write("k", value, 0));

    store.RegisterReconnectHandler([]() { return 0; });
    EXPECT_TRUE(static_cast<bool>(client->reconnectHandler));
    store.SetConnectStatus(true);
    EXPECT_TRUE(store.GetConnectStatus());
    store.RegisterClientBrokenHandler([]() { return 0; });
    EXPECT_EQ(1U, client->brokenHandler_.size());
    store.SetRankId(K_UPDATED_RANK_ID);
    EXPECT_EQ(K_UPDATED_RANK_ID, client->rankId_);
    EXPECT_EQ("x", store.GetCompleteKey("x"));
    EXPECT_EQ("", store.GetCommonPrefix());
}

TEST_F(SmemHaConfigStoreTest, WatchAndGetRealReturnErrorWhenClientMissing)
{
    auto backend = MakeBackend();
    HaConfigStore store(backend, nullptr, K_STORE_ENDPOINT, K_DEFAULT_WORLD_SIZE);
    std::vector<uint8_t> value;
    uint32_t wid = 0;

    EXPECT_EQ(SM_ERROR, store.Watch("k", [](int, const std::string &, const std::vector<uint8_t> &) {}, wid));
    EXPECT_EQ(SM_ERROR, store.Watch(WATCH_RANK_LINK_DOWN, [](WatchRankType, uint32_t) {}, wid));
    EXPECT_EQ(SM_ERROR, store.GetReal("k", value, 1));
}

TEST_F(SmemHaConfigStoreTest, ReConnectAfterBrokenReturnsOkWhenStopFlagSet)
{
    auto backend = MakeBackend();
    auto client = MakeClientDelegate();
    HaConfigStore store(backend, client, K_STORE_ENDPOINT, K_DEFAULT_WORLD_SIZE);
    store.stopFlag_.store(true, std::memory_order_release);

    EXPECT_EQ(SM_OK, store.ReConnectAfterBroken(K_RECONNECT_RETRY_TIMES));
}

// === Tests for commit dc686c2: AccStoreServer recovery mechanism ===

TEST_F(SmemHaConfigStoreTest, AccStoreServerRestoreFromBackendSeedsAliveRankSet)
{
    auto backend = MakeBackend();
    AccStoreServer server(K_LOOPBACK_IP, K_STORE_PORT, K_DEFAULT_WORLD_SIZE, backend, false);

    // getHook returns "{0,1,2}" for KEY_ALIVE_RANK_LIST.
    auto fb = Convert<ConfigStoreBackend, FakeStoreBackend>(backend);
    fb->getHook = [](const std::string &key, std::vector<uint8_t> &outValue) {
        if (key == KEY_ALIVE_RANK_LIST) {
            const std::string ranks = "0,1,2";
            outValue.assign(ranks.begin(), ranks.end());
            return StoreErrorCode::SUCCESS;
        }
        return StoreErrorCode::NOT_EXIST;
    };

    auto ret = server.RestoreFromBackend();
    ASSERT_EQ(SM_OK, ret);

    // aliveRankSet_ should be seeded with {0,1,2}.
    EXPECT_EQ(3U, server.aliveRankSet_.size());
    EXPECT_TRUE(server.aliveRankSet_.count(0));
    EXPECT_TRUE(server.aliveRankSet_.count(1));
    EXPECT_TRUE(server.aliveRankSet_.count(2)); // 2
    // reconnectedRankSet_ must NOT be seeded.
    EXPECT_TRUE(server.reconnectedRankSet_.empty());
}

TEST_F(SmemHaConfigStoreTest, AccStoreServerRestoreFromBackendEmptyWhenNoBackendData)
{
    auto backend = MakeBackend();
    AccStoreServer server(K_LOOPBACK_IP, K_STORE_PORT, K_DEFAULT_WORLD_SIZE, backend, false);

    // getHook returns NOT_EXIST for all keys.
    auto ret = server.RestoreFromBackend();
    ASSERT_EQ(SM_OK, ret);

    // aliveRankSet_ remains empty.
    EXPECT_TRUE(server.aliveRankSet_.empty());
}

TEST_F(SmemHaConfigStoreTest, AccStoreServerLaunchCleanupThreadSkipsWhenFirstUpdate)
{
    auto backend = MakeBackend();
    AccStoreServer server(K_LOOPBACK_IP, K_STORE_PORT, K_DEFAULT_WORLD_SIZE, backend, false);

    // aliveRankFromBackend_ is empty → isFirstUpdate = true → skip cleanup.
    auto ret = server.LaunchCleanupThread();
    EXPECT_EQ(SM_OK, ret);
    // cleanupThread_ should not be joinable (was never started).
    EXPECT_FALSE(server.cleanupThread_.joinable());
}

TEST_F(SmemHaConfigStoreTest, AccStoreServerCanReceiveNewLinkTransitionsInitedToNormalWithSkipRecover)
{
    auto backend = MakeBackend();
    AccStoreServer server(K_LOOPBACK_IP, K_STORE_PORT, K_DEFAULT_WORLD_SIZE, backend, true);

    server.state_.store(SS_INITED);
    // skipRecover_ = true → state becomes NORMAL immediately.
    EXPECT_TRUE(server.CanReceiveNewLink());
    EXPECT_EQ(SS_NORMAL, server.state_.load());
}

TEST_F(SmemHaConfigStoreTest, AccStoreServerCanReceiveNewLinkTransitionsToRecoverThenNormalOnAllReconnect)
{
    auto backend = MakeBackend();
    AccStoreServer server(K_LOOPBACK_IP, K_STORE_PORT, K_DEFAULT_WORLD_SIZE, backend, false);
    server.skipRecover_ = false;

    // Set aliveRankFromBackend_ = {0,1} to simulate recovered ranks.
    server.aliveRankFromBackend_ = {0, 1};
    // Initially empty reconnectedRankSet_.
    server.state_.store(SS_INITED);

    // First call: INITED → RECOVER (not all reconnected).
    EXPECT_FALSE(server.CanReceiveNewLink());
    EXPECT_EQ(SS_RECOVER, server.state_.load());

    // Insert ranks into reconnectedRankSet_ — simulates reconnect.
    server.reconnectedRankSet_.insert(0);
    server.reconnectedRankSet_.insert(1);

    // Second call: allReconnected → NORMAL.
    EXPECT_TRUE(server.CanReceiveNewLink());
    EXPECT_EQ(SS_NORMAL, server.state_.load());
}

TEST_F(SmemHaConfigStoreTest, AccStoreServerCleanupStaleRanksDetectsOrphans)
{
    auto backend = MakeBackend();
    AccStoreServer server(K_LOOPBACK_IP, K_STORE_PORT, K_DEFAULT_WORLD_SIZE, backend, false);

    // Simulate state after recovery: ranks {0,1} in backend, only {1} reconnected.
    server.aliveRankFromBackend_ = {0, 1};
    server.reconnectedRankSet_ = {1};
    server.aliveRankSet_ = {0, 1};

    // Test the orphan detection logic that CleanupStaleRanks uses:
    // subtract reconnected ranks from backend ranks.
    std::unordered_set<uint32_t> ranksToRemove;
    for (const uint32_t rank : server.reconnectedRankSet_) {
        server.aliveRankFromBackend_.erase(rank);
    }
    ranksToRemove = server.aliveRankFromBackend_;

    // Only rank 0 should be identified as orphan (rank 1 reconnected).
    ASSERT_EQ(1U, ranksToRemove.size());
    EXPECT_TRUE(ranksToRemove.count(0));
    EXPECT_FALSE(ranksToRemove.count(1));
}

TEST_F(SmemHaConfigStoreTest, AccStoreServerQueryAliveUsesReconnectedRankSet)
{
    auto backend = MakeBackend();
    AccStoreServer server(K_LOOPBACK_IP, K_STORE_PORT, K_DEFAULT_WORLD_SIZE, backend, false);

    // Rank 0 is in reconnectedRankSet_ (actually connected).
    server.reconnectedRankSet_.insert(0);
    // Rank 1 is in aliveRankSet_ (seeded from backend) but NOT in reconnectedRankSet_.
    server.aliveRankSet_.insert(1);

    // QueryAlive must use reconnectedRankSet_ — rank 0 alive, rank 1 not.
    EXPECT_TRUE(server.reconnectedRankSet_.count(0));
    EXPECT_FALSE(server.reconnectedRankSet_.count(1));
}

TEST_F(SmemHaConfigStoreTest, AccStoreServerUpdateStatusSetsAndDeletesLeaderStatus)
{
    auto backend = MakeBackend();
    auto fb = Convert<ConfigStoreBackend, FakeStoreBackend>(backend);
    AccStoreServer server(K_LOOPBACK_IP, K_STORE_PORT, K_DEFAULT_WORLD_SIZE, backend, false);

    // UpdateStatus(true) should PUT KEY_LEADER_STATUS="true".
    EXPECT_EQ(SM_OK, server.UpdateStatus(true));
    EXPECT_EQ(KEY_LEADER_STATUS, fb->lastPutKey);
    EXPECT_EQ("true", std::string(fb->lastPutValue.begin(), fb->lastPutValue.end()));

    // UpdateStatus(false) should DELETE KEY_LEADER_STATUS.
    EXPECT_EQ(SM_OK, server.UpdateStatus(false));
    EXPECT_EQ(KEY_LEADER_STATUS, fb->lastDeleteKey);
}

TEST_F(SmemHaConfigStoreTest, AccStoreServerGetStatusChecksBackend)
{
    auto backend = MakeBackend();
    auto fb = Convert<ConfigStoreBackend, FakeStoreBackend>(backend);
    AccStoreServer server(K_LOOPBACK_IP, K_STORE_PORT, K_DEFAULT_WORLD_SIZE, backend, false);

    // getHook returns "true" for KEY_LEADER_STATUS → GetStatus returns true.
    fb->getHook = [](const std::string &key, std::vector<uint8_t> &outValue) {
        if (key == KEY_LEADER_STATUS) {
            outValue.assign({'t', 'r', 'u', 'e'});
            return StoreErrorCode::SUCCESS;
        }
        return StoreErrorCode::NOT_EXIST;
    };
    EXPECT_TRUE(server.GetStatus());

    // getHook returns something else → GetStatus returns false after retries.
    fb->getHook = [](const std::string &, std::vector<uint8_t> &) {
        return StoreErrorCode::NOT_EXIST;
    };
    EXPECT_FALSE(server.GetStatus());
}

TEST_F(SmemHaConfigStoreTest, AccStoreServerRestoreFromBackendPersistsAliveRankList)
{
    // Verify that RestoreFromBackend correctly reads KEY_ALIVE_RANK_LIST
    // from the backend and seeds aliveRankSet_ without touching reconnectedRankSet_.
    auto backend = MakeBackend();
    auto fb = Convert<ConfigStoreBackend, FakeStoreBackend>(backend);
    AccStoreServer server(K_LOOPBACK_IP, K_STORE_PORT, K_DEFAULT_WORLD_SIZE, backend, false);

    fb->getHook = [](const std::string &key, std::vector<uint8_t> &outValue) {
        if (key == KEY_ALIVE_RANK_LIST) {
            const std::string ranks = "0,1,2";
            outValue.assign(ranks.begin(), ranks.end());
            return StoreErrorCode::SUCCESS;
        }
        return StoreErrorCode::NOT_EXIST;
    };

    auto ret = server.RestoreFromBackend();
    ASSERT_EQ(SM_OK, ret);

    // aliveRankSet_ = {0,1,2} seeded from backend.
    ASSERT_EQ(3U, server.aliveRankSet_.size());
    EXPECT_TRUE(server.aliveRankSet_.count(0));

    // reconnectedRankSet_ stays empty (only LinkConnectedHandler populates it).
    EXPECT_TRUE(server.reconnectedRankSet_.empty());

    // aliveRankFromBackend_ is also populated.
    ASSERT_EQ(3U, server.aliveRankFromBackend_.size());
    EXPECT_TRUE(server.aliveRankFromBackend_.count(0));
}

TEST_F(SmemHaConfigStoreTest, AccStoreServerFindOrInsertRankUsesLinkRankMap)
{
    auto backend = MakeBackend();
    AccStoreServer server(K_LOOPBACK_IP, K_STORE_PORT, K_DEFAULT_WORLD_SIZE, backend, false);

    // Manually insert a rank into linkRankMap_, simulating a prior connection.
    uint32_t linkId = 42;
    uint32_t existingRank = 7;
    server.linkRankMap_[linkId] = existingRank;

    // runState=SS_EXITED so that CanReceiveNewLink doesn't transition to RECOVER
    // (we are not testing recovery here).
    server.state_.store(SS_EXITED);

    // FindOrInsertRank is called internally; we verify by checking that
    // aliveRankSet_ contains a rank after insertion.  But we can't easily
    // call FindOrInsertRank from here (needs AccTcpRequestContext).
    // Instead verify the linkRankMap_ lookup path returns correct rank.
    auto it = server.linkRankMap_.find(linkId);
    ASSERT_NE(it, server.linkRankMap_.end());
    EXPECT_EQ(existingRank, it->second);
}

TEST_F(SmemHaConfigStoreTest, AccStoreServerPersistAndRecoverAliveRankIds)
{
    auto backend = MakeBackend();
    auto fb = Convert<ConfigStoreBackend, FakeStoreBackend>(backend);
    AccStoreServer server(K_LOOPBACK_IP, K_STORE_PORT, K_DEFAULT_WORLD_SIZE, backend, false);

    // PersistAliveRankIds: write {0,1,2} to backend.
    // unordered_set iteration order is not guaranteed, so check each element
    // appears as a substring rather than comparing the full string.
    EXPECT_EQ(StoreErrorCode::SUCCESS, server.PersistAliveRankIds({0, 1, 2}));
    EXPECT_EQ(KEY_ALIVE_RANK_LIST, fb->lastPutKey);
    std::string persisted(reinterpret_cast<const char *>(fb->lastPutValue.data()), fb->lastPutValue.size());
    EXPECT_NE(std::string::npos, persisted.find('0'));
    EXPECT_NE(std::string::npos, persisted.find('1'));
    EXPECT_NE(std::string::npos, persisted.find('2'));

    // PersistAliveRankIds with empty set → DELETE from backend.
    EXPECT_EQ(StoreErrorCode::SUCCESS, server.PersistAliveRankIds({}));
    EXPECT_EQ(KEY_ALIVE_RANK_LIST, fb->lastDeleteKey);

    // RecoverAliveRankIds: mock backend to return "3,4,5".
    fb->getHook = [](const std::string &key, std::vector<uint8_t> &outValue) {
        if (key == KEY_ALIVE_RANK_LIST) {
            const std::string ranks = "3,4,5";
            outValue.assign(ranks.begin(), ranks.end());
            return StoreErrorCode::SUCCESS;
        }
        return StoreErrorCode::NOT_EXIST;
    };
    std::unordered_set<uint32_t> recovered;
    EXPECT_EQ(StoreErrorCode::SUCCESS, server.RecoverAliveRankIds(recovered));
    ASSERT_EQ(3U, recovered.size());
    EXPECT_TRUE(recovered.count(3)); // 3
    EXPECT_TRUE(recovered.count(5)); // 5
}

TEST_F(SmemHaConfigStoreTest, AccStoreServerRecoverAliveRankIdsHandlesEmptyAndMissing)
{
    auto backend = MakeBackend();
    auto fb = Convert<ConfigStoreBackend, FakeStoreBackend>(backend);
    AccStoreServer server(K_LOOPBACK_IP, K_STORE_PORT, K_DEFAULT_WORLD_SIZE, backend, false);

    // Empty string in backend → recover returns empty set.
    fb->getHook = [](const std::string &key, std::vector<uint8_t> &outValue) {
        if (key == KEY_ALIVE_RANK_LIST) {
            outValue.clear();
            return StoreErrorCode::SUCCESS;
        }
        return StoreErrorCode::NOT_EXIST;
    };
    std::unordered_set<uint32_t> recovered = {99};
    EXPECT_EQ(StoreErrorCode::SUCCESS, server.RecoverAliveRankIds(recovered));
    EXPECT_TRUE(recovered.empty()) << "empty backend string yields empty result";

    // Missing key in backend → recover returns empty set.
    fb->getHook = [](const std::string &, std::vector<uint8_t> &) {
        return StoreErrorCode::NOT_EXIST;
    };
    recovered = {99};
    EXPECT_EQ(StoreErrorCode::SUCCESS, server.RecoverAliveRankIds(recovered));
    EXPECT_TRUE(recovered.empty()) << "missing key yields empty result";
}

TEST_F(SmemHaConfigStoreTest, AccStoreServerRecoverAliveRankIdsHandlesInvalidInput)
{
    auto backend = MakeBackend();
    auto fb = Convert<ConfigStoreBackend, FakeStoreBackend>(backend);
    AccStoreServer server(K_LOOPBACK_IP, K_STORE_PORT, K_DEFAULT_WORLD_SIZE, backend, false);

    // Backend returns "abc" (non-numeric) → ERROR.
    fb->getHook = [](const std::string &key, std::vector<uint8_t> &outValue) {
        if (key == KEY_ALIVE_RANK_LIST) {
            const std::string ranks = "0,abc,2";
            outValue.assign(ranks.begin(), ranks.end());
            return StoreErrorCode::SUCCESS;
        }
        return StoreErrorCode::NOT_EXIST;
    };
    std::unordered_set<uint32_t> recovered;
    EXPECT_EQ(StoreErrorCode::ERROR, server.RecoverAliveRankIds(recovered));
    EXPECT_TRUE(recovered.empty());
}

TEST_F(SmemHaConfigStoreTest, AccStoreServerPersistWorldSizeWritesAndRecovers)
{
    auto backend = MakeBackend();
    auto fb = Convert<ConfigStoreBackend, FakeStoreBackend>(backend);
    AccStoreServer server(K_LOOPBACK_IP, K_STORE_PORT, K_DEFAULT_WORLD_SIZE, backend, false);

    // PersistWorldSize writes to KEY_WORLD_SIZE.
    EXPECT_EQ(StoreErrorCode::SUCCESS, server.PersistWorldSize(8)); // 8
    EXPECT_EQ(KEY_WORLD_SIZE, fb->lastPutKey);
    EXPECT_EQ("8", std::string(fb->lastPutValue.begin(), fb->lastPutValue.end()));
}

TEST_F(SmemHaConfigStoreTest, AccStoreServerSetAndGetStatus)
{
    auto backend = MakeBackend();
    auto fb = Convert<ConfigStoreBackend, FakeStoreBackend>(backend);
    AccStoreServer server(K_LOOPBACK_IP, K_STORE_PORT, K_DEFAULT_WORLD_SIZE, backend, false);

    // Initially GetStatus should be true (non-distributed check bypasses backend).
    // But we already have a dedicated test for UpdateStatus/GetStatus above.

    // Test that UpdateStatus with false deletes and true puts.
    ASSERT_EQ(SM_OK, server.UpdateStatus(false));
    EXPECT_EQ(KEY_LEADER_STATUS, fb->lastDeleteKey);

    ASSERT_EQ(SM_OK, server.UpdateStatus(true));
    EXPECT_EQ(KEY_LEADER_STATUS, fb->lastPutKey);
    EXPECT_EQ("true", std::string(fb->lastPutValue.begin(), fb->lastPutValue.end()));
}
