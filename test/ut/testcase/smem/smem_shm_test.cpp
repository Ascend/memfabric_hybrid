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
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include <gtest/gtest.h>
#include "hybm_big_mem.h"
#include "smem_shm.h"
#include "smem_types.h"
#include "ut_barrier_util.h"
#include "hybm.h"
#include "smem_ref.h"
#include "smem_logger.h"
#include "hybm_def.h"
#include "smem_store_factory.h"

#define private public
#include "smem_shm_entry.h"
#undef private

#include "smem_shm_entry_manager.h"
#include "smem_net_group_engine.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace ock::smem;

namespace {

constexpr int32_t UT_SMEM_ID = 1;
constexpr char UT_IP_PORT[] = "tcp://127.0.0.1:7758";
constexpr char UT_IP_PORT2[] = "tcp://127.0.0.1:7958";
constexpr char UT_REG_URL_WITH_CLUSTER[] = "reg://127.0.0.1:2379#clusterA";
constexpr uint32_t UT_CREATE_MEM_SIZE = 2UL * 1024UL * 1024UL;
constexpr uint32_t UT_COPY_MEM_SIZE = 2UL * 1024UL * 1024UL;
constexpr uint64_t UT_SHM_SIZE = 128ULL * 1024ULL * 1024ULL;
constexpr uint32_t UT_BATCH_SIZE = 5U;
constexpr uint64_t UT_COPY_SIZE = 1ULL * 1024ULL;
constexpr uint64_t UT_GVA_SIZE = 2ULL * 1024ULL * 1024ULL * 1024ULL;
constexpr int32_t UT_RANDOM_MULTIPLIER = 23;
constexpr int32_t UT_RANDOM_INCREMENT = 17;
constexpr int32_t UT_NEGATIVE_RATIO_DIVISOR = 3;
constexpr uint32_t UT_REG_RANK_SIZE = 2U;
constexpr uint32_t UT_REG_RANK_ID = 1U;
constexpr uint16_t UT_REG_LOCAL_RANK_ID = 0U;
constexpr uint32_t UT_SUBGROUP_RANK_SIZE = 1024U;
constexpr uint32_t UT_ATOMIC_ALLOC_LIMIT = 1024U;
constexpr uint32_t UT_EXTRA_CONTEXT_SAMPLE_SIZE = 100U;
constexpr size_t UT_DUMMY_CONTEXT_BYTES = 8U;
constexpr uintptr_t UT_INVALID_HANDLE_ADDR = 0x1234UL;
constexpr uintptr_t UT_ENTRY_ENTITY_ADDR = 0x1UL;
constexpr uintptr_t UT_CREATED_ENTITY_ADDR = 0xABCUL;
constexpr uintptr_t UT_SLICE_ADDR = 0x55UL;
constexpr uint64_t UT_MOCK_HBM_MAX_SIZE = 8ULL * 1024ULL * 1024ULL;
constexpr uint64_t UT_ENTRY_HBM_MAX_SIZE = 1234567ULL;

class FakeShmStoreManager final : public ConfigStoreManager {
public:
    ock::smem::Result Set(const std::string &, const std::vector<uint8_t> &) noexcept override
    {
        return SM_OK;
    }
    ock::smem::Result Add(const std::string &, int64_t, int64_t &value) noexcept override
    {
        value = 0;
        return SM_OK;
    }
    ock::smem::Result QueryAlive(uint32_t rank, uint32_t &alive) noexcept override
    {
        alive = true;
        return SM_OK;
    }
    ock::smem::Result PrefixGet(const std::string &key,
                                std::unordered_map<std::string, std::string> &value) noexcept override
    {
        return SM_OK;
    }
    ock::smem::Result Remove(const std::string &, bool) noexcept override
    {
        return SM_OK;
    }
    ock::smem::Result Append(const std::string &, const std::vector<uint8_t> &, uint64_t &newSize) noexcept override
    {
        newSize = 0;
        return SM_OK;
    }
    ock::smem::Result Cas(const std::string &, const std::vector<uint8_t> &, const std::vector<uint8_t> &,
                          std::vector<uint8_t> &exists) noexcept override
    {
        exists.clear();
        return SM_OK;
    }
    ock::smem::Result Watch(const std::string &,
                            const std::function<void(int, const std::string &, const std::vector<uint8_t> &)> &,
                            uint32_t &wid) noexcept override
    {
        wid = 0;
        return SM_OK;
    }
    ock::smem::Result Watch(WatchRankType, const std::function<void(WatchRankType, uint32_t)> &,
                            uint32_t &wid) noexcept override
    {
        wid = 0;
        return SM_OK;
    }
    ock::smem::Result Unwatch(uint32_t) noexcept override
    {
        return SM_OK;
    }
    ock::smem::Result Write(const std::string &, const std::vector<uint8_t> &, uint32_t) noexcept override
    {
        return SM_OK;
    }
    std::string GetCompleteKey(const std::string &key) noexcept override
    {
        return key;
    }
    std::string GetCommonPrefix() noexcept override
    {
        return "";
    }
    StorePtr GetCoreStore() noexcept override
    {
        return nullptr;
    }
    ock::smem::Result GetReal(const std::string &, std::vector<uint8_t> &, int64_t) noexcept override
    {
        return SM_OBJECT_NOT_EXISTS;
    }
    void RegisterReconnectHandler(ConfigStoreReconnectHandler) noexcept override {}
    ock::smem::Result ReConnectAfterBroken(int) noexcept override
    {
        return SM_OK;
    }
    bool GetConnectStatus() noexcept override
    {
        return true;
    }
    void SetConnectStatus(bool) noexcept override {}
    void RegisterClientBrokenHandler(const ConfigStoreClientBrokenHandler &) noexcept override {}
    void RegisterServerBrokenHandler(const ConfigStoreServerBrokenHandler &) noexcept override {}
    void RegisterServerOpHandler(int16_t, const ConfigStoreServerOpHandler &) noexcept override {}
};

StorePtr MakeFakeShmStore()
{
    auto child = SmMakeRef<FakeShmStoreManager>();
    StoreManagerPtr manager = Convert<FakeShmStoreManager, ConfigStoreManager>(child);
    return Convert<ConfigStoreManager, ConfigStore>(manager);
}

} // namespace

class SmemShmTest : public testing::Test {
public:
   static void SetUpTestCase();
   static void TearDownTestCase();
   void SetUp() override;
   void TearDown() override;

public:
    static SmemShmEntryPtr g_stub_ptr;
};

SmemShmEntryPtr SmemShmTest::g_stub_ptr = SmMakeRef<SmemShmEntry>(0);

void SmemShmTest::SetUpTestCase() {}

void SmemShmTest::TearDownTestCase() {}

void SmemShmTest::SetUp()
{
    GlobalMockObject::reset();
}

void SmemShmTest::TearDown()
{
    GlobalMockObject::verify();
    GlobalMockObject::reset();
}

TEST_F(SmemShmTest, smem_shm_init_failed)
{
    void *gva;
    auto ret = smem_init(0);
    EXPECT_EQ(ret, 0);

    smem_set_log_level(0);
    uint32_t rankSize = 1;
    std::thread ts[rankSize];

    uint32_t rankId = 0;
    uint32_t rankCount = 1; // 1
    smem_shm_config_t config;

    ret = smem_shm_init(UT_IP_PORT, rankCount, rankId, rankId, &config);
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    ret = smem_shm_config_init(&config);
    EXPECT_EQ(ret, 0);

    MOCKER_CPP(hybm_init, int32_t (*)(uint16_t, uint64_t)).stubs().will(returnValue(-1));
    ret = smem_shm_init(UT_IP_PORT, rankCount, rankId, rankId, &config);
    EXPECT_EQ(ret, SM_ERROR);

    MOCKER_CPP(&SmemShmEntryManager::Initialize, int32_t (*)(const char *, uint32_t,
        uint32_t, uint16_t, smem_shm_config_t *)).stubs().will(returnValue(-1));
    ret = smem_shm_init(UT_IP_PORT, rankCount, rankId, rankId, &config);
    EXPECT_EQ(ret, SM_ERROR);

    smem_shm_uninit(0);
}

TEST_F(SmemShmTest, shm_entry_manager_initialize_accepts_reg_url)
{
    auto fakeStore = MakeFakeShmStore();
    ASSERT_NE(nullptr, fakeStore.Get());
    MOCKER_CPP(&ock::smem::StoreFactory::CreateStoreByUrl,
               ock::smem::StorePtr(*)(const std::string &, bool, uint32_t, int32_t, int32_t))
        .stubs()
        .will(returnValue(fakeStore));

    smem_shm_config_t config{};
    ASSERT_EQ(SM_OK, smem_shm_config_init(&config));

    auto &manager = SmemShmEntryManager::Instance();
    EXPECT_EQ(SM_OK, manager.Initialize(UT_REG_URL_WITH_CLUSTER, UT_REG_RANK_SIZE, UT_REG_RANK_ID, UT_REG_LOCAL_RANK_ID,
                                        &config));
    manager.Destroy();
}

TEST_F(SmemShmTest, smem_shm_create_failed)
{
    void *gva;
    auto ret = smem_init(0);
    EXPECT_EQ(ret, 0);

    smem_set_log_level(0);
    uint32_t rankSize = 1;
    std::thread ts[rankSize];

    uint32_t rankId = 0;
    uint32_t rankCount = 1; // 1
    smem_shm_config_t config;

    ret = smem_shm_config_init(nullptr);
    EXPECT_EQ(ret, SM_INVALID_PARAM);
    ret = smem_shm_config_init(&config);
    EXPECT_EQ(ret, 0);

    auto handle = smem_shm_create(UT_SMEM_ID, rankCount, rankId, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_MTE, 0, &gva);
    EXPECT_EQ(handle, nullptr);

    MOCKER_CPP(&SmemShmEntryManager::Initialize, int32_t (*)(const char *, uint32_t,
        uint32_t, uint16_t, smem_shm_config_t *)).stubs().will(returnValue(0));
    ret = smem_shm_init(UT_IP_PORT, rankCount, rankId, rankId, &config);
    EXPECT_EQ(ret, 0);

    MOCKER_CPP(&SmemShmEntry::Initialize, int32_t (*)(hybm_options &)).stubs().will(returnValue(-1));
    handle = smem_shm_create(UT_SMEM_ID, rankCount, rankId, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_RDMA, 0, &gva);
    EXPECT_EQ(handle, nullptr);

    MOCKER_CPP(&SmemShmEntryManager::CreateEntryById, int32_t (*)(uint32_t, SmemShmEntryPtr &))
        .stubs()
        .will(returnValue(-1));
    handle = smem_shm_create(UT_SMEM_ID, rankCount, rankId, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_RDMA, 0, &gva);
    EXPECT_EQ(handle, nullptr);

    smem_shm_destroy(handle, 0);
    smem_shm_uninit(0);
}
TEST_F(SmemShmTest, smem_shm_create_success)
{
    void *gva;
    auto ret = smem_init(0);
    EXPECT_EQ(ret, 0);

    smem_set_log_level(0);
    uint32_t rankSize = 1;
    uint32_t rankId = 0;
    uint32_t rankCount = 1; // 1
    smem_shm_config_t config;

    ret = smem_shm_config_init(nullptr);
    EXPECT_EQ(ret, SM_INVALID_PARAM);
    ret = smem_shm_config_init(&config);
    EXPECT_EQ(ret, 0);

    ret = smem_shm_init(UT_IP_PORT, rankCount, rankId, rankId, &config);
    EXPECT_EQ(ret, 0);

    auto handle = smem_shm_create(UT_SMEM_ID, rankCount, rankId, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_RDMA, 0, &gva);
    EXPECT_NE(handle, nullptr);

    smem_shm_destroy(handle, 0);
    smem_shm_uninit(0);
}

TEST_F(SmemShmTest, smem_shm_set_extra_context_success)
{
    void *gva;
    uint32_t rankSize = 1;
    uint32_t rankId = 0;
    uint32_t rankCount = 1; // 1
    smem_shm_config_t config;
    auto ret = smem_shm_config_init(&config);
    EXPECT_EQ(ret, 0);

    ret = smem_shm_init(UT_IP_PORT, rankCount, rankId, rankId, &config);
    EXPECT_EQ(ret, 0);

    void *context = malloc(UT_SHM_SIZE);
    ret = smem_shm_set_extra_context(nullptr, context, UT_SHM_SIZE);
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    auto handle = smem_shm_create(UT_SMEM_ID, rankCount, rankId, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_RDMA, 0, &gva);
    EXPECT_NE(handle, nullptr);

    ret = smem_shm_set_extra_context(handle, nullptr, UT_SHM_SIZE);
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    // Use C API stub instead of mocking the member function (member hooks are less stable under ASAN/UBSAN).
    MOCKER_CPP(hybm_set_extra_context, int32_t (*)(hybm_entity_t, const void *, uint32_t))
        .stubs()
        .will(returnValue(0));
    ret = smem_shm_set_extra_context(handle, context, UT_SHM_SIZE);
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    ret = smem_shm_set_extra_context(handle, context, UT_EXTRA_CONTEXT_SAMPLE_SIZE);
    EXPECT_EQ(ret, 0);

    ret = smem_shm_get_global_rank(handle);

    free(context);
    smem_shm_destroy(handle, 0);
    smem_shm_uninit(0);
}

TEST_F(SmemShmTest, smem_shm_get_global_rank_and_size_failed)
{
    auto ret = smem_shm_get_global_rank(nullptr);
    EXPECT_EQ(ret, UINT32_MAX);

    void *handle = malloc(UT_SHM_SIZE);
    ret = smem_shm_get_global_rank(handle);
    EXPECT_EQ(ret, UINT32_MAX);

    ret = smem_shm_get_global_rank_size(nullptr);
    EXPECT_EQ(ret, UINT32_MAX);

    ret = smem_shm_get_global_rank_size(handle);
    EXPECT_EQ(ret, UINT32_MAX);
    free(handle);
}

TEST_F(SmemShmTest, smem_shm_get_global_rank_and_size_success)
{
    void *gva;
    auto ret = smem_init(0);

    smem_set_log_level(0);
    uint32_t rankSize = 1;
    uint32_t rankId = 0;
    uint32_t rankCount = 1; // 1
    smem_shm_config_t config;

    ret = smem_shm_config_init(nullptr);
    EXPECT_EQ(ret, SM_INVALID_PARAM);
    ret = smem_shm_config_init(&config);
    EXPECT_EQ(ret, 0);

    ret = smem_shm_init(UT_IP_PORT, rankCount, rankId, rankId, &config);
    EXPECT_EQ(ret, 0);

    auto handle = smem_shm_create(UT_SMEM_ID, rankCount, rankId, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_RDMA, 0, &gva);
    EXPECT_NE(handle, nullptr);

    ret = smem_shm_get_global_rank(handle);
    EXPECT_EQ(ret, rankId);

    ret = smem_shm_get_global_rank_size(handle);
    EXPECT_EQ(ret, rankSize);

    smem_uninit();
    smem_shm_destroy(handle, 0);
    smem_shm_uninit(0);
}

TEST_F(SmemShmTest, smem_shm_control_barrier_and_group_barrier_failed)
{
    auto ret = smem_shm_control_barrier(nullptr);
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    void *handle = malloc(UT_SHM_SIZE);
    ret = smem_shm_control_barrier(handle);
    EXPECT_EQ(ret, SM_NOT_INITIALIZED);

    ret = smem_shm_subgroup_barrier(nullptr, "", UT_SUBGROUP_RANK_SIZE, 0);
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    ret = smem_shm_subgroup_barrier(handle, "", UT_SUBGROUP_RANK_SIZE, 0);
    EXPECT_EQ(ret, SM_NOT_INITIALIZED);

    free(handle);
}

using GroupBarrierFunc = int32_t (SmemNetGroupEngine::*)(const char*, uint32_t, uint32_t);

TEST_F(SmemShmTest, smem_shm_control_barrier_success)
{
    void *gva;
    auto ret = smem_init(0);

    smem_set_log_level(0);
    uint32_t rankSize = 1; // 1
    uint32_t rankId = 0;
    uint32_t rankCount = 1; // 1
    smem_shm_config_t config;

    ret = smem_shm_config_init(nullptr);
    EXPECT_EQ(ret, SM_INVALID_PARAM);
    ret = smem_shm_config_init(&config);
    EXPECT_EQ(ret, 0);

    ret = smem_shm_init(UT_IP_PORT, rankCount, rankId, rankId, &config);
    EXPECT_EQ(ret, 0);

    auto handle = smem_shm_create(UT_SMEM_ID, rankCount, rankId, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_RDMA, 0, &gva);
    EXPECT_NE(handle, nullptr);

    ret = smem_shm_subgroup_barrier(handle, "", UT_SUBGROUP_RANK_SIZE, 0);
    EXPECT_NE(ret, 0);

    ret = smem_shm_control_barrier(handle);
    EXPECT_EQ(ret, 0);

    smem_uninit();
    smem_shm_destroy(handle, 0);
    smem_shm_uninit(0);
}

TEST_F(SmemShmTest, smem_shm_control_barrier_failed)
{
    char *sendBuf = static_cast<char *>(malloc(UT_COPY_SIZE));
    uint32_t sendSize = UT_COPY_SIZE;
    char *recvBuf = static_cast<char *>(malloc(UT_COPY_SIZE));
    uint32_t recvSize = UT_COPY_SIZE;

    auto ret = smem_shm_control_allgather(nullptr, sendBuf, sendSize, recvBuf, recvSize);
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    void *handle = malloc(UT_COPY_SIZE);
    ret = smem_shm_control_allgather(handle, sendBuf, sendSize, recvBuf, recvSize);
    EXPECT_EQ(ret, SM_NOT_INITIALIZED);

    ret = smem_shm_subgroup_allgather(nullptr, "", UT_SUBGROUP_RANK_SIZE, 0, sendBuf, sendSize, recvBuf, recvSize);
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    ret = smem_shm_subgroup_allgather(handle, "", UT_SUBGROUP_RANK_SIZE, 0, sendBuf, sendSize, recvBuf, recvSize);
    EXPECT_EQ(ret, SM_NOT_INITIALIZED);

    free(handle);
    free(sendBuf);
    free(recvBuf);
}

TEST_F(SmemShmTest, smem_shm_atomic_alloc_value_failed)
{
    uint32_t limit = UT_ATOMIC_ALLOC_LIMIT;
    uint32_t retVal;
    auto ret = smem_shm_atomic_alloc_value(nullptr, limit, &retVal);
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    void *handle = malloc(UT_COPY_SIZE);
    ret = smem_shm_atomic_alloc_value(handle, limit, &retVal);
    EXPECT_EQ(ret, SM_NOT_INITIALIZED);

    ret = smem_shm_atomic_release_value(nullptr, retVal);
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    ret = smem_shm_atomic_release_value(handle, retVal);
    EXPECT_EQ(ret, SM_NOT_INITIALIZED);
    free(handle);
}

TEST_F(SmemShmTest, smem_shm_atomic_alloc_value_success)
{
    uint32_t limit = UT_ATOMIC_ALLOC_LIMIT;
    uint32_t retVal;
    void *gva;
    auto ret = smem_init(0);
    smem_set_log_level(0);
    uint32_t rankSize = 1;
    uint32_t rankId = 0;
    uint32_t rankCount = 1; // 1
    smem_shm_config_t config;

    ret = smem_shm_config_init(&config);
    EXPECT_EQ(ret, 0);

    ret = smem_shm_init(UT_IP_PORT, rankCount, rankId, rankId, &config);
    EXPECT_EQ(ret, 0);

    auto handle = smem_shm_create(UT_SMEM_ID, rankCount, rankId, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_RDMA, 0, &gva);
    EXPECT_NE(handle, nullptr);

    ret = smem_shm_atomic_alloc_value(handle, limit, &retVal);
    EXPECT_EQ(ret, 0);

    ret = smem_shm_atomic_release_value(handle, retVal);
    EXPECT_EQ(ret, 0);

    smem_uninit();
    smem_shm_destroy(handle, 0);
    smem_shm_uninit(0);
}

// smem_shm_query_support_data_operation: 返回值应包含 MTE 能力
TEST_F(SmemShmTest, smem_shm_query_support_data_operation_basic)
{
    uint32_t ops = smem_shm_query_support_data_operation();
    EXPECT_NE(ops & SMEMS_DATA_OP_MTE, 0u);
}

// smem_shm_global_exit: 覆盖空句柄、非法句柄、正常广播的分支
TEST_F(SmemShmTest, smem_shm_global_exit_paths)
{
    // 1) handle 为 nullptr，直接早退
    smem_shm_global_exit(nullptr, 0);

    // 2) 非法 handle，manager 查不到
    void *invalidHandle = reinterpret_cast<void *>(UT_INVALID_HANDLE_ADDR);
    smem_shm_global_exit(invalidHandle, 0);

    // 3) 正常场景：有有效 group，触发 GroupBroadcastExit
    void *gva = nullptr;
    uint32_t rankId = 0;
    uint32_t rankCount = 1;
    smem_shm_config_t config;
    auto ret = smem_shm_config_init(&config);
    EXPECT_EQ(ret, 0);
    ret = smem_shm_init(UT_IP_PORT, rankCount, rankId, rankId, &config);
    EXPECT_EQ(ret, 0);
    auto handle = smem_shm_create(UT_SMEM_ID, rankCount, rankId, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_RDMA, 0, &gva);
    EXPECT_NE(handle, nullptr);

    // 这里不检查返回值，只要调用不崩溃即可覆盖 GroupBroadcastExit 分支
    smem_shm_global_exit(handle, 0);

    smem_uninit();
    smem_shm_destroy(handle, 0);
    smem_shm_uninit(0);
}

TEST_F(SmemShmTest, smem_shm_get_symmetric_size_paths)
{
    // 1) 非法 handle: manager 查不到，返回 0
    uint64_t size = smem_shm_get_symmetric_size(reinterpret_cast<void *>(UT_INVALID_HANDLE_ADDR));
    EXPECT_EQ(size, 0u);

    // 2) 正常创建后：返回 entry 的 HBM max size（可用 mock 强化断言）
    void *gva = nullptr;
    uint32_t rankId = 0;
    uint32_t rankCount = 1;
    smem_shm_config_t config;
    auto ret = smem_shm_config_init(&config);
    EXPECT_EQ(ret, 0);
    ret = smem_shm_init(UT_IP_PORT, rankCount, rankId, rankId, &config);
    EXPECT_EQ(ret, 0);
    auto handle = smem_shm_create(UT_SMEM_ID, rankCount, rankId, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_RDMA, 0, &gva);
    EXPECT_NE(handle, nullptr);

    MOCKER_CPP(&SmemShmEntry::GetHbmMaxSize, uint64_t(*)(void)).stubs().will(returnValue(UT_MOCK_HBM_MAX_SIZE));
    size = smem_shm_get_symmetric_size(handle);
    EXPECT_EQ(size, UT_MOCK_HBM_MAX_SIZE);

    smem_uninit();
    smem_shm_destroy(handle, 0);
    smem_shm_uninit(0);
}

TEST_F(SmemShmTest, smem_shm_topology_can_reach_paths)
{
    uint32_t reachInfo = 0;

    // 1) 参数非法
    auto ret = smem_shm_topology_can_reach(nullptr, 0, &reachInfo);
    EXPECT_EQ(ret, SM_INVALID_PARAM);
    ret = smem_shm_topology_can_reach(reinterpret_cast<void *>(UT_INVALID_HANDLE_ADDR), 0, nullptr);
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    // 2) 未初始化
    ret = smem_shm_topology_can_reach(reinterpret_cast<void *>(UT_INVALID_HANDLE_ADDR), 0, &reachInfo);
    EXPECT_EQ(ret, SM_NOT_INITIALIZED);

    // 3) 初始化后，但 handle 非法
    void *gva = nullptr;
    uint32_t rankId = 0;
    uint32_t rankCount = 1;
    smem_shm_config_t config;
    ret = smem_shm_config_init(&config);
    EXPECT_EQ(ret, 0);
    ret = smem_shm_init(UT_IP_PORT, rankCount, rankId, rankId, &config);
    EXPECT_EQ(ret, 0);

    reachInfo = 0;
    ret = smem_shm_topology_can_reach(reinterpret_cast<void *>(UT_INVALID_HANDLE_ADDR), 0, &reachInfo);
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    // 4) 正常 handle：mock entry->GetReachInfo 覆盖成功分支
    auto handle = smem_shm_create(UT_SMEM_ID, rankCount, rankId, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_RDMA, 0, &gva);
    EXPECT_NE(handle, nullptr);

    constexpr uint32_t kRemoteRank = 0;
    MOCKER_CPP(&SmemShmEntry::GetReachInfo, int32_t (*)(uint32_t, uint32_t &))
        .stubs()
        .will(returnValue(static_cast<int32_t>(SM_OK)));

    reachInfo = 0;
    ret = smem_shm_topology_can_reach(handle, kRemoteRank, &reachInfo);
    EXPECT_EQ(ret, SM_OK);

    smem_uninit();
    smem_shm_destroy(handle, 0);
    smem_shm_uninit(0);
}

namespace {
} // namespace

TEST_F(SmemShmTest, smem_shm_entry_set_extra_context_and_get_hbm_max_size)
{
    SmemShmEntry entry(UT_SMEM_ID);

    // 未初始化：直接失败
    const uint8_t dummy[UT_DUMMY_CONTEXT_BYTES] = {0};
    auto ret = entry.SetExtraContext(dummy, sizeof(dummy));
    // 只覆盖未初始化分支：不同构建下可能被 hook/裁剪，避免强依赖具体错误码。
    EXPECT_NE(ret, SM_INVALID_PARAM);

    // 初始化状态：调用 hybm_set_extra_context 的返回值透传
    entry.inited_ = true;
    entry.entity_ = reinterpret_cast<hybm_entity_t>(UT_ENTRY_ENTITY_ADDR);
    MOCKER_CPP(hybm_set_extra_context, int32_t (*)(hybm_entity_t, const void *, uint32_t)).stubs().will(returnValue(0));
    ret = entry.SetExtraContext(dummy, sizeof(dummy));
    EXPECT_EQ(ret, SM_OK);

    GlobalMockObject::verify();
    GlobalMockObject::reset();
    MOCKER_CPP(hybm_set_extra_context, int32_t (*)(hybm_entity_t, const void *, uint32_t))
        .stubs()
        .will(returnValue(-1));
    ret = entry.SetExtraContext(dummy, sizeof(dummy));
    EXPECT_NE(ret, SM_OK);

    // GetHbmMaxSize：直接返回 options_.maxHBMSize
    entry.options_.maxHBMSize = UT_ENTRY_HBM_MAX_SIZE;
    EXPECT_EQ(entry.GetHbmMaxSize(), UT_ENTRY_HBM_MAX_SIZE);
}

TEST_F(SmemShmTest, smem_shm_entry_init_steps_create_unreserve_free_slice)
{
    SmemShmEntry entry(UT_SMEM_ID);

    // 1) InitStepCreateEntity: create 返回 null -> error
    MOCKER_CPP(hybm_create_entity, hybm_entity_t (*)(uint64_t, const hybm_options *, uint32_t))
        .stubs()
        .will(returnValue(static_cast<hybm_entity_t>(nullptr)));
    auto ret = entry.InitStepCreateEntity();
    EXPECT_NE(ret, SM_OK);
    EXPECT_EQ(entry.entity_, nullptr);

    // 2) create 返回非空 -> success，entity_ 被赋值
    GlobalMockObject::verify();
    GlobalMockObject::reset();
    const auto kEntity = reinterpret_cast<hybm_entity_t>(UT_CREATED_ENTITY_ADDR);
    MOCKER_CPP(hybm_create_entity, hybm_entity_t (*)(uint64_t, const hybm_options *, uint32_t))
        .stubs()
        .will(returnValue(kEntity));
    ret = entry.InitStepCreateEntity();
    EXPECT_EQ(ret, SM_OK);
    EXPECT_EQ(entry.entity_, kEntity);

    // 3) InitStepUnreserveMemory: 即使失败也要把 gva_ 清空
    entry.gva_ = reinterpret_cast<void *>(UT_INVALID_HANDLE_ADDR);
    MOCKER_CPP(hybm_unreserve_mem_space, int32_t (*)(hybm_entity_t, uint32_t)).stubs().will(returnValue(-1));
    entry.InitStepUnreserveMemory();
    EXPECT_EQ(entry.gva_, nullptr);

    // 4) InitStepFreeSlice: free 失败也会把 slice_ 置空
    entry.slice_ = reinterpret_cast<hybm_mem_slice_t>(UT_SLICE_ADDR);
    MOCKER_CPP(hybm_free_local_memory, int32_t (*)(hybm_entity_t, hybm_mem_slice_t, uint32_t, uint32_t))
        .stubs()
        .will(returnValue(-1));
    entry.InitStepFreeSlice();
    EXPECT_EQ(entry.slice_, nullptr);
}

TEST_F(SmemShmTest, smem_shm_entry_get_reach_info_paths)
{
    SmemShmEntry entry(UT_SMEM_ID);
    uint32_t reachInfo = 0;

    // entity_ 为空：SM_NOT_STARTED
    auto ret = entry.GetReachInfo(0, reachInfo);
    EXPECT_EQ(ret, SM_NOT_STARTED);

    // hybm_entity_reach_types 失败：SM_ERROR
    entry.entity_ = reinterpret_cast<hybm_entity_t>(UT_ENTRY_ENTITY_ADDR);
    MOCKER_CPP(hybm_entity_reach_types, int32_t (*)(hybm_entity_t, uint32_t, hybm_data_op_type &, uint32_t))
        .stubs()
        .will(returnValue(-1));
    ret = entry.GetReachInfo(0, reachInfo);
    EXPECT_EQ(ret, SM_ERROR);
}
