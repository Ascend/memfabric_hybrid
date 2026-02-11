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
#include <thread>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gtest/gtest.h>
#include "smem.h"
#include "smem_shm.h"
#include "smem_bm.h"
#include "smem_types.h"
#include "ut_barrier_util.h"

const int32_t UT_SMEM_ID = 1;
const char UT_IP_PORT[] = "tcp://127.0.0.1:7758";
const char UT_IP_PORT2[] = "tcp://127.0.0.1:7958";
const uint32_t UT_CREATE_MEM_SIZE = 2UL * 1024UL * 1024UL;
const uint32_t UT_COPY_MEM_SIZE = 2UL * 1024UL * 1024UL;
const uint64_t UT_SHM_SIZE = 128 * 1024 * 1024ULL;

class SmemBmTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp() override;
    void TearDown() override;
};

void SmemBmTest::SetUpTestCase() {}

void SmemBmTest::TearDownTestCase() {}

void SmemBmTest::SetUp() {}

void SmemBmTest::TearDown() {}

bool CheckMem(void* base, void* ptr, uint64_t size)
{
    int32_t *arr1 = (int32_t *)base;
    int32_t *arr2 = (int32_t *)ptr;
    for (uint64_t i = 0; i < size / sizeof(int); i++) {
        if (arr1[i] != arr2[i]) {
            return false;
        }
    }
    return true;
}

TEST_F(SmemBmTest, smem_bm_config_init_success)
{
    smem_bm_config_t config;
    int32_t ret = smem_bm_config_init(&config);
    EXPECT_EQ(ret, ock::smem::SM_OK);
    EXPECT_EQ(config.initTimeout, ock::smem::SMEM_DEFAUT_WAIT_TIME);
    EXPECT_EQ(config.createTimeout, ock::smem::SMEM_DEFAUT_WAIT_TIME);
    EXPECT_EQ(config.controlOperationTimeout, ock::smem::SMEM_DEFAUT_WAIT_TIME);
    EXPECT_TRUE(config.startConfigStoreServer);
    EXPECT_FALSE(config.startConfigStoreOnly);
    EXPECT_FALSE(config.dynamicWorldSize);
    EXPECT_TRUE(config.unifiedAddressSpace);
    EXPECT_TRUE(config.autoRanking);
    EXPECT_EQ(config.flags, 0u);
}

TEST_F(SmemBmTest, smem_bm_config_init_invalid_param)
{
    int32_t ret = smem_bm_config_init(nullptr);
    EXPECT_EQ(ret, ock::smem::SM_INVALID_PARAM);
}

TEST_F(SmemBmTest, smem_bm_init_invalid_params)
{
    smem_bm_config_t config;
    EXPECT_EQ(smem_bm_config_init(&config), ock::smem::SM_OK);

    int32_t ret = smem_bm_init(nullptr, 1, 0, &config);
    EXPECT_EQ(ret, ock::smem::SM_INVALID_PARAM);

    ret = smem_bm_init(UT_IP_PORT2, 0, 0, &config);
    EXPECT_EQ(ret, ock::smem::SM_INVALID_PARAM);

    config.unifiedAddressSpace = false;
    ret = smem_bm_init(UT_IP_PORT2, 1, 0, &config);
    EXPECT_EQ(ret, ock::smem::SM_INVALID_PARAM);
}

TEST_F(SmemBmTest, smem_bm_create_before_init)
{
    smem_bm_t handle = smem_bm_create(0, 1, SMEMB_DATA_OP_SDMA, 1024, 0, 0);
    EXPECT_EQ(handle, nullptr);
}

TEST_F(SmemBmTest, smem_bm_create2_before_init)
{
    smem_bm_t handle = smem_bm_create2(0, nullptr);
    EXPECT_EQ(handle, nullptr);
}

TEST_F(SmemBmTest, smem_bm_get_local_mem_size_invalid_handle)
{
    uint64_t size = smem_bm_get_local_mem_size_by_mem_type(nullptr, SMEM_MEM_TYPE_HOST);
    EXPECT_EQ(size, 0UL);
}

TEST_F(SmemBmTest, smem_bm_ptr_by_mem_type_invalid)
{
    void *ptr = smem_bm_ptr_by_mem_type(nullptr, SMEM_MEM_TYPE_HOST, 0);
    EXPECT_EQ(ptr, nullptr);

    smem_bm_t fakeHandle = reinterpret_cast<smem_bm_t>(0x1);
    ptr = smem_bm_ptr_by_mem_type(fakeHandle, SMEM_MEM_TYPE_HOST, 0);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(SmemBmTest, smem_bm_copy_invalid_params)
{
    smem_copy_params params = {nullptr, nullptr, 0};

    int32_t ret = smem_bm_copy(nullptr, &params, SMEMB_COPY_G2G, 0);
    EXPECT_EQ(ret, ock::smem::SM_INVALID_PARAM);

    smem_bm_t fakeHandle = reinterpret_cast<smem_bm_t>(0x1);
    ret = smem_bm_copy(fakeHandle, nullptr, SMEMB_COPY_G2G, 0);
    EXPECT_EQ(ret, ock::smem::SM_INVALID_PARAM);

    ret = smem_bm_copy(fakeHandle, &params, SMEMB_COPY_G2G, 0);
    EXPECT_EQ(ret, ock::smem::SM_NOT_INITIALIZED);
}

TEST_F(SmemBmTest, smem_bm_copy_batch_invalid_params)
{
    smem_batch_copy_params params{};
    smem_bm_t fakeHandle = reinterpret_cast<smem_bm_t>(0x1);

    int32_t ret = smem_bm_copy_batch(nullptr, &params, SMEMB_COPY_G2G, 0);
    EXPECT_EQ(ret, ock::smem::SM_INVALID_PARAM);

    ret = smem_bm_copy_batch(fakeHandle, nullptr, SMEMB_COPY_G2G, 0);
    EXPECT_EQ(ret, ock::smem::SM_INVALID_PARAM);

    ret = smem_bm_copy_batch(fakeHandle, &params, SMEMB_COPY_G2G, 0);
    EXPECT_EQ(ret, ock::smem::SM_NOT_INITIALIZED);
}

TEST_F(SmemBmTest, smem_bm_wait_invalid_params)
{
    int32_t ret = smem_bm_wait(nullptr);
    EXPECT_EQ(ret, ock::smem::SM_INVALID_PARAM);

    smem_bm_t fakeHandle = reinterpret_cast<smem_bm_t>(0x1);
    ret = smem_bm_wait(fakeHandle);
    EXPECT_EQ(ret, ock::smem::SM_NOT_INITIALIZED);
}

TEST_F(SmemBmTest, smem_bm_get_rank_id_by_gva_invalid_params)
{
    uint32_t ret = smem_bm_get_rank_id_by_gva(nullptr, nullptr);
    EXPECT_EQ(ret, static_cast<uint32_t>(ock::smem::SM_INVALID_PARAM));

    smem_bm_t fakeHandle = reinterpret_cast<smem_bm_t>(0x1);
    ret = smem_bm_get_rank_id_by_gva(fakeHandle, nullptr);
    EXPECT_EQ(ret, static_cast<uint32_t>(ock::smem::SM_NOT_INITIALIZED));
}

TEST_F(SmemBmTest, smem_bm_register_user_mem_invalid_params)
{
    smem_bm_t fakeHandle = reinterpret_cast<smem_bm_t>(0x1);

    int32_t ret = smem_bm_register_user_mem(nullptr, 0x1000, 1024);
    EXPECT_EQ(ret, ock::smem::SM_INVALID_PARAM);

    ret = smem_bm_register_user_mem(fakeHandle, 0, 1024);
    EXPECT_EQ(ret, ock::smem::SM_INVALID_PARAM);

    ret = smem_bm_register_user_mem(fakeHandle, 0x1000, 1024);
    EXPECT_EQ(ret, ock::smem::SM_NOT_INITIALIZED);
}

TEST_F(SmemBmTest, smem_bm_unregister_user_mem_invalid_params)
{
    smem_bm_t fakeHandle = reinterpret_cast<smem_bm_t>(0x1);

    int32_t ret = smem_bm_unregister_user_mem(nullptr, 0x1000);
    EXPECT_EQ(ret, ock::smem::SM_INVALID_PARAM);

    ret = smem_bm_unregister_user_mem(fakeHandle, 0);
    EXPECT_EQ(ret, ock::smem::SM_INVALID_PARAM);

    ret = smem_bm_unregister_user_mem(fakeHandle, 0x1000);
    EXPECT_EQ(ret, ock::smem::SM_NOT_INITIALIZED);
}

TEST_F(SmemBmTest, smem_bm_uninit_without_init_safe)
{
    smem_bm_uninit(0);
}

/*
TEST_F(SmemBmTest, two_card_shm_create_success)
{
    smem_set_log_level(0);
    uint32_t rankSize = 2;
    std::thread ts[rankSize];
    auto func = [](uint32_t rank, uint32_t rankCount) {
        void *gva;
        int32_t ret = smem_init(0);
        if (ret != 0) {
            exit(1);
        }

        smem_shm_config_t config;
        ret = smem_shm_config_init(&config);
        if (ret != 0) {
            exit(2);
        }
        ret = smem_shm_init(UT_IP_PORT, rankCount, rank, rank, &config);
        if (ret != 0) {
            exit(3);
        }

        auto handle = smem_shm_create(UT_SMEM_ID, rankCount, rank, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_MTE, 0, &gva);
        if (handle == nullptr) {
            exit(4);
        }
        smem_shm_destroy(handle, 0);
        smem_shm_uninit(0);
    };

    pid_t pids[rankSize];
    uint32_t maxProcess = rankSize;
    bool needKillOthers = false;
    for (uint32_t i = 0; i < rankSize; ++i) {
        pids[i] = fork();
        EXPECT_NE(pids[i], -1);
        if (pids[i] == -1) {
            maxProcess = i;
            needKillOthers = true;
            break;
        }
        if (pids[i] == 0) {
            func(i, rankSize);
            exit(0);
        }
    }

    if (needKillOthers) {
        for (uint32_t i = 0; i < maxProcess; ++i) {
            int status = 0;
            kill(pids[i], SIGKILL);
            waitpid(pids[i], &status, 0);
        }
        ASSERT_NE(needKillOthers, true);
    }

    for (uint32_t i = 0; i < rankSize; ++i) {
        int status = 0;
        if (needKillOthers) {
            kill(pids[i], SIGKILL);
        }
        waitpid(pids[i], &status, 0);
        EXPECT_EQ(WIFEXITED(status), true);
        if (WIFEXITED(status)) {
            EXPECT_EQ(WEXITSTATUS(status), 0);
            if (WEXITSTATUS(status) != 0) {
                needKillOthers = true;
            }
        } else {
            needKillOthers = true;
        }
    }
}

TEST_F(SmemBmTest, two_crad_bm_copy_success)
{
    smem_set_log_level(0);
    uint32_t rankSize = 2;
    auto func = [](uint32_t rank, uint32_t rankCount) {
        int32_t ret = smem_init(0);
        if (ret != 0) {
            exit(1);
        }

        smem_bm_config_t config;
        ret = smem_bm_config_init(&config);
        if (ret != 0) {
            exit(2);
        }
        config.rankId = rank;
        ret = smem_bm_init(UT_IP_PORT2, rankCount, rank, &config);
        if (ret != 0) {
            exit(3);
        }

        auto barrier = new (std::nothrow) BarrierUtil;
        if (barrier == nullptr) {
            exit(4);
        }
        ret = barrier->Init(rank, rank, rankCount, UT_IP_PORT2);
        if (ret != 0) {
            exit(5);
        }

        auto handle = smem_bm_create(0, rankCount, SMEMB_DATA_OP_SDMA, 0, UT_CREATE_MEM_SIZE, 0);
        if (handle == nullptr) {
            exit(6);
        }

        ret = smem_bm_join(handle, 0);
        if (ret != 0) {
            exit(22);
        }

        ret = barrier->Barrier();
        if (ret != 0) {
            exit(7);
        }

        smem_bm_mem_type memType = SMEM_MEM_TYPE_DEVICE;

        void *local = smem_bm_ptr_by_mem_type(handle, memType, rank);
        if (local == nullptr) {
            exit(8);
        }
        void *remote = smem_bm_ptr_by_mem_type(handle, memType, (rank + 1) % rankCount);
        if (remote == nullptr) {
            exit(9);
        }
        void *hostSrc = malloc(UT_COPY_MEM_SIZE);
        void *hostDst = malloc(UT_COPY_MEM_SIZE);
        if (hostDst == nullptr || hostSrc == nullptr) {
            exit(10);
        }
        memset(hostSrc, rank + 1, UT_COPY_MEM_SIZE);
        memset(hostDst, 0, UT_COPY_MEM_SIZE);

        smem_copy_params params = {hostSrc, remote, UT_COPY_MEM_SIZE};
        ret = smem_bm_copy(handle, &params, SMEMB_COPY_H2G, 0);
        if (ret != 0) {
            exit(11);
        }
        ret = barrier->Barrier();
        if (ret != 0) {
            exit(12);
        }

        params = {remote, hostDst, UT_COPY_MEM_SIZE};
        ret = smem_bm_copy(handle, &params, SMEMB_COPY_G2H, 0);
        if (ret != 0) {
            exit(13);
        }

        ret = barrier->Barrier();
        if (ret != 0) {
            exit(14);
        }
        auto cpyRet = CheckMem(hostSrc, hostDst, UT_COPY_MEM_SIZE);
        free(hostSrc);
        free(hostDst);
        smem_bm_destroy(handle);
        delete barrier;
        barrier = nullptr;
        smem_bm_uninit(0);
    };
    pid_t pids[rankSize];
    uint32_t maxProcess = rankSize;
    bool needKillOthers = false;
    for (uint32_t i = 0; i < rankSize; ++i) {
        pids[i] = fork();
        EXPECT_NE(pids[i], -1);
        if (pids[i] == -1) {
            maxProcess = i;
            needKillOthers = true;
            break;
        }
        if (pids[i] == 0) {
            func(i, rankSize);
            exit(0);
        }
    }

    if (needKillOthers) {
        for (uint32_t i = 0; i < maxProcess; ++i) {
            int status = 0;
            kill(pids[i], SIGKILL);
            waitpid(pids[i], &status, 0);
        }
        ASSERT_NE(needKillOthers, true);
    }

    for (uint32_t i = 0; i < rankSize; ++i) {
        int status = 0;
        if (needKillOthers) {
            kill(pids[i], SIGKILL);
        }
        waitpid(pids[i], &status, 0);
        EXPECT_EQ(WIFEXITED(status), true);
        if (WIFEXITED(status)) {
            EXPECT_EQ(WEXITSTATUS(status), 0);
            if (WEXITSTATUS(status) != 0) {
                needKillOthers = true;
            }
        } else {
            needKillOthers = true;
        }
    }
}
 */