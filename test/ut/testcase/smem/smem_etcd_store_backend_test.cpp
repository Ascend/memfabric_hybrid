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

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#define private public
#include "dl_etcd_api.h"
#include "smem_etcd_client.h"
#undef private

#include "smem_etcd_store_backend.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace ock::smem;

namespace {

constexpr char K_ETCD_BACKEND_URL[] = "etcd://127.0.0.1:2379";
constexpr char K_TCP_BACKEND_URL[] = "tcp://127.0.0.1:2379";
constexpr char K_IPV6_ETCD_BACKEND_URL[] = "etcd://[::1]:2379";
constexpr char K_HTTP_IPV4_ENDPOINT[] = "http://127.0.0.1:2379";
constexpr char K_HTTP_IPV6_ENDPOINT[] = "http://[::1]:2379";
constexpr char K_USER_NAME[] = "user";
constexpr char K_PASSWORD[] = "pwd";
constexpr int32_t K_MOCK_FAILURE = -1;
constexpr int64_t K_TRY_LOCK_TIMEOUT_MS = 10;
constexpr int64_t K_PUT_TTL_SECONDS = 3;
constexpr int K_DOUBLE_CLOSE_CALL_COUNT = 2;

struct FakeEtcdClientState {
    std::unordered_map<std::string, std::string> kv;
    std::string lastError = "fake-etcd-error";
    bool locked = false;
    int getRet = 0;
    int putRet = 0;
    int deleteRet = 0;
    int lockRet = 0;
    int unlockRet = 0;
};

struct FakeEtcdEnv {
    std::string lastEndpoints;
    int newCallCount = 0;
    int closeCallCount = 0;
    FakeEtcdClientState *lastClient = nullptr;
} g_fakeEtcdEnv;

EtcdClient *FakeEtcdNew(const char *endpoints, const char *, const char *, int)
{
    auto *state = new FakeEtcdClientState();
    g_fakeEtcdEnv.lastEndpoints = endpoints == nullptr ? "" : endpoints;
    g_fakeEtcdEnv.newCallCount++;
    g_fakeEtcdEnv.lastClient = state;
    return reinterpret_cast<EtcdClient *>(state);
}

void FakeEtcdClose(EtcdClient *client)
{
    g_fakeEtcdEnv.closeCallCount++;
    delete reinterpret_cast<FakeEtcdClientState *>(client);
}

char *FakeEtcdGetLastError(EtcdClient *client)
{
    auto *state = reinterpret_cast<FakeEtcdClientState *>(client);
    return const_cast<char *>(state->lastError.c_str());
}

int FakeEtcdPut(EtcdClient *client, const char *key, const void *value, size_t valueLen, int64_t)
{
    auto *state = reinterpret_cast<FakeEtcdClientState *>(client);
    if (state->putRet != 0) {
        return state->putRet;
    }
    state->kv[key] = std::string(reinterpret_cast<const char *>(value), valueLen);
    return 0;
}

int FakeEtcdGet(EtcdClient *client, const char *key, char **outValue, size_t *outValueLen)
{
    auto *state = reinterpret_cast<FakeEtcdClientState *>(client);
    if (state->getRet != 0) {
        return state->getRet;
    }
    auto it = state->kv.find(key);
    if (it == state->kv.end()) {
        state->lastError = "not found";
        return -1;
    }
    const auto &value = it->second;
    auto *buffer = new char[value.size()];
    if (!value.empty()) {
        std::memcpy(buffer, value.data(), value.size());
    }
    *outValue = buffer;
    *outValueLen = value.size();
    return 0;
}

void FakeEtcdFreeValue(const char *value)
{
    delete[] value;
}

int FakeEtcdRemove(EtcdClient *client, const char *key)
{
    auto *state = reinterpret_cast<FakeEtcdClientState *>(client);
    if (state->deleteRet != 0) {
        return state->deleteRet;
    }
    return state->kv.erase(key) > 0 ? 0 : -1;
}

int FakeEtcdLock(EtcdClient *client)
{
    auto *state = reinterpret_cast<FakeEtcdClientState *>(client);
    if (state->lockRet != 0) {
        return state->lockRet;
    }
    state->locked = true;
    return 0;
}

int FakeEtcdUnlock(EtcdClient *client)
{
    auto *state = reinterpret_cast<FakeEtcdClientState *>(client);
    if (state->unlockRet != 0) {
        return state->unlockRet;
    }
    state->locked = false;
    return 0;
}

void ResetFakeEtcdApi()
{
    g_fakeEtcdEnv = {};
    EtcdApi::etcdNew_ = FakeEtcdNew;
    EtcdApi::etcdClose_ = FakeEtcdClose;
    EtcdApi::etcdGetLastError_ = FakeEtcdGetLastError;
    EtcdApi::etcdPut_ = FakeEtcdPut;
    EtcdApi::etcdGet_ = FakeEtcdGet;
    EtcdApi::etcdFreeValue_ = FakeEtcdFreeValue;
    EtcdApi::etcdRemove_ = FakeEtcdRemove;
    EtcdApi::etcdLock_ = FakeEtcdLock;
    EtcdApi::etcdUnLock_ = FakeEtcdUnlock;
}

} // namespace

class SmemEtcdStoreBackendTest : public testing::Test {
public:
    void SetUp() override
    {
        GlobalMockObject::reset();
        EtcdClientV3::GetInstance().Close();
        ResetFakeEtcdApi();
    }

    void TearDown() override
    {
        EtcdClientV3::GetInstance().Close();
        GlobalMockObject::verify();
        GlobalMockObject::reset();
    }
};

TEST_F(SmemEtcdStoreBackendTest, InitializeFailsWhenLoadLibraryFails)
{
    MOCKER_CPP(&EtcdApi::LoadLibrary, int32_t(*)()).stubs().will(returnValue(K_MOCK_FAILURE));

    SmemEtcdStoreBackend backend;
    EXPECT_EQ(StoreErrorCode::ERROR, backend.Initialize(K_ETCD_BACKEND_URL, "", ""));
    EXPECT_EQ(0, g_fakeEtcdEnv.newCallCount);
}

TEST_F(SmemEtcdStoreBackendTest, InitializeAcceptsTcpEndpointAndBuildsHttpEndpoint)
{
    MOCKER_CPP(&EtcdApi::LoadLibrary, int32_t(*)()).stubs().will(returnValue(int32_t(0)));

    SmemEtcdStoreBackend backend;
    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.Initialize(K_TCP_BACKEND_URL, "", ""));
    EXPECT_EQ(K_HTTP_IPV4_ENDPOINT, g_fakeEtcdEnv.lastEndpoints);
    EXPECT_EQ(1, g_fakeEtcdEnv.newCallCount);
}

TEST_F(SmemEtcdStoreBackendTest, InitializeBuildsIpv4EndpointAndSkipsSecondInit)
{
    MOCKER_CPP(&EtcdApi::LoadLibrary, int32_t(*)()).stubs().will(returnValue(int32_t(0)));

    SmemEtcdStoreBackend backend;
    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.Initialize(K_ETCD_BACKEND_URL, K_USER_NAME, K_PASSWORD));
    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.Initialize(K_ETCD_BACKEND_URL, K_USER_NAME, K_PASSWORD));

    EXPECT_EQ(K_HTTP_IPV4_ENDPOINT, g_fakeEtcdEnv.lastEndpoints);
    EXPECT_EQ(1, g_fakeEtcdEnv.newCallCount);
}

TEST_F(SmemEtcdStoreBackendTest, InitializeBuildsIpv6Endpoint)
{
    MOCKER_CPP(&EtcdApi::LoadLibrary, int32_t(*)()).stubs().will(returnValue(int32_t(0)));

    SmemEtcdStoreBackend backend;
    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.Initialize(K_IPV6_ETCD_BACKEND_URL, "", ""));
    EXPECT_EQ(K_HTTP_IPV6_ENDPOINT, g_fakeEtcdEnv.lastEndpoints);
    EXPECT_EQ(1, g_fakeEtcdEnv.newCallCount);
}

TEST_F(SmemEtcdStoreBackendTest, CrudAndLockApisRequireInitialization)
{
    SmemEtcdStoreBackend backend;
    std::vector<uint8_t> outValue;

    EXPECT_EQ(StoreErrorCode::ERROR, backend.Get("k", outValue));
    EXPECT_EQ(StoreErrorCode::ERROR, backend.Put("k", std::vector<uint8_t>{'v'}, 1));
    EXPECT_EQ(StoreErrorCode::ERROR, backend.Delete("k"));
    EXPECT_EQ(StoreErrorCode::ERROR, backend.AcquireDistributedLock("lock"));
    EXPECT_EQ(StoreErrorCode::ERROR, backend.ReleaseDistributedLock("lock"));
    EXPECT_EQ(StoreErrorCode::ERROR, backend.Exist("k"));
    EXPECT_EQ(StoreErrorCode::ERROR, backend.TryAcquireDistributedLock("lock", K_TRY_LOCK_TIMEOUT_MS));
    EXPECT_EQ("Etcd", backend.BackendName());
    EXPECT_TRUE(backend.IsDistributed());
    EXPECT_TRUE(backend.SupportsTTL());
    backend.Clear();
}

TEST_F(SmemEtcdStoreBackendTest, CrudAndLockApisMapUnderlyingReturnCodes)
{
    MOCKER_CPP(&EtcdApi::LoadLibrary, int32_t(*)()).stubs().will(returnValue(int32_t(0)));

    SmemEtcdStoreBackend backend;
    ASSERT_EQ(StoreErrorCode::SUCCESS, backend.Initialize(K_ETCD_BACKEND_URL, "", ""));
    ASSERT_NE(nullptr, g_fakeEtcdEnv.lastClient);

    auto *client = g_fakeEtcdEnv.lastClient;
    client->kv["alpha"] = "value";

    std::vector<uint8_t> outValue;
    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.Get("alpha", outValue));
    EXPECT_EQ("value", std::string(outValue.begin(), outValue.end()));
    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.Put("beta", std::vector<uint8_t>{'1', '2'}, K_PUT_TTL_SECONDS));
    EXPECT_EQ("12", client->kv["beta"]);
    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.Exist("beta"));
    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.Delete("beta"));
    EXPECT_EQ(StoreErrorCode::NOT_EXIST, backend.Exist("beta"));
    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.AcquireDistributedLock("lock"));
    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.ReleaseDistributedLock("lock"));

    client->getRet = -1;
    client->lastError = "get failed";
    EXPECT_EQ(StoreErrorCode::NOT_EXIST, backend.Get("missing", outValue));
    EXPECT_EQ(StoreErrorCode::NOT_EXIST, backend.Exist("missing"));

    client->getRet = 0;
    client->putRet = -1;
    client->lastError = "put failed";
    EXPECT_EQ(StoreErrorCode::ERROR, backend.Put("gamma", std::vector<uint8_t>{'x'}, 0));

    client->putRet = 0;
    client->deleteRet = -1;
    client->lastError = "delete failed";
    EXPECT_EQ(StoreErrorCode::ERROR, backend.Delete("alpha"));

    client->deleteRet = 0;
    client->lockRet = -1;
    client->lastError = "lock failed";
    EXPECT_EQ(StoreErrorCode::ERROR, backend.AcquireDistributedLock("lock"));

    client->lockRet = 0;
    client->unlockRet = -1;
    client->lastError = "unlock failed";
    EXPECT_EQ(StoreErrorCode::ERROR, backend.ReleaseDistributedLock("lock"));
}

TEST_F(SmemEtcdStoreBackendTest, UnInitializeAndDestructorCloseInitializedClient)
{
    MOCKER_CPP(&EtcdApi::LoadLibrary, int32_t(*)()).stubs().will(returnValue(int32_t(0)));

    {
        SmemEtcdStoreBackend backend;
        ASSERT_EQ(StoreErrorCode::SUCCESS, backend.Initialize(K_ETCD_BACKEND_URL, "", ""));
        backend.UnInitialize();
        EXPECT_EQ(1, g_fakeEtcdEnv.closeCallCount);
    }

    EXPECT_EQ(1, g_fakeEtcdEnv.closeCallCount);

    {
        SmemEtcdStoreBackend backend;
        ASSERT_EQ(StoreErrorCode::SUCCESS, backend.Initialize(K_ETCD_BACKEND_URL, "", ""));
    }

    EXPECT_EQ(K_DOUBLE_CLOSE_CALL_COUNT, g_fakeEtcdEnv.closeCallCount);
}
