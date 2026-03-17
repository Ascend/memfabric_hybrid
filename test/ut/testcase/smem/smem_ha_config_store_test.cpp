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
constexpr uint16_t K_STORE_PORT = 19000;
constexpr uint32_t K_DEFAULT_WORLD_SIZE = 4;
constexpr uint32_t K_RECOVERED_WORLD_SIZE = 8;
constexpr int32_t K_FOLLOWER_RANK_ID = -1;
constexpr int32_t K_UPDATED_RANK_ID = 9;
constexpr int16_t K_TEST_OP_CODE = 42;
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
    return SmMakeRef<TcpConfigStore>(nullptr, K_LOOPBACK_IP, K_STORE_PORT, false, worldSize, rankId);
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
    ConfigStoreServerOpHandler opHandler = [](const uint32_t, const std::string &, std::vector<uint8_t> &,
                                              const StoreBackendPtr &) { return SM_OK; };
    store.RegisterServerBrokenHandler(brokenHandler);
    store.RegisterServerOpHandler(K_TEST_OP_CODE, opHandler);

    MOCKER_CPP(&AccStoreServer::UpdateStatus, int32_t(*)(bool)).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&AccStoreServer::RestoreFromBackend, int32_t(*)()).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&AccStoreServer::Startup, int32_t(*)(const smem_tls_config &)).stubs().will(returnValue(int32_t(0)));

    store.StartServer();

    ASSERT_NE(nullptr, store.serverDelegate_.Get());
    EXPECT_TRUE(store.isLeader_.load(std::memory_order_acquire));
    EXPECT_EQ(K_RECOVERED_WORLD_SIZE, store.serverDelegate_->worldSize_);
    EXPECT_TRUE(static_cast<bool>(store.serverDelegate_->externalBrokenHandler_));
    EXPECT_EQ(1U, store.serverDelegate_->externalOpHandlerMap_.count(K_TEST_OP_CODE));

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
