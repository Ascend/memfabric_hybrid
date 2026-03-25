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

#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "smem.h"
#include "smem_external_backend.h"
#include "smem_external_backend_registry.h"
#include "fake_external_backend_helper.h"

using namespace ock::smem;

namespace {

constexpr char K_REG_BACKEND_URL[] = "reg://127.0.0.1:2379";
constexpr char K_REG_BACKEND_URL_IPV6[] = "reg://[::1]:2379";
constexpr char K_EXTERNAL_BACKEND_NAME[] = "memfabric";
constexpr char K_CLUSTER_ID[] = "cluster-a";
constexpr char K_DEFAULT_CLUSTER_ROOT[] = "/memfabric_hybrid/config_store/clusters/";
constexpr char K_CLUSTER_ROOT[] = "/memfabric_hybrid/config_store/clusters/cluster-a";
constexpr char K_DEFAULT_CLUSTERED_BETA_KEY[] = "/memfabric_hybrid/config_store/clusters/beta";
constexpr char K_CLUSTERED_ALPHA_KEY[] = "/memfabric_hybrid/config_store/clusters/cluster-a/alpha";
constexpr char K_CLUSTERED_BETA_KEY[] = "/memfabric_hybrid/config_store/clusters/cluster-a/beta";
constexpr char K_CLUSTERED_LEADER_KEY[] =
    "/memfabric_hybrid/config_store/clusters/cluster-a/memfabric_hybrid/config_store/meta/leader";
constexpr char K_CLUSTERED_BACKEND_LOCK[] = "/memfabric_hybrid/config_store/clusters/cluster-a/backend";
constexpr char K_CLUSTERED_CUSTOM_LOCK[] = "/memfabric_hybrid/config_store/clusters/cluster-a/custom-lock";
constexpr char K_BACKEND_LOCK_NAME[] = "backend";
constexpr char K_CUSTOM_LOCK_NAME[] = "custom-lock";
constexpr char K_VALUE[] = "value";
constexpr size_t K_GET_RETRY_LARGE_VALUE_SIZE = 2U * 1024U * 1024U;
constexpr int EXPECTED_GET_COUNT_AFTER_ALPHA = 1;
constexpr int EXPECTED_GET_COUNT_AFTER_LEADER = 2;
constexpr int EXPECTED_GET_COUNT_AFTER_BUFEX_RETRY = 2;
constexpr int64_t K_PUT_TTL_SECONDS = 7;
constexpr int64_t K_TRY_LOCK_TIMEOUT_MS = 10;

namespace fake_backend = ock::smem::test;
int g_fakeExternalCreateAltCount = 0;

fake_backend::FakeExternalBackendEnv &GetFakeExternalEnv() noexcept
{
    return fake_backend::GetFakeEnv();
}

void ResetFakeExternalEnv()
{
    fake_backend::ResetFakeExternalEnv();
    g_fakeExternalCreateAltCount = 0;
}

int32_t FakeCreateAlt(const char *name, const char *prefix, uint32_t flags, void **handle)
{
    (void)flags;
    auto &env = GetFakeExternalEnv();
    env.lastName = fake_backend::NormalizeFakeText(name);
    env.lastPrefix = fake_backend::NormalizeFakeText(prefix);
    g_fakeExternalCreateAltCount++;
    return env.createRet == SMEM_STORE_BACKEND_CODE_OK ? fake_backend::CreateFakeHandle(handle, env)
                                                       : fake_backend::ReturnCreateFailure(handle, env.createRet);
}

smem_conf_store_backend_op_t MakeBackendOp(bool useAltCreate = false)
{
    auto backendOp = fake_backend::MakeFakeBackendOp();
    backendOp.create = useAltCreate ? FakeCreateAlt : fake_backend::FakeCreate;
    return backendOp;
}

} // namespace

class SmemExternalBackendTest : public testing::Test {
public:
    void SetUp() override
    {
        ResetFakeExternalEnv();
    }

    void TearDown() override
    {
        ResetFakeExternalEnv();
    }
};

TEST_F(SmemExternalBackendTest, InitializeFailsWhenBackendNotRegistered)
{
    SmemExternalBackend backend;
    EXPECT_EQ(StoreErrorCode::ERROR, backend.Initialize(K_REG_BACKEND_URL, "", ""));
}

TEST_F(SmemExternalBackendTest, RegistrationOverwriteUsesLatestBackendOp)
{
    auto backendOp = MakeBackendOp(false);
    ASSERT_EQ(SM_OK, smem_config_store_set_backend_op(&backendOp));

    auto backendOpAlt = MakeBackendOp(true);
    ASSERT_EQ(SM_OK, smem_config_store_set_backend_op(&backendOpAlt));

    SmemExternalBackend backend;
    ASSERT_EQ(StoreErrorCode::SUCCESS, backend.Initialize(K_REG_BACKEND_URL_IPV6, "", ""));
    EXPECT_EQ(0, GetFakeExternalEnv().createCount);
    EXPECT_EQ(1, g_fakeExternalCreateAltCount);
    EXPECT_EQ(K_EXTERNAL_BACKEND_NAME, GetFakeExternalEnv().lastName);
    EXPECT_EQ(K_DEFAULT_CLUSTER_ROOT, GetFakeExternalEnv().lastPrefix);
}

TEST_F(SmemExternalBackendTest, CrudAndLockApisRequireInitialization)
{
    SmemExternalBackend backend;
    std::vector<uint8_t> outValue;

    EXPECT_EQ(StoreErrorCode::ERROR, backend.Get("k", outValue));
    EXPECT_EQ(StoreErrorCode::ERROR, backend.Put("k", std::vector<uint8_t>{'v'}, 0));
    EXPECT_EQ(StoreErrorCode::ERROR, backend.Delete("k"));
    EXPECT_EQ(StoreErrorCode::ERROR, backend.Exist("k"));
    EXPECT_EQ(StoreErrorCode::ERROR, backend.AcquireDistributedLock(K_BACKEND_LOCK_NAME));
    EXPECT_EQ(StoreErrorCode::ERROR, backend.TryAcquireDistributedLock(K_BACKEND_LOCK_NAME, 0));
    EXPECT_EQ(StoreErrorCode::ERROR, backend.ReleaseDistributedLock(K_BACKEND_LOCK_NAME));
    EXPECT_EQ("External", backend.BackendName());
    EXPECT_FALSE(backend.SupportsTTL());
    EXPECT_FALSE(backend.IsDistributed());
    backend.Clear();
}

TEST_F(SmemExternalBackendTest, ClusterBackendUsesPrefixForKeysAndLocks)
{
    auto backendOp = MakeBackendOp();
    ASSERT_EQ(SM_OK, smem_config_store_set_backend_op(&backendOp));

    SmemExternalBackend backend(K_CLUSTER_ID);
    ASSERT_EQ(StoreErrorCode::SUCCESS, backend.Initialize(K_REG_BACKEND_URL, "", ""));
    ASSERT_NE(nullptr, GetFakeExternalEnv().lastHandle);
    EXPECT_EQ(K_EXTERNAL_BACKEND_NAME, GetFakeExternalEnv().lastName);
    EXPECT_EQ(K_CLUSTER_ROOT, GetFakeExternalEnv().lastPrefix);

    const std::string largeValue(300, 'a');
    GetFakeExternalEnv().lastHandle->kv[K_CLUSTERED_ALPHA_KEY] =
        std::vector<uint8_t>(largeValue.begin(), largeValue.end());
    GetFakeExternalEnv().lastHandle->kv[K_CLUSTERED_LEADER_KEY] =
        std::vector<uint8_t>(K_VALUE, K_VALUE + std::strlen(K_VALUE));

    std::vector<uint8_t> outValue;
    GetFakeExternalEnv().getCount = 0;
    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.Get("alpha", outValue));
    EXPECT_EQ(EXPECTED_GET_COUNT_AFTER_ALPHA, GetFakeExternalEnv().getCount);
    EXPECT_EQ(largeValue, std::string(outValue.begin(), outValue.end()));
    EXPECT_EQ("alpha", GetFakeExternalEnv().lastGetKey);
    EXPECT_EQ(K_CLUSTERED_ALPHA_KEY, GetFakeExternalEnv().lastResolvedGetKey);

    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.Get(KEY_LEADER, outValue));
    EXPECT_EQ(EXPECTED_GET_COUNT_AFTER_LEADER, GetFakeExternalEnv().getCount);
    EXPECT_EQ(K_VALUE, std::string(outValue.begin(), outValue.end()));
    EXPECT_EQ(KEY_LEADER, GetFakeExternalEnv().lastGetKey);
    EXPECT_EQ(K_CLUSTERED_LEADER_KEY, GetFakeExternalEnv().lastResolvedGetKey);

    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.Put("beta", std::vector<uint8_t>{'1', '2'}, K_PUT_TTL_SECONDS));
    EXPECT_EQ("beta", GetFakeExternalEnv().lastPutKey);
    EXPECT_EQ(K_CLUSTERED_BETA_KEY, GetFakeExternalEnv().lastResolvedPutKey);
    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.Exist("beta"));

    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.AcquireDistributedLock(K_BACKEND_LOCK_NAME));
    EXPECT_EQ(K_BACKEND_LOCK_NAME, GetFakeExternalEnv().lastLockName);
    EXPECT_EQ(K_CLUSTERED_BACKEND_LOCK, GetFakeExternalEnv().lastResolvedLockName);

    GetFakeExternalEnv().lastHandle->locks.insert(K_CLUSTERED_CUSTOM_LOCK);
    EXPECT_EQ(StoreErrorCode::TIMEOUT, backend.TryAcquireDistributedLock(K_CUSTOM_LOCK_NAME, 0));
    EXPECT_EQ(K_CUSTOM_LOCK_NAME, GetFakeExternalEnv().lastTryLockName);
    EXPECT_EQ(K_CLUSTERED_CUSTOM_LOCK, GetFakeExternalEnv().lastResolvedTryLockName);

    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.ReleaseDistributedLock(K_BACKEND_LOCK_NAME));
    EXPECT_EQ(K_BACKEND_LOCK_NAME, GetFakeExternalEnv().lastUnlockName);
    EXPECT_EQ(K_CLUSTERED_BACKEND_LOCK, GetFakeExternalEnv().lastResolvedUnlockName);

    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.Delete("beta"));
    EXPECT_EQ("beta", GetFakeExternalEnv().lastRemoveKey);
    EXPECT_EQ(K_CLUSTERED_BETA_KEY, GetFakeExternalEnv().lastResolvedRemoveKey);
    EXPECT_EQ(StoreErrorCode::NOT_EXIST, backend.Exist("beta"));
}

TEST_F(SmemExternalBackendTest, GetRetriesWithLargerBufferAfterBufex)
{
    auto backendOp = MakeBackendOp();
    ASSERT_EQ(SM_OK, smem_config_store_set_backend_op(&backendOp));

    SmemExternalBackend backend;
    ASSERT_EQ(StoreErrorCode::SUCCESS, backend.Initialize(K_REG_BACKEND_URL, "", ""));

    ASSERT_NE(nullptr, GetFakeExternalEnv().lastHandle);
    const std::string largeValue(K_GET_RETRY_LARGE_VALUE_SIZE, 'x');
    GetFakeExternalEnv().lastHandle->kv["/memfabric_hybrid/config_store/clusters/alpha"] =
        std::vector<uint8_t>(largeValue.begin(), largeValue.end());
    GetFakeExternalEnv().getCount = 0;
    std::vector<uint8_t> outValue;
    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.Get("alpha", outValue));
    EXPECT_EQ(EXPECTED_GET_COUNT_AFTER_BUFEX_RETRY, GetFakeExternalEnv().getCount);
    EXPECT_EQ(largeValue, std::string(outValue.begin(), outValue.end()));
    EXPECT_EQ("alpha", GetFakeExternalEnv().lastGetKey);
    EXPECT_EQ("/memfabric_hybrid/config_store/clusters/alpha", GetFakeExternalEnv().lastResolvedGetKey);
}

TEST_F(SmemExternalBackendTest, GetAllowsEmptyValueWhenBackendReturnsOk)
{
    auto backendOp = MakeBackendOp();
    ASSERT_EQ(SM_OK, smem_config_store_set_backend_op(&backendOp));

    SmemExternalBackend backend;
    ASSERT_EQ(StoreErrorCode::SUCCESS, backend.Initialize(K_REG_BACKEND_URL, "", ""));

    ASSERT_NE(nullptr, GetFakeExternalEnv().lastHandle);
    GetFakeExternalEnv().lastHandle->kv[K_DEFAULT_CLUSTERED_BETA_KEY] = {};

    std::vector<uint8_t> outValue{1, 2, 3};
    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.Get("beta", outValue));
    EXPECT_TRUE(outValue.empty());
    EXPECT_EQ("beta", GetFakeExternalEnv().lastGetKey);
    EXPECT_EQ(K_DEFAULT_CLUSTERED_BETA_KEY, GetFakeExternalEnv().lastResolvedGetKey);
}

TEST_F(SmemExternalBackendTest, BackendMapsCommonErrors)
{
    auto backendOp = MakeBackendOp();
    ASSERT_EQ(SM_OK, smem_config_store_set_backend_op(&backendOp));

    SmemExternalBackend backend;
    ASSERT_EQ(StoreErrorCode::SUCCESS, backend.Initialize(K_REG_BACKEND_URL, "", ""));

    GetFakeExternalEnv().forcedPutRet = SMEM_STORE_BACKEND_CODE_INVAL;
    EXPECT_EQ(StoreErrorCode::INVALID_MESSAGE, backend.Put("alpha", std::vector<uint8_t>{'x'}, 0));

    GetFakeExternalEnv().forcedPutRet = SMEM_STORE_BACKEND_CODE_OK;
    GetFakeExternalEnv().forcedRemoveRet = SMEM_STORE_BACKEND_CODE_NORES;
    EXPECT_EQ(StoreErrorCode::IO_ERROR, backend.Delete("alpha"));

    GetFakeExternalEnv().forcedRemoveRet = SMEM_STORE_BACKEND_CODE_OK;
    GetFakeExternalEnv().forcedLockRet = SMEM_STORE_BACKEND_CODE_INVAL;
    EXPECT_EQ(StoreErrorCode::INVALID_MESSAGE, backend.AcquireDistributedLock(K_BACKEND_LOCK_NAME));

    GetFakeExternalEnv().forcedLockRet = SMEM_STORE_BACKEND_CODE_OK;
    GetFakeExternalEnv().forcedTryLockRet = SMEM_STORE_BACKEND_CODE_LOCKED;
    EXPECT_EQ(StoreErrorCode::TIMEOUT, backend.TryAcquireDistributedLock(K_BACKEND_LOCK_NAME, K_TRY_LOCK_TIMEOUT_MS));

    GetFakeExternalEnv().forcedTryLockRet = SMEM_STORE_BACKEND_CODE_OK;
    GetFakeExternalEnv().forcedUnlockRet = SMEM_STORE_BACKEND_CODE_UNLOCKED;
    EXPECT_EQ(StoreErrorCode::NOT_EXIST, backend.ReleaseDistributedLock(K_BACKEND_LOCK_NAME));

    GetFakeExternalEnv().forcedUnlockRet = SMEM_STORE_BACKEND_CODE_OK;
    GetFakeExternalEnv().forcedGetRet = SMEM_STORE_BACKEND_CODE_INVAL;
    std::vector<uint8_t> outValue;
    EXPECT_EQ(StoreErrorCode::INVALID_MESSAGE, backend.Get("alpha", outValue));

    GetFakeExternalEnv().forcedGetRet = SMEM_STORE_BACKEND_CODE_NORES;
    EXPECT_EQ(StoreErrorCode::IO_ERROR, backend.Exist("alpha"));
}

TEST_F(SmemExternalBackendTest, ExistTreatsBufexAsSuccessWithoutRetry)
{
    auto backendOp = MakeBackendOp();
    ASSERT_EQ(SM_OK, smem_config_store_set_backend_op(&backendOp));

    SmemExternalBackend backend;
    ASSERT_EQ(StoreErrorCode::SUCCESS, backend.Initialize(K_REG_BACKEND_URL, "", ""));

    ASSERT_NE(nullptr, GetFakeExternalEnv().lastHandle);
    GetFakeExternalEnv().lastHandle->kv[K_DEFAULT_CLUSTERED_BETA_KEY] = std::vector<uint8_t>{'1', '2'};
    GetFakeExternalEnv().getCount = 0;

    EXPECT_EQ(StoreErrorCode::SUCCESS, backend.Exist("beta"));
    EXPECT_EQ(1, GetFakeExternalEnv().getCount);
    EXPECT_EQ("beta", GetFakeExternalEnv().lastGetKey);
    EXPECT_EQ(K_DEFAULT_CLUSTERED_BETA_KEY, GetFakeExternalEnv().lastResolvedGetKey);
}

TEST_F(SmemExternalBackendTest, UnInitializeAndDestructorDestroyHandleOnce)
{
    auto backendOp = MakeBackendOp();
    ASSERT_EQ(SM_OK, smem_config_store_set_backend_op(&backendOp));

    {
        SmemExternalBackend backend;
        ASSERT_EQ(StoreErrorCode::SUCCESS, backend.Initialize(K_REG_BACKEND_URL, "", ""));
        EXPECT_TRUE(backend.IsDistributed());
        backend.UnInitialize();
        EXPECT_EQ(1, GetFakeExternalEnv().destroyCount);
    }

    EXPECT_EQ(1, GetFakeExternalEnv().destroyCount);
}
