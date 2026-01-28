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
#include <sys/stat.h>
#include <unistd.h>
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <array>
#include <thread>
#include <chrono>
#include "smem_types.h"
#include "smem_store_factory.h"
#include "smem_trans.h"
#include "dl_acl_api.h"
#include "dl_api.h"
#include "hybm_mem_segment.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace ock::smem;
using namespace ock::mf;

const char STORE_URL[] = "tcp://127.0.0.1:5432";
const char UNIQUE_ID[] = "127.0.0.1:5321";
const char STORE_URL_IPV6[] = "tcp://[::1]:5432";
const char UNIQUE_IPV6_ID[] = "[::1]:5321";
const uint32_t TRANS_TEST_WAIT_TIME = 8; // 8s
const smem_trans_config_t g_trans_options = {SMEM_TRANS_SENDER, SMEM_DEFAUT_WAIT_TIME, 0, 0};

class SmemTransTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp() override;
    void TearDown() override;
};

void SmemTransTest::SetUpTestCase() {}

void SmemTransTest::TearDownTestCase() {}

void SmemTransTest::SetUp() {}

void SmemTransTest::TearDown()
{
    GlobalMockObject::verify();
    GlobalMockObject::reset();
}

TEST_F(SmemTransTest, smem_trans_config_init_success)
{
    smem_trans_config_t config;
    EXPECT_EQ(smem_trans_config_init(&config), SM_OK);
}

TEST_F(SmemTransTest, smem_trans_config_init_failed_invalid_param)
{
    EXPECT_EQ(smem_trans_config_init(nullptr), SM_INVALID_PARAM);
}

TEST_F(SmemTransTest, smem_trans_create_success)
{
    pid_t pid = fork();
    EXPECT_NE(pid, -1);

    if (pid == 0) {
        smem_tls_config tls;
        tls.tlsEnable = false;
        ock::smem::StoreFactory::SetTlsInfo(tls);

        int32_t ret = smem_create_config_store(STORE_URL);
        if (ret != 0) {
            exit(1);
        }

        ret = smem_trans_init(&g_trans_options);
        if (ret != 0) {
            exit(2);
        }

        auto handle = smem_trans_create(STORE_URL, UNIQUE_ID, &g_trans_options);
        if (handle == nullptr) {
            exit(3);
        }

        smem_trans_destroy(handle, 0);
        smem_trans_uninit(0);
        exit(0);
    }

    int status;
    EXPECT_NE(waitpid(pid, &status, 0), -1);

    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST_F(SmemTransTest, smem_trans_create_success_ipv6)
{
    pid_t pid = fork();
    EXPECT_NE(pid, -1);

    if (pid == 0) {
        smem_tls_config tls;
        tls.tlsEnable = false;
        ock::smem::StoreFactory::SetTlsInfo(tls);

        int32_t ret = smem_create_config_store(STORE_URL_IPV6);
        if (ret != 0) {
            exit(1);
        }

        ret = smem_trans_init(&g_trans_options);
        if (ret != 0) {
            exit(2);
        }

        auto handle = smem_trans_create(STORE_URL_IPV6, UNIQUE_IPV6_ID, &g_trans_options);
        if (handle == nullptr) {
            exit(3);
        }

        smem_trans_destroy(handle, 0);
        smem_trans_uninit(0);

        exit(0);
    }

    int status;
    EXPECT_NE(waitpid(pid, &status, 0), -1);

    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST_F(SmemTransTest, smem_trans_create_failed_invalid_param)
{
    // not initialized
    EXPECT_EQ(smem_trans_create(nullptr, UNIQUE_ID, &g_trans_options), nullptr);

    int ret = smem_trans_init(&g_trans_options);
    EXPECT_EQ(ret, 0);

    // storeUrl == nullptr
    EXPECT_EQ(smem_trans_create(nullptr, UNIQUE_ID, &g_trans_options), nullptr);
    // uniqueId == nullptr
    EXPECT_EQ(smem_trans_create(STORE_URL, nullptr, &g_trans_options), nullptr);
    // config == nullptr
    EXPECT_EQ(smem_trans_create(STORE_URL, UNIQUE_ID, nullptr), nullptr);

    // storeUrl or uniqueId is empty
    const char STORE_URL_TEST1[] = "";
    const char UNIQUE_ID_TEST[] = "";
    EXPECT_EQ(smem_trans_create(STORE_URL_TEST1, UNIQUE_ID, &g_trans_options), nullptr);
    EXPECT_EQ(smem_trans_create(STORE_URL, UNIQUE_ID_TEST, &g_trans_options), nullptr);
    EXPECT_EQ(smem_trans_create(STORE_URL_IPV6, UNIQUE_ID_TEST, &g_trans_options), nullptr);

    smem_trans_uninit(0);
}

TEST_F(SmemTransTest, smem_trans_register_mem_failed_invalid_param)
{
    pid_t pid = fork();
    EXPECT_NE(pid, -1);

    if (pid == 0) {
        uint8_t flag = 0;
        int *address = new int[10];
        size_t size = 10 * sizeof(int);

        // first create server
        smem_set_conf_store_tls(false, nullptr, 0);
        smem_create_config_store(STORE_URL);
        int ret = smem_trans_init(&g_trans_options);
        EXPECT_EQ(ret, 0);
        // client connect to server when initializing
        auto handle = smem_trans_create(STORE_URL, UNIQUE_ID, &g_trans_options);

        // handle = nullptr
        ret = smem_trans_register_mem(nullptr, address, size, 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 1;
            goto cleanup;
        }
        // address = nullptr
        ret = smem_trans_register_mem(handle, nullptr, size, 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 2;
            goto cleanup;
        }
        // size = 0
        ret = smem_trans_register_mem(handle, address, 0, 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 3;
            goto cleanup;
        }

    cleanup:
        delete[] address;
        smem_trans_destroy(handle, 0);
        smem_trans_uninit(0);
        exit(flag);
    }

    int status;
    EXPECT_NE(waitpid(pid, &status, 0), -1);

    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST_F(SmemTransTest, smem_trans_register_mem_failed_invalid_param_ipv6)
{
    pid_t pid = fork();
    EXPECT_NE(pid, -1);

    if (pid == 0) {
        uint8_t flag = 0;
        int *address = new int[10];
        size_t size = 10 * sizeof(int);

        // first create server
        smem_set_conf_store_tls(false, nullptr, 0);
        smem_create_config_store(STORE_URL_IPV6);
        int ret = smem_trans_init(&g_trans_options);
        EXPECT_EQ(ret, 0);
        // client connect to server when initializing
        auto handle = smem_trans_create(STORE_URL_IPV6, UNIQUE_IPV6_ID, &g_trans_options);

        // handle = nullptr
        ret = smem_trans_register_mem(nullptr, address, size, 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 1;
            goto cleanup;
        }
        // address = nullptr
        ret = smem_trans_register_mem(handle, nullptr, size, 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 2;
            goto cleanup;
        }
        // size = 0
        ret = smem_trans_register_mem(handle, address, 0, 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 3;
            goto cleanup;
        }

    cleanup:
        delete[] address;
        smem_trans_destroy(handle, 0);
        smem_trans_uninit(0);
        exit(flag);
    }

    int status;
    EXPECT_NE(waitpid(pid, &status, 0), -1);

    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST_F(SmemTransTest, smem_trans_read_write)
{
    uint32_t rankSize = 2;
    int *sender_buffer = new int[500];
    int *recv_buffer = new int[500];
    size_t capacities = 500 * sizeof(int);
    smem_trans_config_t sender_trans_options = {SMEM_TRANS_SENDER, SMEM_DEFAUT_WAIT_TIME, 0, 0};
    smem_trans_config_t recv_trans_options = {SMEM_TRANS_RECEIVER, SMEM_DEFAUT_WAIT_TIME, 1, 0};

    auto func = [](uint32_t rank, uint32_t rankCount, smem_trans_config_t trans_options, std::vector<int *> addrPtrs,
                   size_t capacities, const std::array<const char *, 2> unique_ids) {
        trans_options.dataOpType = SMEMB_DATA_OP_SDMA;
        int ret = smem_trans_init(&trans_options);
        if (ret != 0) {
            exit(1);
        }
        auto handle = smem_trans_create(STORE_URL, unique_ids[rank], &trans_options);
        if (handle == nullptr) {
            exit(2);
        }

        ret = smem_trans_register_mem(handle, addrPtrs[rank], capacities, 0);
        if (ret != SM_OK) {
            exit(3);
        }

        if (rank == 0) {
            ret = smem_trans_write(handle, addrPtrs[0], unique_ids[1], addrPtrs[1], capacities, 0);
            if (ret != SM_OK) {
                exit(4);
            }
        }

        ret = smem_trans_deregister_mem(handle, addrPtrs[rank]);
        if (ret != SM_OK) {
            exit(5);
        }

        smem_trans_destroy(handle, 0);
        smem_trans_uninit(0);
    };

    const std::array<const char *, 2> unique_ids = {{"127.0.0.1:5321", "127.0.0.1:5322"}};
    std::vector<int *> addrPtrs = {sender_buffer, recv_buffer};
    std::vector<smem_trans_config_t> trans_options = {sender_trans_options, recv_trans_options};

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
            smem_set_conf_store_tls(false, nullptr, 0);
            if (i == 0) {
                smem_create_config_store(STORE_URL);
            }
            func(i, rankSize, trans_options[i], addrPtrs, capacities, unique_ids);
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
        waitpid(pids[i], &status, 0);
        EXPECT_EQ(WIFEXITED(status), true);
        if (WIFEXITED(status)) {
            EXPECT_EQ(WEXITSTATUS(status), 0);
            if (WEXITSTATUS(status) != 0 && !needKillOthers) {
                needKillOthers = true;
                for (uint32_t j = 0; j < rankSize; ++j) {
                    if (i != j && pids[j] > 0) {
                        kill(pids[j], SIGKILL);
                    }
                }
            }
        } else {
            needKillOthers = true;
            for (uint32_t j = 0; j < rankSize; ++j) {
                if (i != j && pids[j] > 0) {
                    kill(pids[j], SIGKILL);
                }
            }
        }
    }
    delete[] sender_buffer;
    delete[] recv_buffer;
}

TEST_F(SmemTransTest, smem_trans_malloc)
{
    size_t mallocTinySize = 0x8000000; // 128MB
    size_t mallocMiddleSize = mallocTinySize * 8; // 1GB
    size_t mallocHugeSize = mallocMiddleSize * 64; // 128GB
    size_t mallocOversize = mallocHugeSize * 129; // 512GB

    smem_trans_config_t trans_options = {SMEM_TRANS_SENDER, SMEM_DEFAUT_WAIT_TIME, 0, 0, SMEMB_DATA_OP_SDMA, 1};
    auto ret = smem_trans_init(&trans_options);
    EXPECT_EQ(ret, 0);
    auto handle = smem_trans_create(STORE_URL, "127.0.0.1:5321", &trans_options);
    EXPECT_NE(handle, nullptr);

    auto ptr = smem_trans_malloc(handle, mallocTinySize);
    EXPECT_NE(ptr, nullptr);
    ret = smem_trans_free(handle, ptr);
    EXPECT_EQ(ret, 0);

    ptr = smem_trans_malloc(handle, mallocMiddleSize);
    EXPECT_NE(ptr, nullptr);
    ret = smem_trans_free(handle, ptr);
    EXPECT_EQ(ret, 0);

    ptr = smem_trans_malloc(handle, mallocHugeSize);
    EXPECT_NE(ptr, nullptr);
    ret = smem_trans_free(handle, ptr);
    EXPECT_EQ(ret, 0);

    ptr = smem_trans_malloc(handle, mallocOversize);
    EXPECT_EQ(ptr, nullptr);
    smem_trans_destroy(handle, 0);
    smem_trans_uninit(0);
}

TEST_F(SmemTransTest, smem_trans_write_dram)
{
    uint32_t rankSize = 2;
    size_t capacities = 0x200000; // 最少2M对齐
    smem_trans_config_t sender_trans_options = {SMEM_TRANS_SENDER, SMEM_DEFAUT_WAIT_TIME, 0, 0};
    smem_trans_config_t recv_trans_options = {SMEM_TRANS_RECEIVER, SMEM_DEFAUT_WAIT_TIME, 1, 0};

    int pipe_fd[2]; // C2 -> C1
    EXPECT_NE(pipe(pipe_fd), -1);

    auto func = [pipe_fd](uint32_t rank, uint32_t rankCount, smem_trans_config_t trans_options,
                          size_t capacities, const std::array<const char *, 2> unique_ids) {
        if (rank == 1) {
            std::this_thread::sleep_for(std::chrono::seconds(TRANS_TEST_WAIT_TIME));
        }
        trans_options.dataOpType = SMEMB_DATA_OP_SDMA;
        int ret = smem_trans_init(&trans_options);
        if (ret != 0) {
            exit(1);
        }
        auto handle = smem_trans_create(STORE_URL, unique_ids[rank], &trans_options);
        if (handle == nullptr) {
            exit(2);
        }

        // 申请内存
        auto ptr = smem_trans_malloc(handle, capacities);
        if (ptr == nullptr) {
            exit(3);
        }

        // 接收dst addr
        if (rank == 0) {
            close(pipe_fd[1]);
            void* dst_addr;
            ssize_t n = read(pipe_fd[0], &dst_addr, sizeof(dst_addr));
            close(pipe_fd[0]);
            std::this_thread::sleep_for(std::chrono::seconds(TRANS_TEST_WAIT_TIME));
            ret = smem_trans_write(handle, ptr, unique_ids[1], dst_addr, capacities, 0);
            if (ret != SM_OK) {
                exit(4);
            }
        } else {
            close(pipe_fd[0]);
            if (write(pipe_fd[1], &ptr, sizeof(ptr)) != sizeof(ptr)) {
                std::cerr << "rank1 send addr failed" << std::endl;
            }
            close(pipe_fd[1]);
        }
        if (rank == 1) {
            std::this_thread::sleep_for(std::chrono::seconds(TRANS_TEST_WAIT_TIME));
        }
        std::cerr << " will cleanup rank:" << rank << std::endl;
        ret = smem_trans_free(handle, ptr);
        if (ret != 0) {
            exit(5);
        }
        smem_trans_destroy(handle, 0);
        smem_trans_uninit(0);
    };

    const std::array<const char *, 2> unique_ids = {{"127.0.0.1:5321", "127.0.0.1:5322"}};
    std::vector<smem_trans_config_t> trans_options = {sender_trans_options, recv_trans_options};

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
            smem_set_conf_store_tls(false, nullptr, 0);
            if (i == 0) {
                smem_create_config_store(STORE_URL);
            }
            func(i, rankSize, trans_options[i], capacities, unique_ids);
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
        waitpid(pids[i], &status, 0);
        EXPECT_EQ(WIFEXITED(status), true);
        if (WIFEXITED(status)) {
            EXPECT_EQ(WEXITSTATUS(status), 0);
            if (WEXITSTATUS(status) != 0 && !needKillOthers) {
                needKillOthers = true;
                for (uint32_t j = 0; j < rankSize; ++j) {
                    if (i != j && pids[j] > 0) {
                        kill(pids[j], SIGKILL);
                    }
                }
            }
        } else {
            needKillOthers = true;
            for (uint32_t j = 0; j < rankSize; ++j) {
                if (i != j && pids[j] > 0) {
                    kill(pids[j], SIGKILL);
                }
            }
        }
    }
}

TEST_F(SmemTransTest, smem_trans_write_ipv6)
{
    uint32_t rankSize = 2;
    int *sender_buffer = new int[500];
    int *recv_buffer = new int[500];
    size_t capacities = 500 * sizeof(int);
    smem_trans_config_t sender_trans_options = {SMEM_TRANS_SENDER, SMEM_DEFAUT_WAIT_TIME, 0, 0};
    smem_trans_config_t recv_trans_options = {SMEM_TRANS_RECEIVER, SMEM_DEFAUT_WAIT_TIME, 1, 0};

    auto func = [](uint32_t rank, uint32_t rankCount, smem_trans_config_t trans_options, std::vector<int *> addrPtrs,
                   size_t capacities, const std::array<const char *, 2> unique_ids) {
        trans_options.dataOpType = SMEMB_DATA_OP_SDMA;
        int ret = smem_trans_init(&trans_options);
        if (ret != 0) {
            exit(1);
        }
        auto handle = smem_trans_create(STORE_URL_IPV6, unique_ids[rank], &trans_options);
        if (handle == nullptr) {
            exit(2);
        }

        ret = smem_trans_register_mem(handle, addrPtrs[rank], capacities, 0);
        if (ret != SM_OK) {
            exit(3);
        }

        if (rank == 0) {
            ret = smem_trans_write(handle, addrPtrs[0], unique_ids[1], addrPtrs[1], capacities, 0);
            if (ret != SM_OK) {
                exit(4);
            }

            uint64_t stream_cnt = 0;
            ret = smem_trans_write_submit(handle, addrPtrs[0], unique_ids[1], addrPtrs[1], capacities,
                                          (void *)&stream_cnt, 0);
            if (ret != SM_OK) {
                exit(6);
            }
            if (stream_cnt != 1) {
                exit(7);
            }

            ret = smem_trans_read(handle, addrPtrs[0], unique_ids[1], addrPtrs[1], capacities, 0);
            if (ret != SM_OK) {
                exit(8);
            }
            stream_cnt = 0;
            ret = smem_trans_read_submit(handle, addrPtrs[0], unique_ids[1], addrPtrs[1], capacities,
                                         (void *)&stream_cnt, 0);
            if (ret != SM_OK) {
                exit(9);
            }
            if (stream_cnt != 1) {
                exit(10);
            }
        }

        ret = smem_trans_deregister_mem(handle, addrPtrs[rank]);
        if (ret != SM_OK) {
            exit(5);
        }

        smem_trans_destroy(handle, 0);
        smem_trans_uninit(0);
    };

    const std::array<const char *, 2> unique_ids = {{"[::]:5321", "[::]:5322"}};
    std::vector<int *> addrPtrs = {sender_buffer, recv_buffer};
    std::vector<smem_trans_config_t> trans_options = {sender_trans_options, recv_trans_options};

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
            smem_set_conf_store_tls(false, nullptr, 0);
            if (i == 0) {
                smem_create_config_store(STORE_URL_IPV6);
            }
            func(i, rankSize, trans_options[i], addrPtrs, capacities, unique_ids);
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
        waitpid(pids[i], &status, 0);
        EXPECT_EQ(WIFEXITED(status), true);
        if (WIFEXITED(status)) {
            EXPECT_EQ(WEXITSTATUS(status), 0);
            if (WEXITSTATUS(status) != 0 && !needKillOthers) {
                needKillOthers = true;
                for (uint32_t j = 0; j < rankSize; ++j) {
                    if (i != j && pids[j] > 0) {
                        kill(pids[j], SIGKILL);
                    }
                }
            }
        } else {
            needKillOthers = true;
            for (uint32_t j = 0; j < rankSize; ++j) {
                if (i != j && pids[j] > 0) {
                    kill(pids[j], SIGKILL);
                }
            }
        }
    }
    delete[] sender_buffer;
    delete[] recv_buffer;
}

TEST_F(SmemTransTest, smem_trans_batch_read_write)
{
    uint32_t rankSize = 2;
    int *sender_buffer = new int[500];
    int *recv_buffer = new int[500];
    std::vector<void *> sender_addrPtrs = {sender_buffer};
    std::vector<void *> recv_addrPtrs = {recv_buffer};
    std::vector<size_t> capacities = {500 * sizeof(int)};
    smem_trans_config_t sender_trans_options = {SMEM_TRANS_SENDER, SMEM_DEFAUT_WAIT_TIME, 0, 0};
    smem_trans_config_t recv_trans_options = {SMEM_TRANS_RECEIVER, SMEM_DEFAUT_WAIT_TIME, 1, 0};

    auto func = [](uint32_t rank, uint32_t rankCount, smem_trans_config_t trans_options,
                   std::vector<std::vector<void *>> addrPtrs, std::vector<size_t> capacities,
                   const std::array<const char *, 2> unique_ids) {
        trans_options.dataOpType = SMEMB_DATA_OP_SDMA;
        int ret = smem_trans_init(&trans_options);
        if (ret != 0) {
            exit(1);
        }
        auto handle = smem_trans_create(STORE_URL, unique_ids[rank], &trans_options);
        if (handle == nullptr) {
            exit(2);
        }

        ret = smem_trans_batch_register_mem(handle, addrPtrs[rank].data(), capacities.data(), 1, 0);
        if (ret != SM_OK) {
            exit(3);
        }

        if (rank == 0) {
            const void *srcAddr[] = {addrPtrs[0][0]};
            ret = smem_trans_batch_write(handle, srcAddr, unique_ids[1], addrPtrs[1].data(), capacities.data(), 1, 0);
            if (ret != SM_OK) {
                exit(4);
            }

            uint64_t stream_cnt = 0;
            ret = smem_trans_batch_write_submit(handle, srcAddr, unique_ids[1], addrPtrs[1].data(), capacities.data(),
                                                1, (void *)&stream_cnt, 0);
            if (ret != SM_OK) {
                exit(6);
            }
            if (stream_cnt != 1) {
                exit(7);
            }

            ret = smem_trans_batch_read(handle, srcAddr, unique_ids[1], addrPtrs[1].data(), capacities.data(), 1, 0);
            if (ret != SM_OK) {
                exit(4);
            }
            stream_cnt = 0;
            ret = smem_trans_batch_read_submit(handle, srcAddr, unique_ids[1], addrPtrs[1].data(), capacities.data(),
                                               1, (void *)&stream_cnt, 0);
            if (ret != SM_OK) {
                exit(6);
            }
            if (stream_cnt != 1) {
                exit(7);
            }
        }

        smem_trans_destroy(handle, 0);
        smem_trans_uninit(0);
    };

    const std::array<const char *, 2> unique_ids = {{"127.0.0.1:5321", "127.0.0.1:5322"}};
    std::vector<std::vector<void *>> addrPtrs = {sender_addrPtrs, recv_addrPtrs};
    std::vector<smem_trans_config_t> trans_options = {sender_trans_options, recv_trans_options};

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
            smem_set_conf_store_tls(false, nullptr, 0);
            if (i == 0) {
                smem_create_config_store(STORE_URL);
            }
            func(i, rankSize, trans_options[i], addrPtrs, capacities, unique_ids);
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
        waitpid(pids[i], &status, 0);
        EXPECT_EQ(WIFEXITED(status), true);
        if (WIFEXITED(status)) {
            EXPECT_EQ(WEXITSTATUS(status), 0);
            if (WEXITSTATUS(status) != 0 && !needKillOthers) {
                needKillOthers = true;
                for (uint32_t j = 0; j < rankSize; ++j) {
                    if (i != j && pids[j] > 0) {
                        kill(pids[j], SIGKILL);
                    }
                }
            }
        } else {
            needKillOthers = true;
            for (uint32_t j = 0; j < rankSize; ++j) {
                if (i != j && pids[j] > 0) {
                    kill(pids[j], SIGKILL);
                }
            }
        }
    }
    delete[] sender_buffer;
    delete[] recv_buffer;
}

TEST_F(SmemTransTest, smem_trans_batch_write_dram)
{
    uint32_t rankSize = 2;
    size_t capacities = 0x200000; // 最少2M对齐
    smem_trans_config_t sender_trans_options = {SMEM_TRANS_SENDER, SMEM_DEFAUT_WAIT_TIME, 0, 0};
    smem_trans_config_t recv_trans_options = {SMEM_TRANS_RECEIVER, SMEM_DEFAUT_WAIT_TIME, 1, 0};

    int pipe_fd[2]; // C2 -> C1
    EXPECT_NE(pipe(pipe_fd), -1);

    auto func = [pipe_fd](uint32_t rank, uint32_t rankCount, smem_trans_config_t trans_options,
                          size_t capacities, const std::array<const char *, 2> unique_ids) {
        if (rank == 1) {
            // 确保传入的rank和内部生成的rank对应
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        trans_options.dataOpType = SMEMB_DATA_OP_SDMA;
        int ret = smem_trans_init(&trans_options);
        if (ret != 0) {
            exit(1);
        }
        auto handle = smem_trans_create(STORE_URL, unique_ids[rank], &trans_options);
        if (handle == nullptr) {
            exit(2);
        }

        // 申请内存
        auto ptr0 = smem_trans_malloc(handle, capacities);
        if (ptr0 == nullptr) {
            exit(3);
        }

        auto ptr1 = smem_trans_malloc(handle, capacities);
        if (ptr1 == nullptr) {
            exit(3);
        }

        const void *local_addrs[] = {ptr0, ptr1};
        // 接收dst addr
        if (rank == 0) {
            close(pipe_fd[1]);
            void* dst_addr1;
            void* dst_addr2;
            ssize_t n = read(pipe_fd[0], &dst_addr1, sizeof(dst_addr1));
            n = read(pipe_fd[0], &dst_addr2, sizeof(dst_addr2));
            close(pipe_fd[0]);
            void *remote_addrs[] = {dst_addr1, dst_addr2};

            std::this_thread::sleep_for(std::chrono::seconds(TRANS_TEST_WAIT_TIME));
            size_t data_sizes[] = {capacities, capacities};

            ret = smem_trans_batch_write(handle, local_addrs, unique_ids[1], remote_addrs, data_sizes, 2, 0);
            if (ret != 0) {
                exit(4);
            }
        } else {
            close(pipe_fd[0]);
            if (write(pipe_fd[1], &ptr0, sizeof(ptr0)) != sizeof(ptr0)) {
                std::cerr << "rank1 send addr0 failed" << std::endl;
            }
            if (write(pipe_fd[1], &ptr1, sizeof(ptr1)) != sizeof(ptr1)) {
                std::cerr << "rank1 send addr1 failed" << std::endl;
            }
            close(pipe_fd[1]);
        }
        if (rank == 1) {
            std::this_thread::sleep_for(std::chrono::seconds(TRANS_TEST_WAIT_TIME));
        }
        std::cerr << " will cleanup rank:" << rank << std::endl;
        smem_trans_free(handle, ptr0);
        smem_trans_free(handle, ptr1);
        smem_trans_destroy(handle, 0);
        smem_trans_uninit(0);
    };

    const std::array<const char *, 2> unique_ids = {{"127.0.0.1:5321", "127.0.0.1:5322"}};
    std::vector<smem_trans_config_t> trans_options = {sender_trans_options, recv_trans_options};

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
            smem_set_conf_store_tls(false, nullptr, 0);
            if (i == 0) {
                smem_create_config_store(STORE_URL);
            }
            func(i, rankSize, trans_options[i], capacities, unique_ids);
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
        waitpid(pids[i], &status, 0);
        EXPECT_EQ(WIFEXITED(status), true);
        if (WIFEXITED(status)) {
            EXPECT_EQ(WEXITSTATUS(status), 0);
            if (WEXITSTATUS(status) != 0 && !needKillOthers) {
                needKillOthers = true;
                for (uint32_t j = 0; j < rankSize; ++j) {
                    if (i != j && pids[j] > 0) {
                        kill(pids[j], SIGKILL);
                    }
                }
            }
        } else {
            needKillOthers = true;
            for (uint32_t j = 0; j < rankSize; ++j) {
                if (i != j && pids[j] > 0) {
                    kill(pids[j], SIGKILL);
                }
            }
        }
    }
}

TEST_F(SmemTransTest, smem_trans_batch_write_ipv6)
{
    uint32_t rankSize = 2;
    int *sender_buffer = new int[500];
    int *recv_buffer = new int[500];
    std::vector<void *> sender_addrPtrs = {sender_buffer};
    std::vector<void *> recv_addrPtrs = {recv_buffer};
    std::vector<size_t> capacities = {500 * sizeof(int)};
    smem_trans_config_t sender_trans_options = {SMEM_TRANS_SENDER, SMEM_DEFAUT_WAIT_TIME, 0, 0};
    smem_trans_config_t recv_trans_options = {SMEM_TRANS_RECEIVER, SMEM_DEFAUT_WAIT_TIME, 1, 0};

    auto func = [](uint32_t rank, uint32_t rankCount, smem_trans_config_t trans_options,
                   std::vector<std::vector<void *>> addrPtrs, std::vector<size_t> capacities,
                   const std::array<const char *, 2> unique_ids) {
        trans_options.dataOpType = SMEMB_DATA_OP_SDMA;
        int ret = smem_trans_init(&trans_options);
        if (ret != 0) {
            exit(1);
        }
        auto handle = smem_trans_create(STORE_URL_IPV6, unique_ids[rank], &trans_options);
        if (handle == nullptr) {
            exit(2);
        }

        ret = smem_trans_batch_register_mem(handle, addrPtrs[rank].data(), capacities.data(), 1, 0);
        if (ret != SM_OK) {
            exit(3);
        }

        if (rank == 0) {
            const void *srcAddr[] = {addrPtrs[0][0]};
            ret = smem_trans_batch_write(handle, srcAddr, unique_ids[1], addrPtrs[1].data(), capacities.data(), 1, 0);
            if (ret != SM_OK) {
                exit(4);
            }
        }

        smem_trans_destroy(handle, 0);
        smem_trans_uninit(0);
    };

    const std::array<const char *, 2> unique_ids = {{"[::]:5321", "[::]:5322"}};
    std::vector<std::vector<void *>> addrPtrs = {sender_addrPtrs, recv_addrPtrs};
    std::vector<smem_trans_config_t> trans_options = {sender_trans_options, recv_trans_options};

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
            smem_set_conf_store_tls(false, nullptr, 0);
            if (i == 0) {
                smem_create_config_store(STORE_URL_IPV6);
            }
            func(i, rankSize, trans_options[i], addrPtrs, capacities, unique_ids);
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
        waitpid(pids[i], &status, 0);
        EXPECT_EQ(WIFEXITED(status), true);
        if (WIFEXITED(status)) {
            EXPECT_EQ(WEXITSTATUS(status), 0);
            if (WEXITSTATUS(status) != 0 && !needKillOthers) {
                needKillOthers = true;
                for (uint32_t j = 0; j < rankSize; ++j) {
                    if (i != j && pids[j] > 0) {
                        kill(pids[j], SIGKILL);
                    }
                }
            }
        } else {
            needKillOthers = true;
            for (uint32_t j = 0; j < rankSize; ++j) {
                if (i != j && pids[j] > 0) {
                    kill(pids[j], SIGKILL);
                }
            }
        }
    }
    delete[] sender_buffer;
    delete[] recv_buffer;
}

TEST_F(SmemTransTest, smem_trans_batch_write_failed_invalid_param)
{
    pid_t pid = fork();
    EXPECT_NE(pid, -1);

    if (pid == 0) {
        uint8_t flag = 0;
        int *srcPtr1 = new int[1000];
        int *srcPtr2 = new int[2000];
        std::vector<const void *> srcAddrPtrs = {srcPtr1, srcPtr2};
        int *destPtr1 = new int[5000];
        int *destPtr2 = new int[6000];
        std::vector<void *> destAddrPtrs = {destPtr1, destPtr2};
        std::vector<size_t> dataSizes = {128U, 128U};

        // first create server
        smem_set_conf_store_tls(false, nullptr, 0);
        smem_create_config_store(STORE_URL);
        int ret = smem_trans_init(&g_trans_options);
        EXPECT_EQ(ret, 0);

        // client connect to server when initializing
        auto handle = smem_trans_create(STORE_URL, UNIQUE_ID, &g_trans_options);

        // handle = nullptr
        ret = smem_trans_batch_write(nullptr, srcAddrPtrs.data(), UNIQUE_ID, destAddrPtrs.data(), dataSizes.data(),
                                     dataSizes.size(), 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 2;
            goto cleanup;
        }
        // srcAddresses = nullptr
        ret = smem_trans_batch_write(handle, nullptr, UNIQUE_ID, destAddrPtrs.data(), dataSizes.data(),
                                     dataSizes.size(), 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 3;
            goto cleanup;
        }
        // destUniqueId = nullptr
        ret = smem_trans_batch_write(handle, srcAddrPtrs.data(), nullptr, destAddrPtrs.data(), dataSizes.data(),
                                     dataSizes.size(), 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 4;
            goto cleanup;
        }
        // destAddresses = nullptr
        ret = smem_trans_batch_write(handle, srcAddrPtrs.data(), UNIQUE_ID, nullptr, dataSizes.data(),
                                     dataSizes.size(), 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 5;
            goto cleanup;
        }
        // dataSizes = nullptr
        ret = smem_trans_batch_write(handle, srcAddrPtrs.data(), UNIQUE_ID, destAddrPtrs.data(), nullptr,
                                     dataSizes.size(), 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 6;
            goto cleanup;
        }
        // batchSize = 0
        ret = smem_trans_batch_write(handle, srcAddrPtrs.data(), UNIQUE_ID, destAddrPtrs.data(), dataSizes.data(),
                                     0, 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 7;
            goto cleanup;
        }

    cleanup:
        delete[] srcPtr1;
        delete[] srcPtr2;
        delete[] destPtr1;
        delete[] destPtr2;
        smem_trans_destroy(handle, 0);
        smem_trans_uninit(0);
        exit(flag);
    }

    int status;
    EXPECT_NE(waitpid(pid, &status, 0), -1);

    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST_F(SmemTransTest, smem_trans_batch_write_failed_invalid_param_ipv6)
{
    pid_t pid = fork();
    EXPECT_NE(pid, -1);

    if (pid == 0) {
        uint8_t flag = 0;
        int *srcPtr1 = new int[1000];
        int *srcPtr2 = new int[2000];
        std::vector<const void *> srcAddrPtrs = {srcPtr1, srcPtr2};
        int *destPtr1 = new int[5000];
        int *destPtr2 = new int[6000];
        std::vector<void *> destAddrPtrs = {destPtr1, destPtr2};
        std::vector<size_t> dataSizes = {128U, 128U};

        // first create server
        smem_set_conf_store_tls(false, nullptr, 0);
        smem_create_config_store(STORE_URL_IPV6);
        int ret = smem_trans_init(&g_trans_options);
        EXPECT_EQ(ret, 0);

        // client connect to server when initializing
        auto handle = smem_trans_create(STORE_URL_IPV6, UNIQUE_IPV6_ID, &g_trans_options);

        // handle = nullptr
        ret = smem_trans_batch_write(nullptr, srcAddrPtrs.data(), UNIQUE_IPV6_ID, destAddrPtrs.data(), dataSizes.data(),
                                     dataSizes.size(), 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 2;
            goto cleanup;
        }
        // srcAddresses = nullptr
        ret = smem_trans_batch_write(handle, nullptr, UNIQUE_IPV6_ID, destAddrPtrs.data(), dataSizes.data(),
                                     dataSizes.size(), 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 3;
            goto cleanup;
        }
        // destUniqueId = nullptr
        ret = smem_trans_batch_write(handle, srcAddrPtrs.data(), nullptr, destAddrPtrs.data(), dataSizes.data(),
                                     dataSizes.size(), 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 4;
            goto cleanup;
        }
        // destAddresses = nullptr
        ret = smem_trans_batch_write(handle, srcAddrPtrs.data(), UNIQUE_IPV6_ID, nullptr, dataSizes.data(),
                                     dataSizes.size(), 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 5;
            goto cleanup;
        }
        // dataSizes = nullptr
        ret = smem_trans_batch_write(handle, srcAddrPtrs.data(), UNIQUE_IPV6_ID, destAddrPtrs.data(), nullptr,
                                     dataSizes.size(), 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 6;
            goto cleanup;
        }
        // batchSize = 0
        ret = smem_trans_batch_write(handle, srcAddrPtrs.data(), UNIQUE_IPV6_ID, destAddrPtrs.data(), dataSizes.data(),
                                     0, 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 7;
            goto cleanup;
        }

    cleanup:
        delete[] srcPtr1;
        delete[] srcPtr2;
        delete[] destPtr1;
        delete[] destPtr2;
        smem_trans_destroy(handle, 0);
        smem_trans_uninit(0);
        exit(flag);
    }

    int status;
    EXPECT_NE(waitpid(pid, &status, 0), -1);

    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST_F(SmemTransTest, smem_trans_register_mems_success_receiver)
{
    smem_trans_config_t trans_options = g_trans_options;
    trans_options.role = SMEM_TRANS_RECEIVER;
    pid_t pid = fork();
    EXPECT_NE(pid, -1);

    if (pid == 0) {
        uint8_t flag = 0;
        int *address1 = new int[1000];
        int *address2 = new int[2000];
        std::vector<void *> addrPtrs = {address1, address2};
        std::vector<size_t> capacities = {1000 * sizeof(int), 2000 * sizeof(int)};

        // first create server
        smem_set_conf_store_tls(false, nullptr, 0);
        smem_create_config_store(STORE_URL);
        int ret = smem_trans_init(&g_trans_options);
        EXPECT_EQ(ret, 0);

        // client connect to server when initializing
        auto handle = smem_trans_create(STORE_URL, UNIQUE_ID, &trans_options);

        ret = smem_trans_batch_register_mem(handle, addrPtrs.data(), capacities.data(), capacities.size(), 0);
        if (ret != SM_OK) {
            flag = 2;
            goto cleanup;
        }

    cleanup:
        delete[] address1;
        delete[] address2;
        smem_trans_destroy(handle, 0);
        smem_trans_uninit(0);
        exit(flag);
    }

    int status;
    EXPECT_NE(waitpid(pid, &status, 0), -1);

    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST_F(SmemTransTest, smem_trans_register_mems_success_receiver_ipv6)
{
    smem_trans_config_t trans_options = g_trans_options;
    trans_options.role = SMEM_TRANS_RECEIVER;
    pid_t pid = fork();
    EXPECT_NE(pid, -1);

    if (pid == 0) {
        uint8_t flag = 0;
        int *address1 = new int[1000];
        int *address2 = new int[2000];
        std::vector<void *> addrPtrs = {address1, address2};
        std::vector<size_t> capacities = {1000 * sizeof(int), 2000 * sizeof(int)};

        // first create server
        smem_set_conf_store_tls(false, nullptr, 0);
        smem_create_config_store(STORE_URL_IPV6);
        int ret = smem_trans_init(&g_trans_options);
        EXPECT_EQ(ret, 0);

        // client connect to server when initializing
        auto handle = smem_trans_create(STORE_URL_IPV6, UNIQUE_IPV6_ID, &trans_options);

        ret = smem_trans_batch_register_mem(handle, addrPtrs.data(), capacities.data(), capacities.size(), 0);
        if (ret != SM_OK) {
            flag = 2;
            goto cleanup;
        }

    cleanup:
        delete[] address1;
        delete[] address2;
        smem_trans_destroy(handle, 0);
        smem_trans_uninit(0);
        exit(flag);
    }

    int status;
    EXPECT_NE(waitpid(pid, &status, 0), -1);

    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}