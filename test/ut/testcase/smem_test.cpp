/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <thread>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include "hybm_big_mem.h"
#include "hybm_data_op.h"
#include "acc_log.h"
#include "smem.h"
#include "smem_shm.h"
#include "smem_bm.h"
#include "ut_barrier_util.h"
#include "hybm_stub.h"
#include "smem_shm_entry_manager.h"
#define private public
#include "smem_bm_entry_manager.h"
#undef private
#include "smem_tcp_config_store.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace ock::smem;

const int32_t UT_SMEM_ID = 1;
const char UT_IP_PORT[] = "tcp://127.0.0.1:7758";
const char UT_IP_PORT2[] = "tcp://127.0.0.1:7958";
const char UT_SHM_NAME[] = "/mfhy_ut_shm_128M";
const uint32_t UT_CREATE_MEM_SIZE = 1024;
const uint32_t UT_COPY_MEM_SIZE = 128;
const uint64_t UT_SHM_SIZE = 128 * 1024 * 1024ULL;
static uint32_t g_utEntryIdx = 1;

class TestSmem : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp() override;
    void TearDown() override;
};

void TestSmem::SetUpTestCase() {}

void TestSmem::TearDownTestCase() {}

void TestSmem::SetUp()
{
    mock_hybm_api();
}

void TestSmem::TearDown()
{
    GlobalMockObject::verify();
    GlobalMockObject::reset();
}

void FinalizeUTShareMem(int shmFd)
{
    if (shmFd >= 0) {
        close(shmFd);
        shm_unlink(UT_SHM_NAME);
    }
}

bool InitUTShareMem(int &shmFd)
{
    int fd = shm_open(UT_SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        return false;
    }
    int ret = ftruncate(fd, static_cast<off_t>(UT_SHM_SIZE));
    if (ret != 0) {
        FinalizeUTShareMem(fd);
        return false;
    }
    shmFd = fd;
    return true;
}

bool CheckMem(void *base, void *ptr, uint64_t size)
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

TEST_F(TestSmem, two_card_shm_create_success)
{
    int shmFd = -1;
    auto shmCreateRet = InitUTShareMem(shmFd);
    ASSERT_EQ(shmCreateRet, true);
    smem_set_log_level(0);
    uint32_t rankSize = 2;
    std::thread ts[rankSize];
    auto func = [](uint32_t rank, uint32_t rankCount) {
        void *gva;
        int32_t ret = smem_init(0);
        if (ret != 0) {
            exit(1);
        }
        ret = smem_set_conf_store_tls(false, nullptr, 0);
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
        smem_uninit();
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
        FinalizeUTShareMem(shmFd);
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
    FinalizeUTShareMem(shmFd);
}

TEST_F(TestSmem, two_card_shm_allgather_success)
{
    int shmFd = -1;
    auto shmCreateRet = InitUTShareMem(shmFd);
    ASSERT_EQ(shmCreateRet, true);
    smem_set_log_level(0);
    uint32_t rankSize = 2;
    std::thread ts[rankSize];
    auto func = [](uint32_t rank, uint32_t rankCount) {
        setenv("SMEM_CONF_STORE_TLS_ENABLE", "0", 1);
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
        ret = smem_set_conf_store_tls(false, nullptr, 0);
        if (ret != 0) {
            exit(3);
        }
        ret = smem_shm_init(UT_IP_PORT, rankCount, rank, rank, &config);
        if (ret != 0) {
            exit(4);
        }

        auto handle = smem_shm_create(UT_SMEM_ID, rankCount, rank, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_MTE, 0, &gva);
        if (handle == nullptr) {
            exit(5);
        }
        char send[] = "test";
        uint32_t len = sizeof(send) - 1;
        char recv[rankCount * len + 1];
        ret = smem_shm_control_allgather(handle, send, len, recv, rankCount * len);
        EXPECT_EQ(ret, 0);
        char checkResult[rankCount * len + 1];
        for (int i = 0; i < rankCount; ++i) {
            std::strcat(checkResult, send);
        }
        // EXPECT_EQ(std::strcmp(recv, checkResult), 0);
        smem_shm_destroy(handle, 0);
        smem_shm_uninit(0);
        smem_uninit();
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
        FinalizeUTShareMem(shmFd);
        ASSERT_NE(needKillOthers, true);
    }

    for (uint32_t i = 0; i < rankSize; ++i) {
        int status = 0;
        if (needKillOthers) {
            kill(pids[i], SIGKILL);
        }
        waitpid(pids[i], &status, 0);
        // EXPECT_EQ(WIFEXITED(status), true);
        if (WIFEXITED(status)) {
            EXPECT_EQ(WEXITSTATUS(status), 0);
            if (WEXITSTATUS(status) != 0) {
                needKillOthers = true;
            }
        } else {
            needKillOthers = true;
        }
    }
    FinalizeUTShareMem(shmFd);
}

TEST_F(TestSmem, two_crad_bm_copy_success)
{
    int shmFd = -1;
    auto shmCreateRet = InitUTShareMem(shmFd);
    ASSERT_EQ(shmCreateRet, true);
    smem_set_log_level(0);
    uint32_t rankSize = 2;
    auto func = [](uint32_t rank, uint32_t rankCount) {
        int32_t ret = smem_init(0);
        if (ret != 0) {
            exit(1);
        }
        ret = smem_set_conf_store_tls(false, nullptr, 0);
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

        auto barrier = new (std::nothrow) UtBarrierUtil;
        if (barrier == nullptr) {
            exit(4);
        }
        ret = barrier->Init(rank, rank, rankCount, UT_IP_PORT2, UT_CREATE_MEM_SIZE);
        if (ret != 0) {
            exit(5);
        }

        auto handle = smem_bm_create(0, rankCount, SMEMB_DATA_OP_SDMA, 0, UT_CREATE_MEM_SIZE, 0);
        if (handle == nullptr) {
            exit(6);
        }

        ret = barrier->Barrier();
        if (ret != 0) {
            exit(7);
        }

        void *local = smem_bm_ptr(handle, rank);
        if (local == nullptr) {
            exit(8);
        }
        void *remote = smem_bm_ptr(handle, (rank + 1) % rankCount);
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
        smem_uninit();
        if (!cpyRet) {
            exit(15);
        }
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
        FinalizeUTShareMem(shmFd);
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
    FinalizeUTShareMem(shmFd);
}

TEST_F(TestSmem, smem_log_set_level_success)
{
    smem_init(0);
    auto ret = smem_set_log_level(0);
    EXPECT_EQ(ret, 0);
    smem_uninit();
}

TEST_F(TestSmem, smem_log_set_level_failed)
{
    auto ret = smem_set_log_level(-1);
    ASSERT_NE(ret, 0);
    ret = smem_set_log_level(5);
    ASSERT_NE(ret, 0);
}


void UtExLoggerFunc(int level, const char *msg)
{
    if (msg == nullptr) {
        return;
    }
    switch (level) {
        case 0:
            printf("UtLogger: DEBUG %s\n", msg);
            break;
        case 1:
            printf("UtLogger: INFO  %s\n", msg);
            break;
        case 2:
            printf("UtLogger: WARN  %s\n", msg);
            break;
        case 3:
            printf("UtLogger: ERROR %s\n", msg);
            break;
        default:
            printf("UtLogger: level(%d), msg(%s)\n", level, msg);
            break;
    }
}

TEST_F(TestSmem, smem_log_set_ex_logger_success)
{
    smem_init(0);
    auto ret = smem_set_extern_logger(UtExLoggerFunc);
    EXPECT_EQ(ret, 0);
    smem_uninit();
}

TEST_F(TestSmem, smem_log_set_ex_logger_failed)
{
    smem_init(0);
    auto ret = smem_set_extern_logger(nullptr);
    EXPECT_NE(ret, 0);
    smem_uninit();
}

TEST_F(TestSmem, smem_get_last_err_msg)
{
    smem_init(0);
    auto ret = smem_set_log_level(-1);
    ASSERT_NE(ret, 0);
    auto errStr = smem_get_last_err_msg();
    EXPECT_NE(errStr, nullptr);
    auto len = strlen(errStr);
    EXPECT_NE(len, 0);
    smem_uninit();
}

TEST_F(TestSmem, smem_get_and_clear_last_err_msg)
{
    smem_init(0);
    auto ret = smem_set_log_level(-1);
    ASSERT_NE(ret, 0);
    auto errStr = smem_get_and_clear_last_err_msg();
    EXPECT_NE(errStr, nullptr);
    auto len = strlen(errStr);
    EXPECT_NE(len, 0);
    auto emptyStr = smem_get_last_err_msg();
    EXPECT_NE(emptyStr, nullptr);
    len = strlen(emptyStr);
    EXPECT_EQ(len, 0);
    smem_uninit();
}

TEST_F(TestSmem, smem_shm_config_init_failed)
{
    auto ret = smem_shm_config_init(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmem, smem_shm_init_failed_invalid_params)
{
    smem_init(0);
    smem_shm_config_t config;
    smem_shm_config_init(&config);

    auto ret = smem_shm_init(nullptr, 2, 0, 0, &config);
    EXPECT_NE(ret, 0);
    ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, nullptr);
    EXPECT_NE(ret, 0);

    config.shmInitTimeout = 0;
    ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    EXPECT_NE(ret, 0);
    smem_shm_config_init(&config);
    config.shmCreateTimeout = 0;
    ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    EXPECT_NE(ret, 0);
    smem_shm_config_init(&config);
    config.controlOperationTimeout = 0;
    ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    EXPECT_NE(ret, 0);

    smem_shm_config_init(&config);
    ret = smem_shm_init(UT_IP_PORT, 65536, 0, 0, &config);
    EXPECT_NE(ret, 0);
    ret = smem_shm_init(UT_IP_PORT, 2, 2, 0, &config);
    EXPECT_NE(ret, 0);

    smem_uninit();
}

TEST_F(TestSmem, smem_shm_init_failed_manager_error)
{
    smem_init(0);
    smem_shm_config_t config;
    smem_shm_config_init(&config);

    MOCKER_CPP(&SmemShmEntryManager::Initialize, int32_t(*)(SmemShmEntryManager *, const char *,
        uint32_t, uint32_t, uint16_t, smem_shm_config_t *)).stubs().will(returnValue(-1));
    auto ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    EXPECT_NE(ret, 0);

    smem_uninit();
}

TEST_F(TestSmem, smem_shm_init_failed_hybm_error)
{
    GlobalMockObject::verify();
    smem_init(0);
    smem_shm_config_t config;
    smem_shm_config_init(&config);

    MOCKER_CPP(&SmemShmEntryManager::Initialize, int32_t(*)(SmemShmEntryManager *, const char *,
        uint32_t, uint32_t, uint16_t, smem_shm_config_t *)).stubs().will(returnValue(0));
    MOCKER(hybm_init).stubs().will(returnValue(-1));
    auto ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    EXPECT_NE(ret, 0);

    smem_uninit();
}

TEST_F(TestSmem, smem_shm_init_already_inited)
{
    smem_init(0);
    smem_shm_config_t config;
    smem_shm_config_init(&config);

    MOCKER_CPP(&SmemShmEntryManager::Initialize, int32_t(*)(SmemShmEntryManager *, const char *,
        uint32_t, uint32_t, uint16_t, smem_shm_config_t *)).stubs().will(returnValue(0));
    auto ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    ASSERT_EQ(ret, 0);
    ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    EXPECT_EQ(ret, 0);
    smem_shm_uninit(0);

    smem_uninit();
}

TEST_F(TestSmem, smem_shm_uninit_failed_not_inited_yet)
{
    smem_shm_uninit(0);
}

TEST_F(TestSmem, smem_shm_query_support_data_operation_success)
{
    auto ret = smem_shm_query_support_data_operation();
    EXPECT_EQ(ret, 1u);
}

TEST_F(TestSmem, smem_shm_create_failed_invalid_params)
{
    smem_init(0);
    smem_shm_config_t config;
    smem_shm_config_init(&config);
    void *gva;

    auto shm = smem_shm_create(0, 2, 0, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_MTE, 0, &gva);
    EXPECT_EQ(shm, nullptr);

    MOCKER_CPP(&SmemShmEntryManager::Initialize, int32_t(*)(SmemShmEntryManager *, const char *,
        uint32_t, uint32_t, uint16_t, smem_shm_config_t *)).stubs().will(returnValue(0));
    auto ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    ASSERT_EQ(ret, 0);

    shm = smem_shm_create(0, 65536, 0, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_MTE, 0, &gva);
    EXPECT_EQ(shm, nullptr);
    shm = smem_shm_create(0, 2, 2, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_MTE, 0, &gva);
    EXPECT_EQ(shm, nullptr);
    shm = smem_shm_create(0, 2, 0, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_SDMA, 0, &gva);
    EXPECT_EQ(shm, nullptr);
    shm = smem_shm_create(0, 2, 0, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_MTE, 0, nullptr);
    EXPECT_EQ(shm, nullptr);
    shm = smem_shm_create(64, 2, 0, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_MTE, 0, nullptr);
    EXPECT_EQ(shm, nullptr);

    smem_shm_uninit(0);
    smem_uninit();
}

int32_t UtCreateEntryByIdStub(SmemShmEntryManager *, uint32_t id, ock::smem::SmemShmEntryPtr &entry)
{
    auto tempEntry = SmMakeRef<SmemShmEntry>(g_utEntryIdx++);
    entry = tempEntry;
    return 0;
}

TEST_F(TestSmem, smem_shm_create_failed_manager_error)
{
    smem_init(0);
    smem_shm_config_t config;
    smem_shm_config_init(&config);
    void *gva;

    MOCKER_CPP(&SmemShmEntryManager::Initialize, int32_t(*)(SmemShmEntryManager *, const char *,
        uint32_t, uint32_t, uint16_t, smem_shm_config_t *)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemShmEntryManager::CreateEntryById, int32_t(*)(SmemShmEntryManager *,
        uint32_t, ock::smem::SmemShmEntryPtr &)).stubs().will(returnValue(-1));

    auto ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    ASSERT_EQ(ret, 0);

    auto shm = smem_shm_create(0, 2, 0, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_MTE, 0, &gva);
    EXPECT_EQ(shm, nullptr);

    smem_shm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_shm_create_failed_entry_error)
{
    smem_init(0);
    smem_shm_config_t config;
    smem_shm_config_init(&config);
    void *gva;

    MOCKER_CPP(&SmemShmEntryManager::Initialize, int32_t(*)(SmemShmEntryManager *, const char *,
        uint32_t, uint32_t, uint16_t, smem_shm_config_t *)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemShmEntryManager::CreateEntryById, int32_t(*)(SmemShmEntryManager *,
        uint32_t, ock::smem::SmemShmEntryPtr &)).stubs().will(invoke(UtCreateEntryByIdStub));
    MOCKER_CPP(&SmemShmEntry::Initialize, int32_t(*)(SmemShmEntry *, hybm_options &)).stubs().will(returnValue(-1));
    MOCKER_CPP(&SmemShmEntryManager::RemoveEntryByPtr, int32_t(*)(SmemShmEntryManager *,
        uintptr_t)).stubs().will(returnValue(0));

    auto ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    ASSERT_EQ(ret, 0);

    auto shm = smem_shm_create(0, 2, 0, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_MTE, 0, &gva);
    EXPECT_EQ(shm, nullptr);

    smem_shm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_shm_destroy_falied_invalid_params)
{
    char tmp[] = "test";
    smem_shm_t handle = reinterpret_cast<smem_shm_t>(tmp);
    auto ret = smem_shm_destroy(nullptr, 0);
    EXPECT_NE(ret, 0);
    ret = smem_shm_destroy(handle, 0);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmem, smem_shm_set_extra_context_falied_invalid_params)
{
    char tmp[] = "test";
    smem_shm_t handle = reinterpret_cast<smem_shm_t>(tmp);
    void *context = reinterpret_cast<void *>(tmp);
    auto ret = smem_shm_set_extra_context(nullptr, context, 1024);
    EXPECT_NE(ret, 0);
    ret = smem_shm_set_extra_context(handle, nullptr, 1024);
    EXPECT_NE(ret, 0);
    ret = smem_shm_set_extra_context(handle, context, 0);
    EXPECT_NE(ret, 0);
    ret = smem_shm_set_extra_context(handle, context, 65555);
    EXPECT_NE(ret, 0);
    ret = smem_shm_set_extra_context(handle, context, 1024);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmem, smem_shm_set_extra_context_falied_manager_error)
{
    smem_init(0);
    smem_shm_config_t config;
    smem_shm_config_init(&config);
    char tmp[] = "test";
    smem_shm_t handle = reinterpret_cast<smem_shm_t>(tmp);
    void *context = reinterpret_cast<void *>(tmp);
    MOCKER_CPP(&SmemShmEntryManager::Initialize, int32_t(*)(SmemShmEntryManager *, const char *,
        uint32_t, uint32_t, uint16_t, smem_shm_config_t *)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemShmEntryManager::GetEntryByPtr, int32_t(*)(SmemShmEntryManager *, uintptr_t,
        ock::smem::SmemShmEntryPtr &)).stubs().will(returnValue(-1)).then(returnValue(0));

    auto ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    ASSERT_EQ(ret, 0);
    ret = smem_shm_set_extra_context(handle, context, 1024);
    EXPECT_NE(ret, 0);
    ret = smem_shm_set_extra_context(handle, context, 1024);
    EXPECT_NE(ret, 0);

    smem_shm_uninit(0);
    smem_uninit();
}

int32_t UtGetEntryByPtrStub(SmemShmEntryManager *, uintptr_t ptr, ock::smem::SmemShmEntryPtr &entry)
{
    auto tempEntry = SmMakeRef<SmemShmEntry>(g_utEntryIdx++);
    entry = tempEntry;
    return 0;
}

TEST_F(TestSmem, smem_shm_set_extra_context_falied_entry_error)
{
    smem_init(0);
    smem_shm_config_t config;
    smem_shm_config_init(&config);
    char tmp[] = "test";
    smem_shm_t handle = reinterpret_cast<smem_shm_t>(tmp);
    void *context = reinterpret_cast<void *>(tmp);
    MOCKER_CPP(&SmemShmEntryManager::Initialize, int32_t(*)(SmemShmEntryManager *, const char *,
        uint32_t, uint32_t, uint16_t, smem_shm_config_t *)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemShmEntryManager::GetEntryByPtr, int32_t(*)(SmemShmEntryManager *, uintptr_t,
        ock::smem::SmemShmEntryPtr &)).stubs().will(invoke(UtGetEntryByPtrStub));
    MOCKER_CPP(&SmemShmEntry::SetExtraContext, int32_t(*)(SmemShmEntry *, const void *,
        uint32_t)).stubs().will(returnValue(-1));

    auto ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    ASSERT_EQ(ret, 0);
    ret = smem_shm_set_extra_context(handle, context, 1024);
    EXPECT_NE(ret, 0);

    smem_shm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_shm_get_global_rank_falied_invalid_params)
{
    char tmp[] = "test";
    smem_shm_t handle = reinterpret_cast<smem_shm_t>(tmp);
    auto ret = smem_shm_get_global_rank(nullptr);
    EXPECT_EQ(ret, UINT32_MAX);
    ret = smem_shm_get_global_rank(handle);
    EXPECT_EQ(ret, UINT32_MAX);
    ret = smem_shm_get_global_rank_size(nullptr);
    EXPECT_EQ(ret, UINT32_MAX);
    ret = smem_shm_get_global_rank_size(handle);
    EXPECT_EQ(ret, UINT32_MAX);
}

TEST_F(TestSmem, smem_shm_get_global_rank_falied_manager_error)
{
    smem_init(0);
    smem_shm_config_t config;
    smem_shm_config_init(&config);
    char tmp[] = "test";
    smem_shm_t handle = reinterpret_cast<smem_shm_t>(tmp);
    MOCKER_CPP(&SmemShmEntryManager::Initialize, int32_t(*)(SmemShmEntryManager *, const char *,
        uint32_t, uint32_t, uint16_t, smem_shm_config_t *)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemShmEntryManager::GetEntryByPtr, int32_t(*)(SmemShmEntryManager *, uintptr_t,
        ock::smem::SmemShmEntryPtr &)).stubs().will(returnValue(-1)).then(returnValue(-1)).then(returnValue(0));

    auto ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    ASSERT_EQ(ret, 0);
    auto ret2 = smem_shm_get_global_rank(handle);
    EXPECT_EQ(ret2, UINT32_MAX);
    ret2 = smem_shm_get_global_rank_size(handle);
    EXPECT_EQ(ret2, UINT32_MAX);
    ret2 = smem_shm_get_global_rank(handle);
    EXPECT_EQ(ret2, UINT32_MAX);
    ret2 = smem_shm_get_global_rank_size(handle);
    EXPECT_EQ(ret2, UINT32_MAX);

    smem_shm_uninit(0);
    smem_uninit();
}

SmemGroupEnginePtr UtGetGroupStub(SmemShmEntry *entry)
{
    auto tcpStore = SmMakeRef<TcpConfigStore>("127.0.0.1", 8761, false, 0);
    StorePtr ss = tcpStore.Get();
    SmemGroupOption option;
    option.rank = 0;
    option.rankSize = 2;
    option.dynamic = false;
    SmemGroupEnginePtr group = SmMakeRef<SmemNetGroupEngine>(ss, option);
    return group;
}

TEST_F(TestSmem, smem_shm_get_global_rank_falied_entry_error)
{
    smem_init(0);
    smem_shm_config_t config;
    smem_shm_config_init(&config);
    char tmp[] = "test";
    smem_shm_t handle = reinterpret_cast<smem_shm_t>(tmp);
    MOCKER_CPP(&SmemShmEntryManager::Initialize, int32_t(*)(SmemShmEntryManager *, const char *,
        uint32_t, uint32_t, uint16_t, smem_shm_config_t *)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemShmEntryManager::GetEntryByPtr, int32_t(*)(SmemShmEntryManager *, uintptr_t,
        ock::smem::SmemShmEntryPtr &)).stubs().will(invoke(UtGetEntryByPtrStub));

    auto ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    ASSERT_EQ(ret, 0);
    auto ret2 = smem_shm_get_global_rank(handle);
    EXPECT_EQ(ret2, UINT32_MAX);
    ret2 = smem_shm_get_global_rank_size(handle);
    EXPECT_EQ(ret2, UINT32_MAX);
    MOCKER_CPP(&SmemShmEntry::GetGroup, SmemGroupEnginePtr(*)(SmemShmEntry *)).stubs().will(invoke(UtGetGroupStub));
    ret2 = smem_shm_get_global_rank(handle);
    EXPECT_NE(ret2, UINT32_MAX);
    ret2 = smem_shm_get_global_rank_size(handle);
    EXPECT_NE(ret2, UINT32_MAX);

    smem_shm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_shm_control_barrier_falied_invalid_params)
{
    char tmp[] = "test";
    smem_shm_t handle = reinterpret_cast<smem_shm_t>(tmp);
    auto ret = smem_shm_control_barrier(nullptr);
    EXPECT_NE(ret, 0);
    ret = smem_shm_control_barrier(handle);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmem, smem_shm_control_barrier_falied_manager_error)
{
    smem_init(0);
    smem_shm_config_t config;
    smem_shm_config_init(&config);
    char tmp[] = "test";
    smem_shm_t handle = reinterpret_cast<smem_shm_t>(tmp);
    MOCKER_CPP(&SmemShmEntryManager::Initialize, int32_t(*)(SmemShmEntryManager *, const char *,
        uint32_t, uint32_t, uint16_t, smem_shm_config_t *)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemShmEntryManager::GetEntryByPtr, int32_t(*)(SmemShmEntryManager *, uintptr_t,
        ock::smem::SmemShmEntryPtr &)).stubs().will(returnValue(-1)).then(returnValue(0));

    auto ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    ASSERT_EQ(ret, 0);
    ret = smem_shm_control_barrier(handle);
    EXPECT_NE(ret, 0);
    ret = smem_shm_control_barrier(handle);
    EXPECT_NE(ret, 0);

    smem_shm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_shm_control_barrier_falied_entry_error)
{
    smem_init(0);
    smem_shm_config_t config;
    smem_shm_config_init(&config);
    char tmp[] = "test";
    smem_shm_t handle = reinterpret_cast<smem_shm_t>(tmp);
    MOCKER_CPP(&SmemShmEntryManager::Initialize, int32_t(*)(SmemShmEntryManager *, const char *,
        uint32_t, uint32_t, uint16_t, smem_shm_config_t *)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemShmEntryManager::GetEntryByPtr, int32_t(*)(SmemShmEntryManager *, uintptr_t,
        ock::smem::SmemShmEntryPtr &)).stubs().will(invoke(UtGetEntryByPtrStub));

    auto ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    ASSERT_EQ(ret, 0);
    ret = smem_shm_control_barrier(handle);
    EXPECT_NE(ret, 0);

    smem_shm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_shm_control_allgather_falied_invalid_params)
{
    char tmp[] = "test";
    smem_shm_t handle = reinterpret_cast<smem_shm_t>(tmp);
    auto ret = smem_shm_control_allgather(nullptr, tmp, 5, tmp, 5);
    EXPECT_NE(ret, 0);
    ret = smem_shm_control_allgather(handle, nullptr, 5, tmp, 5);
    EXPECT_NE(ret, 0);
    ret = smem_shm_control_allgather(handle, tmp, 5, nullptr, 5);
    EXPECT_NE(ret, 0);
    ret = smem_shm_control_allgather(handle, tmp, 0, tmp, 5);
    EXPECT_NE(ret, 0);
    ret = smem_shm_control_allgather(handle, tmp, 65555, tmp, 5);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmem, smem_shm_control_allgather_falied_manager_error)
{
    smem_init(0);
    smem_shm_config_t config;
    smem_shm_config_init(&config);
    char tmp[] = "test";
    smem_shm_t handle = reinterpret_cast<smem_shm_t>(tmp);
    MOCKER_CPP(&SmemShmEntryManager::Initialize, int32_t(*)(SmemShmEntryManager *, const char *,
        uint32_t, uint32_t, uint16_t, smem_shm_config_t *)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemShmEntryManager::GetEntryByPtr, int32_t(*)(SmemShmEntryManager *, uintptr_t,
        ock::smem::SmemShmEntryPtr &)).stubs().will(returnValue(-1)).then(returnValue(0));

    auto ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    ASSERT_EQ(ret, 0);
    ret = smem_shm_control_allgather(handle, tmp, 5, tmp, 5);
    EXPECT_NE(ret, 0);
    ret = smem_shm_control_allgather(handle, tmp, 5, tmp, 5);
    EXPECT_NE(ret, 0);

    smem_shm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_shm_control_allgather_falied_entry_error)
{
    smem_init(0);
    smem_shm_config_t config;
    smem_shm_config_init(&config);
    char tmp[] = "test";
    smem_shm_t handle = reinterpret_cast<smem_shm_t>(tmp);
    MOCKER_CPP(&SmemShmEntryManager::Initialize, int32_t(*)(SmemShmEntryManager *, const char *,
        uint32_t, uint32_t, uint16_t, smem_shm_config_t *)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemShmEntryManager::GetEntryByPtr, int32_t(*)(SmemShmEntryManager *, uintptr_t,
        ock::smem::SmemShmEntryPtr &)).stubs().will(invoke(UtGetEntryByPtrStub));

    auto ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    ASSERT_EQ(ret, 0);
    ret = smem_shm_control_allgather(handle, tmp, 5, tmp, 5);
    EXPECT_NE(ret, 0);

    smem_shm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_shm_topology_can_reach_falied_invalid_params)
{
    char tmp[] = "test";
    smem_shm_t handle = reinterpret_cast<smem_shm_t>(tmp);
    uint32_t rank = 0;
    auto ret = smem_shm_topology_can_reach(nullptr, rank, &rank);
    EXPECT_NE(ret, 0);
    ret = smem_shm_topology_can_reach(handle, rank, nullptr);
    EXPECT_NE(ret, 0);
    ret = smem_shm_topology_can_reach(handle, rank, &rank);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmem, smem_shm_topology_can_reach_falied_manager_error)
{
    smem_init(0);
    smem_shm_config_t config;
    smem_shm_config_init(&config);
    char tmp[] = "test";
    smem_shm_t handle = reinterpret_cast<smem_shm_t>(tmp);
    uint32_t rank = 0;
    MOCKER_CPP(&SmemShmEntryManager::Initialize, int32_t(*)(SmemShmEntryManager *, const char *,
        uint32_t, uint32_t, uint16_t, smem_shm_config_t *)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemShmEntryManager::GetEntryByPtr, int32_t(*)(SmemShmEntryManager *, uintptr_t,
        ock::smem::SmemShmEntryPtr &)).stubs().will(returnValue(-1)).then(returnValue(0))
        .then(invoke(UtGetEntryByPtrStub));

    auto ret = smem_shm_init(UT_IP_PORT, 2, 0, 0, &config);
    ASSERT_EQ(ret, 0);
    ret = smem_shm_topology_can_reach(handle, rank, &rank);
    EXPECT_NE(ret, 0);
    ret = smem_shm_topology_can_reach(handle, rank, &rank);
    EXPECT_NE(ret, 0);
    ret = smem_shm_topology_can_reach(handle, rank, &rank);
    EXPECT_EQ(ret, SM_NOT_STARTED);

    smem_shm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_bm_config_init_falied_invalid_params)
{
    auto ret = smem_bm_config_init(nullptr);
    ASSERT_NE(ret, 0);
}

TEST_F(TestSmem, smem_bm_init_falied_invalid_params)
{
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    auto ret = smem_bm_init(nullptr, 2, 0, &config);
    ASSERT_NE(ret, 0);
    ret = smem_bm_init(UT_IP_PORT2, 0, 0, &config);
    ASSERT_NE(ret, 0);
    ret = smem_bm_init(UT_IP_PORT2, 65536, 0, &config);
    ASSERT_NE(ret, 0);
    ret = smem_bm_init(UT_IP_PORT2, 2, 0, nullptr);
    ASSERT_NE(ret, 0);
    config.unifiedAddressSpace = false;
    ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_NE(ret, 0);
    smem_bm_config_init(&config);
    config.initTimeout = 0;
    ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_NE(ret, 0);
    smem_bm_config_init(&config);
    config.createTimeout = 0;
    ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_NE(ret, 0);
    smem_bm_config_init(&config);
    config.controlOperationTimeout = 0;
    ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_NE(ret, 0);
}

TEST_F(TestSmem, smem_bm_init_falied_manager_error)
{
    smem_init(0);
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    MOCKER_CPP(&SmemBmEntryManager::Initialize, int32_t(*)(SmemBmEntryManager *, const std::string &,
        uint32_t, uint16_t, const smem_bm_config_t &)).stubs().will(returnValue(-1));
    auto ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    EXPECT_NE(ret, 0);

    smem_uninit();
}

TEST_F(TestSmem, smem_bm_init_falied_hybm_error)
{
    GlobalMockObject::verify();
    smem_init(0);
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    MOCKER_CPP(&SmemBmEntryManager::Initialize, int32_t(*)(SmemBmEntryManager *, const std::string &,
        uint32_t, uint16_t, const smem_bm_config_t &)).stubs().will(returnValue(0));
    MOCKER(hybm_init).stubs().will(returnValue(-1));
    auto ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    EXPECT_NE(ret, 0);

    smem_uninit();
}

TEST_F(TestSmem, smem_bm_init_already_inited)
{
    smem_init(0);
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    MOCKER_CPP(&SmemBmEntryManager::Initialize, int32_t(*)(SmemBmEntryManager *, const std::string &,
        uint32_t, uint16_t, const smem_bm_config_t &)).stubs().will(returnValue(0));
    auto ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_EQ(ret, 0);
    ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    EXPECT_EQ(ret, 0);

    smem_bm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_bm_uninit_falied_not_init_yet)
{
    smem_bm_uninit(0);
}

TEST_F(TestSmem, smem_bm_get_rank_id_failed)
{
    MOCKER_CPP(&SmemBmEntryManager::GetRankId, uint32_t(*)(SmemBmEntryManager *))
        .stubs().will(returnValue(UINT32_MAX));
    auto ret = smem_bm_get_rank_id();
    EXPECT_EQ(ret, UINT32_MAX);
}

TEST_F(TestSmem, smem_bm_create_failed_invalid_params)
{
    smem_init(0);
    auto handle = smem_bm_create(0, 0, SMEMB_DATA_OP_SDMA, 0, 1024, 0);
    EXPECT_EQ(handle, nullptr);
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    MOCKER_CPP(&SmemBmEntryManager::Initialize, int32_t(*)(SmemBmEntryManager *, const std::string &,
        uint32_t, uint16_t, const smem_bm_config_t &)).stubs().will(returnValue(0));
    auto ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_EQ(ret, 0);
    handle = smem_bm_create(0, 0, SMEMB_DATA_OP_SDMA, 0, 0, 0);
    EXPECT_EQ(handle, nullptr);

    smem_bm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_bm_create_failed_manager_error)
{
    smem_init(0);
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    MOCKER_CPP(&SmemBmEntryManager::Initialize, int32_t(*)(SmemBmEntryManager *, const std::string &,
        uint32_t, uint16_t, const smem_bm_config_t &)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemBmEntryManager::CreateEntryById, int32_t(*)(SmemBmEntryManager *, uint32_t,
        ock::smem::SmemBmEntryPtr &)).stubs().will(returnValue(-1)).then(returnValue(0));
    auto ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_EQ(ret, 0);
    auto handle = smem_bm_create(0, 0, SMEMB_DATA_OP_SDMA, 0, 1024, 0);
    EXPECT_EQ(handle, nullptr);
    handle = smem_bm_create(0, 0, SMEMB_DATA_OP_SDMA, 0, 1024, 0);
    EXPECT_EQ(handle, nullptr);

    smem_bm_uninit(0);
    smem_uninit();
}

int32_t UtBmCreateEntryByIdStub(SmemBmEntryManager * mgr, uint32_t id, ock::smem::SmemBmEntryPtr &entry)
{
    SmemBmEntryOptions options;
    auto tcpStore = SmMakeRef<TcpConfigStore>("127.0.0.1", 8975, false, 0);
    StorePtr store = tcpStore.Get();
    auto tmpEntry = SmMakeRef<SmemBmEntry>(options, store);
    entry = tmpEntry;
    return 0;
}

int32_t UtBmGetEntryByIdStub(SmemBmEntryManager * mgr, uintptr_t ptr, ock::smem::SmemBmEntryPtr &entry)
{
    SmemBmEntryOptions options;
    auto tcpStore = SmMakeRef<TcpConfigStore>("127.0.0.1", 8975, false, 0);
    StorePtr store = tcpStore.Get();
    auto tmpEntry = SmMakeRef<SmemBmEntry>(options, store);
    entry = tmpEntry;
    entry->coreOptions_.singleRankVASpace = UT_SHM_SIZE;
    entry->coreOptions_.rankCount = 2;
    return 0;
}

TEST_F(TestSmem, smem_bm_create_failed_invalid_config)
{
    smem_init(0);
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    MOCKER_CPP(&SmemBmEntryManager::PrepareStore, int32_t(*)(SmemBmEntryManager *)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemBmEntryManager::CreateEntryById, int32_t(*)(SmemBmEntryManager *, uint32_t,
        ock::smem::SmemBmEntryPtr &)).stubs().will(invoke(UtBmCreateEntryByIdStub));
    auto ret = smem_bm_init(UT_IP_PORT2, 65535, 0, &config);
    ASSERT_EQ(ret, SM_INVALID_PARAM);
    auto handle = smem_bm_create(0, 0, SMEMB_DATA_OP_SDMA, 0, UINT64_MAX, 0);
    EXPECT_EQ(handle, nullptr);

    smem_bm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_bm_create_failed_entry_error)
{
    smem_init(0);
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    MOCKER_CPP(&SmemBmEntryManager::PrepareStore, int32_t(*)(SmemBmEntryManager *)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemBmEntryManager::CreateEntryById, int32_t(*)(SmemBmEntryManager *, uint32_t,
        ock::smem::SmemBmEntryPtr &)).stubs().will(invoke(UtBmCreateEntryByIdStub));
    MOCKER_CPP(&SmemBmEntry::Initialize, int32_t(*)(SmemBmEntry *, const hybm_options &))
        .stubs().will(returnValue(-1));
    MOCKER_CPP(&SmemBmEntryManager::RemoveEntryByPtr, int32_t(*)(SmemBmEntryManager *, uintptr_t ptr))
        .stubs().will(returnValue(0));
    auto ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_EQ(ret, 0);
    auto handle = smem_bm_create(0, 0, SMEMB_DATA_OP_SDMA, 0, 1024, 0);
    EXPECT_EQ(handle, nullptr);

    smem_bm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_bm_destroy_failed_invalid_params)
{
    char tmp[] = "test";
    smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
    smem_bm_destroy(nullptr);
    smem_bm_destroy(handle);
}

TEST_F(TestSmem, smem_bm_destroy_failed_manager_error)
{
    smem_init(0);
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    char tmp[] = "test";
    smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
    MOCKER_CPP(&SmemBmEntryManager::Initialize, int32_t(*)(SmemBmEntryManager *, const std::string &,
        uint32_t, uint16_t, const smem_bm_config_t &)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemBmEntryManager::RemoveEntryByPtr, int32_t(*)(SmemBmEntryManager *, uintptr_t ptr))
        .stubs().will(returnValue(-1));
    auto ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_EQ(ret, 0);

    smem_bm_destroy(handle);

    smem_bm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_bm_join_failed_invalid_params)
{
    char tmp[] = "test";
    smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
    void *gva;
    auto ret = smem_bm_join(nullptr, 0, &gva);
    EXPECT_NE(ret, 0);
    ret = smem_bm_join(handle, 0, nullptr);
    EXPECT_NE(ret, 0);
    ret = smem_bm_join(handle, 0, &gva);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmem, smem_bm_join_failed_manager_error)
{
    smem_init(0);
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    char tmp[] = "test";
    smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
    void *gva;
    MOCKER_CPP(&SmemBmEntryManager::Initialize, int32_t(*)(SmemBmEntryManager *, const std::string &,
        uint32_t, uint16_t, const smem_bm_config_t &)).stubs().will(returnValue(0));
    auto ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_EQ(ret, 0);

    ret = smem_bm_join(handle, 0, &gva);
    EXPECT_NE(ret, 0);
    ret = smem_bm_join(handle, 0, &gva);
    EXPECT_NE(ret, 0);

    smem_bm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_bm_join_failed_entry_error)
{
    smem_init(0);
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    char tmp[] = "test";
    smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
    void *gva;
    MOCKER_CPP(&SmemBmEntryManager::Initialize, int32_t(*)(SmemBmEntryManager *, const std::string &,
        uint32_t, uint16_t, const smem_bm_config_t &)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemBmEntryManager::GetEntryByPtr, int32_t(*)(SmemBmEntryManager *, uintptr_t,
        ock::smem::SmemBmEntryPtr &)).stubs().will(invoke(UtBmGetEntryByIdStub));
    MOCKER_CPP(&SmemBmEntry::Join, int32_t(*)(SmemBmEntry *, uint32_t, void **))
        .stubs().will(returnValue(-1));
    auto ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_EQ(ret, 0);

    ret = smem_bm_join(handle, 0, &gva);
    EXPECT_NE(ret, 0);

    smem_bm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_bm_leave_failed_invalid_params)
{
    char tmp[] = "test";
    smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
    auto ret = smem_bm_leave(nullptr, 0);
    EXPECT_NE(ret, 0);
    ret = smem_bm_leave(handle, 0);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmem, smem_bm_leave_failed_manager_error)
{
    smem_init(0);
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    char tmp[] = "test";
    smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
    MOCKER_CPP(&SmemBmEntryManager::Initialize, int32_t(*)(SmemBmEntryManager *, const std::string &,
        uint32_t, uint16_t, const smem_bm_config_t &)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemBmEntryManager::GetEntryByPtr, int32_t(*)(SmemBmEntryManager *, uintptr_t,
        ock::smem::SmemBmEntryPtr &)).stubs().will(returnValue(-1)).then(returnValue(0));
    auto ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_EQ(ret, 0);

    ret = smem_bm_leave(handle, 0);
    EXPECT_NE(ret, 0);
    ret = smem_bm_leave(handle, 0);
    EXPECT_NE(ret, 0);

    smem_bm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_bm_leave_failed_entry_error)
{
    smem_init(0);
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    char tmp[] = "test";
    smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
    MOCKER_CPP(&SmemBmEntryManager::Initialize, int32_t(*)(SmemBmEntryManager *, const std::string &,
        uint32_t, uint16_t, const smem_bm_config_t &)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemBmEntryManager::GetEntryByPtr, int32_t(*)(SmemBmEntryManager *, uintptr_t,
        ock::smem::SmemBmEntryPtr &)).stubs().will(invoke(UtBmGetEntryByIdStub));
    MOCKER_CPP(&SmemBmEntry::Leave, int32_t(*)(SmemBmEntry *, uint32_t)).stubs().will(returnValue(-1));
    auto ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_EQ(ret, 0);

    ret = smem_bm_leave(handle, 0);
    EXPECT_NE(ret, 0);

    smem_bm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_bm_get_local_mem_size_failed_invalid_params)
{
    char tmp[] = "test";
    smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
    auto ret = smem_bm_get_local_mem_size(nullptr);
    EXPECT_EQ(ret, 0);
    ret = smem_bm_get_local_mem_size(handle);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestSmem, smem_bm_get_local_mem_size_failed_manager_error)
{
    smem_init(0);
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    char tmp[] = "test";
    smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
    MOCKER_CPP(&SmemBmEntryManager::Initialize, int32_t(*)(SmemBmEntryManager *, const std::string &,
        uint32_t, uint16_t, const smem_bm_config_t &)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemBmEntryManager::GetEntryByPtr, int32_t(*)(SmemBmEntryManager *, uintptr_t,
        ock::smem::SmemBmEntryPtr &)).stubs().will(returnValue(-1)).then(returnValue(0));
    auto ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_EQ(ret, 0);

    ret = smem_bm_get_local_mem_size(handle);
    EXPECT_EQ(ret, 0);
    ret = smem_bm_get_local_mem_size(handle);
    EXPECT_EQ(ret, 0);

    smem_bm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_bm_get_local_mem_size_success)
{
    smem_init(0);
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    char tmp[] = "test";
    smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
    MOCKER_CPP(&SmemBmEntryManager::Initialize, int32_t(*)(SmemBmEntryManager *, const std::string &,
        uint32_t, uint16_t, const smem_bm_config_t &)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemBmEntryManager::GetEntryByPtr, int32_t(*)(SmemBmEntryManager *, uintptr_t,
        ock::smem::SmemBmEntryPtr &)).stubs().will(invoke(UtBmGetEntryByIdStub));
    auto ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_EQ(ret, 0);

    ret = smem_bm_get_local_mem_size(handle);
    EXPECT_EQ(ret, UT_SHM_SIZE);

    smem_bm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_bm_ptr_failed_invalid_params)
{
    char tmp[] = "test";
    smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
    auto ptr = smem_bm_ptr(nullptr, 0);
    EXPECT_EQ(ptr, nullptr);
    ptr = smem_bm_ptr(handle, 0);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(TestSmem, smem_bm_ptr_failed_manager_error)
{
    smem_init(0);
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    char tmp[] = "test";
    smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
    MOCKER_CPP(&SmemBmEntryManager::Initialize, int32_t(*)(SmemBmEntryManager *, const std::string &,
        uint32_t, uint16_t, const smem_bm_config_t &)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemBmEntryManager::GetEntryByPtr, int32_t(*)(SmemBmEntryManager *, uintptr_t,
        ock::smem::SmemBmEntryPtr &)).stubs().will(returnValue(-1)).then(returnValue(0));
    auto ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_EQ(ret, 0);

    auto ptr = smem_bm_ptr(handle, 0);
    EXPECT_EQ(ptr, nullptr);
    ptr = smem_bm_ptr(handle, 0);
    EXPECT_EQ(ptr, nullptr);

    smem_bm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_bm_ptr_failed_invalid_peer_rankId)
{
    smem_init(0);
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    char tmp[] = "test";
    smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
    MOCKER_CPP(&SmemBmEntryManager::Initialize, int32_t(*)(SmemBmEntryManager *, const std::string &,
        uint32_t, uint16_t, const smem_bm_config_t &)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemBmEntryManager::GetEntryByPtr, int32_t(*)(SmemBmEntryManager *, uintptr_t,
        ock::smem::SmemBmEntryPtr &)).stubs().will(invoke(UtBmGetEntryByIdStub));
    auto ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_EQ(ret, 0);

    auto ptr = smem_bm_ptr(handle, 3);
    EXPECT_EQ(ptr, nullptr);

    smem_bm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_bm_copy_failed_invalid_params)
{
    char tmp[] = "test";
    smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
    smem_copy_params params = {nullptr, nullptr, 0};
    auto ret = smem_bm_copy(nullptr, &params, SMEMB_COPY_L2G, 0);
    EXPECT_NE(ret, 0);
    ret = smem_bm_copy(handle, &params, SMEMB_COPY_L2G, 0);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmem, smem_bm_copy_failed_manager_error)
{
    smem_init(0);
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    char tmp[] = "test";
    smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
    MOCKER_CPP(&SmemBmEntryManager::Initialize, int32_t(*)(SmemBmEntryManager *, const std::string &,
        uint32_t, uint16_t, const smem_bm_config_t &)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemBmEntryManager::GetEntryByPtr, int32_t(*)(SmemBmEntryManager *, uintptr_t,
        ock::smem::SmemBmEntryPtr &)).stubs().will(returnValue(-1)).then(returnValue(0));
    auto ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_EQ(ret, 0);

    smem_copy_params params = {nullptr, nullptr, 0};
    ret = smem_bm_copy(handle, &params, SMEMB_COPY_L2G, 0);
    EXPECT_NE(ret, 0);
    ret = smem_bm_copy(handle, &params, SMEMB_COPY_L2G, 0);
    EXPECT_NE(ret, 0);

    smem_bm_uninit(0);
    smem_uninit();
}

TEST_F(TestSmem, smem_bm_copy_2d_failed_invalid_params)
{
    char tmp[] = "test";
    smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
    smem_copy_2d_params copyParams = {nullptr, 0, nullptr, 0, 0, 0};
    auto ret = smem_bm_copy_2d(nullptr, &copyParams, SMEMB_COPY_L2G, 0);
    EXPECT_NE(ret, 0);
    ret = smem_bm_copy_2d(handle, &copyParams, SMEMB_COPY_L2G, 0);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmem, smem_bm_copy_2d_failed_manager_error)
{
    smem_init(0);
    smem_bm_config_t config;
    smem_bm_config_init(&config);
    char tmp[] = "test";
    smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
    MOCKER_CPP(&SmemBmEntryManager::Initialize, int32_t(*)(SmemBmEntryManager *, const std::string &,
        uint32_t, uint16_t, const smem_bm_config_t &)).stubs().will(returnValue(0));
    MOCKER_CPP(&SmemBmEntryManager::GetEntryByPtr, int32_t(*)(SmemBmEntryManager *, uintptr_t,
        ock::smem::SmemBmEntryPtr &)).stubs().will(returnValue(-1)).then(returnValue(0));
    auto ret = smem_bm_init(UT_IP_PORT2, 2, 0, &config);
    ASSERT_EQ(ret, 0);

    smem_copy_2d_params copyParams = {nullptr, 0, nullptr, 0, 0, 0};
    ret = smem_bm_copy_2d(handle, &copyParams, SMEMB_COPY_L2G, 0);
    EXPECT_NE(ret, 0);
    ret = smem_bm_copy_2d(handle, &copyParams, SMEMB_COPY_L2G, 0);
    EXPECT_NE(ret, 0);

    smem_bm_uninit(0);
    smem_uninit();
}