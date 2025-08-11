/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gtest/gtest.h>
#include <array>
#include <thread>
#include <chrono>
#include "smem_types.h"
#include "smem_trans.h"
#include "dl_acl_api.h"
#include "dl_api.h"

using namespace ock::smem;
using namespace ock::mf;

const int32_t UT_SMEM_ID = 1;
const char STORE_URL[] = "tcp://127.0.0.1:5432";
const char SESSION_ID[] = "127.0.0.1:5321";
const smem_trans_config_t g_trans_options = {SMEM_TRANS_SENDER, SMEM_DEFAUT_WAIT_TIME, 0, 0};

class SmemTransTest : public testing::Test {
protected:
    static void SetUpTestSuite()
    {
        auto path = std::getenv("ASCEND_HOME_PATH");
        EXPECT_NE(path, nullptr);
        auto libPath = std::string(path).append("/lib64");
        EXPECT_EQ(DlApi::LoadLibrary(libPath), BM_OK);
        smem_set_log_level(0);
    }
    static void TearDownTestSuite()
    {
        DlApi::CleanupLibrary();
    }
    void SetUp() override {}
    void TearDown() override
    {}

};

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
        setenv("SMEM_CONF_STORE_TLS_ENABLE", "0", 1);

        int ret = smem_create_config_store(STORE_URL);
        if (ret != 0) {
            exit(1);
        }

        auto handle = smem_trans_create(STORE_URL, SESSION_ID, &g_trans_options);
        if (handle == nullptr) {
            exit(2);
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
    // storeUrl == nullptr
    EXPECT_EQ(smem_trans_create(nullptr, SESSION_ID, &g_trans_options), nullptr);
    // sessionId == nullptr
    EXPECT_EQ(smem_trans_create(STORE_URL, nullptr, &g_trans_options), nullptr);
    // config == nullptr
    EXPECT_EQ(smem_trans_create(STORE_URL, SESSION_ID, nullptr), nullptr);

    // storeUrl or sessionId is empty
    const char STORE_URL_TEST1[] = "";
    const char SESSION_ID_TEST[] = "";
    EXPECT_EQ(smem_trans_create(STORE_URL_TEST1, SESSION_ID, &g_trans_options), nullptr);
    EXPECT_EQ(smem_trans_create(STORE_URL, SESSION_ID_TEST, &g_trans_options), nullptr);
}

TEST_F(SmemTransTest, smem_trans_register_mem_failed_invalid_param)
{
    pid_t pid = fork();
    EXPECT_NE(pid, -1);

    if (pid == 0) {
        uint8_t flag = 0;
        int* address = new int[10];
        size_t size = 10 * sizeof(int);

        setenv("SMEM_CONF_STORE_TLS_ENABLE", "0", 1);
        // first create server
        smem_create_config_store(STORE_URL);
        // client connect to server when initializing
        auto handle = smem_trans_create(STORE_URL, SESSION_ID, &g_trans_options);

        // handle = nullptr
        auto ret = smem_trans_register_mem(nullptr, address, size, 0);
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

TEST_F(SmemTransTest, smem_trans_sender_receiver_register_mems)
{
    setenv("SMEM_CONF_STORE_TLS_ENABLE", "0", 1);

    uint32_t rankSize = 2;
    int* sender_buffer = new int[500];
    int* recv_buffer = new int[500];
    std::vector<void*> sender_addrPtrs = {sender_buffer};
    std::vector<void*> recv_addrPtrs = {recv_buffer};
    std::vector<size_t> capacities = {500 * sizeof(int)};
    smem_trans_config_t sender_trans_options = {SMEM_TRANS_SENDER, SMEM_DEFAUT_WAIT_TIME, 0, 0};
    smem_trans_config_t recv_trans_options = {SMEM_TRANS_RECEIVER, SMEM_DEFAUT_WAIT_TIME, 1, 0};

    auto func = [](uint32_t rank, uint32_t rankCount, smem_trans_config_t trans_options,
        std::vector<void*> addrPtrs, std::vector<size_t> capacities, const char* session_id) {
        auto handle = smem_trans_create(STORE_URL, session_id, &trans_options);
        if (handle == nullptr) {
            exit(1);
        }

        auto ret = smem_trans_register_mems(handle, addrPtrs.data(), capacities.data(), 1, 0);
        if (ret != SM_OK) {
            exit(2);
        }

        std::this_thread::sleep_for(std::chrono::seconds(3));

        smem_trans_destroy(handle, 0);
        smem_trans_uninit(0);
    };

    const std::array<const char*, 2> session_ids = {{
        "127.0.0.1:5321",
        "127.0.0.1:5322"
    }};
    std::vector<std::vector<void*>> addrPtrs = {sender_addrPtrs, recv_addrPtrs};
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
            if (i == 0) {
                smem_create_config_store(STORE_URL);
            }
            func(i, rankSize, trans_options[i], addrPtrs[i], capacities, session_ids[i]);
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
        int* srcPtr1 = new int[1000];
        int* srcPtr2 = new int[2000];
        std::vector<const void*> srcAddrPtrs = {srcPtr1, srcPtr2};
        int* destPtr1 = new int[5000];
        int* destPtr2 = new int[6000];
        std::vector<void*> destAddrPtrs = {destPtr1, destPtr2};
        std::vector<size_t> dataSizes = {128U, 128U};

        setenv("SMEM_CONF_STORE_TLS_ENABLE", "0", 1);
        // first create server
        smem_create_config_store(STORE_URL);
        // client connect to server when initializing
        auto handle = smem_trans_create(STORE_URL, SESSION_ID, &g_trans_options);

        // handle = nullptr
        auto ret = smem_trans_batch_write(nullptr, srcAddrPtrs.data(), SESSION_ID, destAddrPtrs.data(),
            dataSizes.data(), 2);
        if (ret != SM_INVALID_PARAM) {
            flag = 1;
            goto cleanup;
        }
        // srcAddresses = nullptr
        ret = smem_trans_batch_write(handle, nullptr, SESSION_ID, destAddrPtrs.data(), dataSizes.data(), 2);
        if (ret != SM_INVALID_PARAM) {
            flag = 2;
            goto cleanup;
        }
        // destSession = nullptr
        ret = smem_trans_batch_write(handle, srcAddrPtrs.data(), nullptr, destAddrPtrs.data(), dataSizes.data(), 2);
        if (ret != SM_INVALID_PARAM) {
            flag = 3;
            goto cleanup;
        }
        // destAddresses = nullptr
        ret = smem_trans_batch_write(handle, srcAddrPtrs.data(), SESSION_ID, nullptr, dataSizes.data(), 2);
        if (ret != SM_INVALID_PARAM) {
            flag = 4;
            goto cleanup;
        }
        // dataSizes = nullptr
        ret = smem_trans_batch_write(handle, srcAddrPtrs.data(), SESSION_ID, destAddrPtrs.data(), nullptr, 2);
        if (ret != SM_INVALID_PARAM) {
            flag = 5;
            goto cleanup;
        }
        // batchSize = 0
        ret = smem_trans_batch_write(handle, srcAddrPtrs.data(), SESSION_ID, destAddrPtrs.data(), dataSizes.data(), 0);
        if (ret != SM_INVALID_PARAM) {
            flag = 6;
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
        int* address1 = new int[1000];
        int* address2 = new int[2000];
        std::vector<void*> addrPtrs = {address1, address2};
        std::vector<size_t> capacities = {1000 * sizeof(int), 2000 * sizeof(int)};

        setenv("SMEM_CONF_STORE_TLS_ENABLE", "0", 1);
        // first create server
        smem_create_config_store(STORE_URL);
        // client connect to server when initializing
        auto handle = smem_trans_create(STORE_URL, SESSION_ID, &trans_options);

        auto ret = smem_trans_register_mems(handle, addrPtrs.data(), capacities.data(), 2, 0);
        if (ret != SM_OK) {
            flag = 1;
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