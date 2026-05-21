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
#include <mockcpp/mockcpp.hpp>

#include "smem_ha_config_store.h"
#include "smem_tcp_config_store.h"
#include "smem_local_memory_backend.h"
#include "smem_etcd_store_backend.h"
#include "smem_external_backend_registry.h"
#include "smem_store_factory.h"

using namespace ock::smem;
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

namespace {

constexpr char K_EXTERNAL_BACKEND_NAME[] = "memfabric";
constexpr char K_DEFAULT_CLUSTER_ROOT[] = "/memfabric_hybrid/config_store/clusters/";
constexpr char K_CLUSTER_A_ROOT[] = "/memfabric_hybrid/config_store/clusters/clusterA";

void ResetFakeExternalBackend();

bool ShouldSkipHaStoreStartupMockOnArm()
{
#if defined(__aarch64__) || defined(__arm__)
    return true;
#else
    return false;
#endif
}

} // namespace

class SmemStoreFactoryTest : public testing::Test {
public:
    void SetUp() override
    {
        ResetFakeExternalBackend();
    };

    void TearDown() override
    {
        ock::smem::StoreFactory::DestroyStoreAll();
        GlobalMockObject::verify();
        GlobalMockObject::reset();
        ResetFakeExternalBackend();
    };
};

namespace {

struct FakeExternalHandle {};

struct FakeExternalBackendState {
    bool distributed = true;
    int32_t createRet = SMEM_STORE_BACKEND_CODE_OK;
    int createCount = 0;
    int destroyCount = 0;
    std::string lastName;
    std::string lastPrefix;
} g_fakeExternalBackendState;

const auto FAKE_GET = +[](void *, const char *, void *, uint64_t, uint32_t, uint64_t *size) -> int32_t {
    if (size != nullptr) {
        *size = 0;
    }
    return SMEM_STORE_BACKEND_CODE_NOENT;
};

bool FakeDistributed(uint32_t flags)
{
    (void)flags;
    return g_fakeExternalBackendState.distributed;
}

int32_t FakeCreate(const char *name, const char *prefix, uint32_t flags, void **handle)
{
    (void)flags;
    g_fakeExternalBackendState.lastName = name == nullptr ? "" : name;
    g_fakeExternalBackendState.lastPrefix = prefix == nullptr ? "" : prefix;
    g_fakeExternalBackendState.createCount++;
    if (g_fakeExternalBackendState.createRet != SMEM_STORE_BACKEND_CODE_OK) {
        if (handle != nullptr) {
            *handle = nullptr;
        }
        return g_fakeExternalBackendState.createRet;
    }

    if (handle != nullptr) {
        *handle = new FakeExternalHandle();
    }
    return SMEM_STORE_BACKEND_CODE_OK;
}

void FakeDestroy(void *handle)
{
    g_fakeExternalBackendState.destroyCount++;
    delete reinterpret_cast<FakeExternalHandle *>(handle);
}

int32_t FakePut(void *handle, const char *key, const void *value, uint64_t size, uint32_t flags)
{
    (void)handle;
    (void)key;
    (void)value;
    (void)size;
    (void)flags;
    return SMEM_STORE_BACKEND_CODE_OK;
}

int32_t FakeRemove(void *handle, const char *key, uint32_t flags)
{
    (void)handle;
    (void)key;
    (void)flags;
    return SMEM_STORE_BACKEND_CODE_OK;
}

int32_t FakeLock(void *handle, const char *name, uint32_t flags)
{
    (void)handle;
    (void)name;
    (void)flags;
    return SMEM_STORE_BACKEND_CODE_OK;
}

int32_t FakeTryLock(void *handle, const char *name, uint32_t flags)
{
    (void)handle;
    (void)name;
    (void)flags;
    return SMEM_STORE_BACKEND_CODE_OK;
}

int32_t FakeUnlock(void *handle, const char *name, uint32_t flags)
{
    (void)handle;
    (void)name;
    (void)flags;
    return SMEM_STORE_BACKEND_CODE_OK;
}

smem_conf_store_backend_op_t MakeFakeExternalBackendOp()
{
    smem_conf_store_backend_op_t backendOp{};
    backendOp.distributed = FakeDistributed;
    backendOp.create = FakeCreate;
    backendOp.destroy = FakeDestroy;
    backendOp.put = FakePut;
    backendOp.get = FAKE_GET;
    backendOp.remove = FakeRemove;
    backendOp.lock = FakeLock;
    backendOp.try_lock = FakeTryLock;
    backendOp.unlock = FakeUnlock;
    return backendOp;
}

void ResetFakeExternalBackend()
{
    g_fakeExternalBackendState = {};
    ock::smem::SmemExternalBackendRegistry::ResetExternalBackendOp();
}

} // namespace

TEST_F(SmemStoreFactoryTest, create_store_success)
{
    MOCKER_CPP(&ock::smem::SmemLocalMemoryBackend::Initialize,
        int32_t (*)(ock::smem::SmemLocalMemoryBackend *, const std::string &, const std::string &,
        const std::string &)).stubs().will(returnValue(0));
    MOCKER_CPP(&ock::smem::TcpConfigStore::Startup,
        int32_t (*)(ock::smem::TcpConfigStore *, const smem_tls_config &, int)).stubs().will(returnValue(0));
    std::string ip = "127.0.0.1";
    uint16_t port = 16888;
    auto tcpStore = ock::smem::StoreFactory::CreateStore(ip, port, ConfigStoreModel::CSM_BOTH, 1, 0);
    ASSERT_NE(true, (tcpStore == nullptr));
    auto tcpStore2 = ock::smem::StoreFactory::CreateStore(ip, port, ConfigStoreModel::CSM_BOTH, 1, 0);
    ASSERT_NE(true, (tcpStore2 == nullptr));
    ock::smem::StoreFactory::DestroyStore(ip, port);
}

TEST_F(SmemStoreFactoryTest, create_store_failed)
{
    int32_t targetErr = -2009;
    MOCKER_CPP(&ock::smem::SmemLocalMemoryBackend::Initialize,
        int32_t (*)(ock::smem::SmemLocalMemoryBackend *, const std::string &, const std::string &,
        const std::string &)).stubs().will(returnValue(0));
    MOCKER_CPP(&ock::smem::TcpConfigStore::Startup,
        int32_t (*)(ock::smem::TcpConfigStore *, const smem_tls_config &, int))
        .stubs().will(returnValue(targetErr)).then(returnValue(-1));
    std::string ip = "127.0.0.1";
    uint16_t port = 16888;
    auto tcpStore = ock::smem::StoreFactory::CreateStore(ip, port, ConfigStoreModel::CSM_BOTH, 1, 0);
    ASSERT_EQ(true, (tcpStore == nullptr));
    auto tcpStore2 = ock::smem::StoreFactory::CreateStore(ip, port, ConfigStoreModel::CSM_BOTH, 1, 0);
    ASSERT_EQ(true, (tcpStore2 == nullptr));
}

TEST_F(SmemStoreFactoryTest, create_store_by_url_tcp_failed)
{
    int32_t targetErr = -2009;
    MOCKER_CPP(&ock::smem::SmemLocalMemoryBackend::Initialize,
        int32_t (*)(ock::smem::SmemLocalMemoryBackend *, const std::string &, const std::string &,
        const std::string &)).stubs().will(returnValue(0));
    MOCKER_CPP(&ock::smem::TcpConfigStore::Startup,
        int32_t (*)(ock::smem::TcpConfigStore *, const smem_tls_config &, int))
        .stubs().will(returnValue(targetErr)).then(returnValue(-1));
    std::string url = "tcp://127.0.0.1:16888";
    auto tcpStore = ock::smem::StoreFactory::CreateStoreByUrl(url, ock::smem::ConfigStoreModel::CSM_BOTH, 1, 0);
    ASSERT_EQ(true, (tcpStore == nullptr));
    auto tcpStore2 = ock::smem::StoreFactory::CreateStoreByUrl(url, ock::smem::ConfigStoreModel::CSM_BOTH, 1, 0);
    ASSERT_EQ(true, (tcpStore2 == nullptr));
}

TEST_F(SmemStoreFactoryTest, destroy_all_store)
{
    MOCKER_CPP(&ock::smem::SmemLocalMemoryBackend::Initialize,
        int32_t (*)(ock::smem::SmemLocalMemoryBackend *, const std::string &, const std::string &,
        const std::string &)).stubs().will(returnValue(0));
    MOCKER_CPP(&ock::smem::TcpConfigStore::Startup,
        int32_t (*)(ock::smem::TcpConfigStore *, const smem_tls_config &, int)).stubs().will(returnValue(0));
    std::string ip = "127.0.0.1";
    uint16_t port = 17888;
    auto tcpStore = ock::smem::StoreFactory::CreateStore(ip, port, ConfigStoreModel::CSM_BOTH, 1, 0);
    EXPECT_NE(true, (tcpStore == nullptr));
    ock::smem::StoreFactory::DestroyStoreAll();
}

TEST_F(SmemStoreFactoryTest, create_store_by_url_reg_success)
{
    if (ShouldSkipHaStoreStartupMockOnArm()) {
        GTEST_SKIP() << "mockcpp member-function patching is unstable on ARM";
    }
    const auto backendOp = MakeFakeExternalBackendOp();
    ock::smem::SmemExternalBackendRegistry::SetExternalBackendOp(backendOp);
    MOCKER_CPP(&ock::smem::HaConfigStore::Startup, int32_t(*)(ock::smem::HaConfigStore *, const smem_tls_config &))
        .stubs()
        .will(returnValue(0));

    std::string url = "reg://127.0.0.1:2379";
    auto regStore = ock::smem::StoreFactory::CreateStoreByUrl(url, ock::smem::ConfigStoreModel::CSM_CLIENT, 1, 0);
    ASSERT_NE(nullptr, regStore.Get());
    EXPECT_EQ(K_EXTERNAL_BACKEND_NAME, g_fakeExternalBackendState.lastName);
    EXPECT_EQ(K_DEFAULT_CLUSTER_ROOT, g_fakeExternalBackendState.lastPrefix);
    EXPECT_EQ(1, g_fakeExternalBackendState.createCount);
}

TEST_F(SmemStoreFactoryTest, create_store_by_url_reg_cluster_success)
{
    if (ShouldSkipHaStoreStartupMockOnArm()) {
        GTEST_SKIP() << "mockcpp member-function patching is unstable on ARM";
    }
    const auto backendOp = MakeFakeExternalBackendOp();
    ock::smem::SmemExternalBackendRegistry::SetExternalBackendOp(backendOp);
    MOCKER_CPP(&ock::smem::HaConfigStore::Startup, int32_t(*)(ock::smem::HaConfigStore *, const smem_tls_config &))
        .stubs()
        .will(returnValue(0));

    std::string url = "reg://127.0.0.1:2379#clusterA";
    auto regStore = ock::smem::StoreFactory::CreateStoreByUrl(url, ock::smem::ConfigStoreModel::CSM_CLIENT, 1, 0);
    ASSERT_NE(nullptr, regStore.Get());
    EXPECT_EQ(K_EXTERNAL_BACKEND_NAME, g_fakeExternalBackendState.lastName);
    EXPECT_EQ(K_CLUSTER_A_ROOT, g_fakeExternalBackendState.lastPrefix);
}

TEST_F(SmemStoreFactoryTest, create_store_by_url_reg_failed_when_backend_not_registered)
{
    std::string url = "reg://127.0.0.1:2379";
    auto regStore = ock::smem::StoreFactory::CreateStoreByUrl(url, ock::smem::ConfigStoreModel::CSM_CLIENT, 1, 0);
    EXPECT_EQ(nullptr, regStore.Get());
    EXPECT_EQ(ock::smem::SM_ERROR, ock::smem::StoreFactory::GetFailedReason());
}

TEST_F(SmemStoreFactoryTest, create_store_by_url_reg_failed_when_backend_not_distributed)
{
    auto backendOp = MakeFakeExternalBackendOp();
    ock::smem::SmemExternalBackendRegistry::SetExternalBackendOp(backendOp);
    g_fakeExternalBackendState.distributed = false;

    std::string url = "reg://127.0.0.1:2379";
    auto regStore = ock::smem::StoreFactory::CreateStoreByUrl(url, ock::smem::ConfigStoreModel::CSM_CLIENT, 1, 0);
    EXPECT_EQ(nullptr, regStore.Get());
    EXPECT_EQ(ock::smem::SM_ERROR, ock::smem::StoreFactory::GetFailedReason());
    EXPECT_EQ(1, g_fakeExternalBackendState.destroyCount);
}

TEST_F(SmemStoreFactoryTest, create_store_by_url_reg_failed_when_backend_create_fails)
{
    auto backendOp = MakeFakeExternalBackendOp();
    ock::smem::SmemExternalBackendRegistry::SetExternalBackendOp(backendOp);
    g_fakeExternalBackendState.createRet = SMEM_STORE_BACKEND_CODE_INTERNAL;

    std::string url = "reg://127.0.0.1:2379";
    auto regStore = ock::smem::StoreFactory::CreateStoreByUrl(url, ock::smem::ConfigStoreModel::CSM_CLIENT, 1, 0);
    EXPECT_EQ(nullptr, regStore.Get());
    EXPECT_EQ(ock::smem::SM_ERROR, ock::smem::StoreFactory::GetFailedReason());
}
