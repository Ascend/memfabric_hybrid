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
#include "smem_shm.h"
#include "smem_types.h"
#include "ut_barrier_util.h"
#include "hybm.h"
#include "smem_ref.h"
#include "smem_shm_entry_manager.h"
#include "smem_net_group_engine.h"
#include "smem_logger.h"
#include "hybm_def.h"

#define private public
#include "smem_shm_entry.h"
#undef private

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

const int32_t UT_SMEM_ID = 1;
const char UT_IP_PORT[] = "tcp://127.0.0.1:7758";
const char UT_IP_PORT2[] = "tcp://127.0.0.1:7958";
const uint32_t UT_CREATE_MEM_SIZE = 2UL * 1024UL * 1024UL;
const uint32_t UT_COPY_MEM_SIZE = 2UL * 1024UL * 1024UL;
const uint64_t UT_SHM_SIZE = 128 * 1024 * 1024ULL;
const uint32_t BATCH_SIZE = 5;
const uint64_t COPY_SIZE = 1 * 1024ULL;
const uint64_t GVA_SIZE = 2 * 1024ULL * 1024 * 1024;
const int32_t RANDOM_MULTIPLIER = 23;
const int32_t RANDOM_INCREMENT = 17;
const int32_t NEGATIVE_RATIO_DIVISOR = 3;

using namespace ock::smem;

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

    MOCKER_CPP(&SmemShmEntry::SetExtraContext, int32_t (*)(const void *, uint32_t)).stubs().will(returnValue(0));
    ret = smem_shm_set_extra_context(handle, context, UT_SHM_SIZE);
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    ret = smem_shm_set_extra_context(handle, context, 100); // 100
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

    ret = smem_shm_subgroup_barrier(nullptr, "", 1024, 0); // 1024
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    ret = smem_shm_subgroup_barrier(handle, "", 1024, 0); // 1024
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

    ret = smem_shm_subgroup_barrier(handle, "", 1024, 0); // 1024
    EXPECT_NE(ret, 0);

    ret = smem_shm_control_barrier(handle);
    EXPECT_EQ(ret, 0);

    smem_uninit();
    smem_shm_destroy(handle, 0);
    smem_shm_uninit(0);
}

TEST_F(SmemShmTest, smem_shm_control_barrier_failed)
{
    char *sendBuf = static_cast<char*>(malloc(COPY_SIZE));
    uint32_t sendSize = COPY_SIZE;
    char *recvBuf = static_cast<char*>(malloc(COPY_SIZE));
    uint32_t recvSize = COPY_SIZE;

    auto ret = smem_shm_control_allgather(nullptr, sendBuf, sendSize, recvBuf, recvSize);
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    void *handle = malloc(COPY_SIZE);
    ret = smem_shm_control_allgather(handle, sendBuf, sendSize, recvBuf, recvSize);
    EXPECT_EQ(ret, SM_NOT_INITIALIZED);

    ret = smem_shm_subgroup_allgather(nullptr, "", 1024, 0, sendBuf, sendSize, recvBuf, recvSize); // 1024
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    ret = smem_shm_subgroup_allgather(handle, "", 1024, 0, sendBuf, sendSize, recvBuf, recvSize); // 1024
    EXPECT_EQ(ret, SM_NOT_INITIALIZED);

    free(handle);
    free(sendBuf);
    free(recvBuf);
}

TEST_F(SmemShmTest, smem_shm_atomic_alloc_value_failed)
{
    uint32_t limit = 1024u; // 1024
    uint32_t retVal;
    auto ret = smem_shm_atomic_alloc_value(nullptr, limit, &retVal);
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    void *handle = malloc(COPY_SIZE);
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
    uint32_t limit = 1024u; // 1024
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