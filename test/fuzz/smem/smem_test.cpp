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
#include "hybm_stub.h"
#include "hybm_device_mem_segment.h"
#include "smem_shm_entry_manager.h"
#include "smem_trans_def.h"
#include "smem_trans.h"
#include "smem_store_factory.h"
#define private public
#include "smem_bm_entry_manager.h"
#undef private
#include "smem_tcp_config_store.h"
#include "../ut/testcase/ut_barrier_util.h"
#include "secodefuzz/secodeFuzz.h"
#include "dt_fuzz.h"

using namespace ock::smem;
using namespace ock::mf;

const int32_t UT_SMEM_ID = 1;
const char STORE_URL[] = "tcp://127.0.0.1:5432";
const char UT_IP_PORT[] = "tcp://127.0.0.1:7758";
const char UT_IP_PORT2[] = "tcp://127.0.0.1:7958";
const char UT_SHM_NAME[] = "/mfhy_ut_shm_128M";
const uint32_t UT_CREATE_MEM_SIZE = 1024;
const uint32_t UT_COPY_MEM_SIZE = 128;
const uint64_t UT_SHM_SIZE = 128 * 1024 * 1024ULL;
static uint32_t g_utEntryIdx = 1;

namespace {
extern int32_t perf_test_ut(int32_t rankId, int32_t rankSize);
char tmp[] = "test";
int smem_decrypt_handler_stub(const char *cipherText, size_t cipherTextLen, char *plainText, size_t &plainTextLen)
{
    return 0;
}
}  // namespace

class TestSmem : public testing::Test {
public:
    void SetUp();
    void TearDown();
};

void TestSmem::SetUp()
{
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(DT_RUNNING_TIME);
}

void TestSmem::TearDown()
{
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

void ExitExample(int status)
{
    return;
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
    char fuzzName[] = "test_two_card_shm_create_success";
    uint64_t seed = 0;
    mock_hybm_api();
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        MFFUzzCountNumber(fuzzName);
        int shmFd = -1;
        auto shmCreateRet = InitUTShareMem(shmFd);
        ASSERT_EQ(shmCreateRet, true);
        smem_set_log_level(3);
        uint32_t rankSize = 2;
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

            ret = smem_shm_init(UT_IP_PORT, rankCount, rank, rank, &config);
            EXPECT_EQ(ret, 0);
            ret = smem_shm_query_support_data_operation();
            EXPECT_EQ(ret, 1u);

            auto handle = smem_shm_create(UT_SMEM_ID, rankCount, rank, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_MTE, 0, &gva);
            if (handle == nullptr) {
                exit(4);
            }
            smem_shm_register_exit(handle, &ExitExample);
            void *context = reinterpret_cast<void *>(tmp);
            MOCKER_CPP(&SmemShmEntry::SetExtraContext, int32_t(*)(SmemShmEntry *, const void *,
                                                        uint32_t)).stubs().will(returnValue(0));
            ret = smem_shm_set_extra_context(handle, context, 1024);
            ASSERT_EQ(ret, 0);

            auto result = smem_shm_get_global_rank_size(handle);
            EXPECT_EQ(result, rankCount);

            result = smem_shm_get_global_rank(handle);
            EXPECT_EQ(result, rank);

            ret = smem_shm_control_barrier(handle);
            EXPECT_EQ(ret, 0);

            char recv[rankCount * 5 + 1];
            ret = smem_shm_control_allgather(handle, tmp, 5, recv, rankCount * 5);
            EXPECT_EQ(ret, 0);

            smem_shm_destroy(handle, 0);
            smem_shm_uninit(0);
            smem_uninit();
        };

        pid_t pids[rankSize];
        uint32_t maxProcess = rankSize;
        bool needKillOthers = false;
        for (uint32_t i = 0; i < rankSize; ++i) {
            int32_t tmpPid = fork();
            if (tmpPid < 0) {
                maxProcess = i;
                needKillOthers = true;
                break;
            }
            pids[i] = tmpPid;
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
        } else {
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
    }
    DT_FUZZ_END()
}

TEST_F(TestSmem, smem_init_copy)
{
    char fuzzName[] = "test_smem_init_copy";
    uint64_t seed = 0;
    mock_hybm_api();
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        MFFUzzCountNumber(fuzzName);
        int shmFd = -1;
        auto shmCreateRet = InitUTShareMem(shmFd);
        ASSERT_EQ(shmCreateRet, true);
        smem_set_log_level(3);
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
            // smem_bm_get_rank_id
            auto rankId = smem_bm_get_rank_id();
            if (rankId != config.rankId) {
                exit(16);
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
            auto memSize = smem_bm_get_local_mem_size(handle);
            if (memSize != UT_CREATE_MEM_SIZE) {
                exit(17);
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
            int32_t tmpPid = fork();
            if (tmpPid < 0) {
                maxProcess = i;
                needKillOthers = true;
                break;
            }
            pids[i] = tmpPid;
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
        } else {
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
    }
    DT_FUZZ_END()
}

void UtExLoggerFunc(int level, const char *msg)
{
    if (msg == nullptr) {
        return;
    }
    switch (level) {
        case 0: printf("UtLogger: DEBUG %s\n", msg); break;
        case 1: printf("UtLogger: INFO  %s\n", msg); break;
        case 2: printf("UtLogger: WARN  %s\n", msg); break;
        case 3: printf("UtLogger: ERROR %s\n", msg); break;
        default: printf("UtLogger: level(%d), msg(%s)\n", level, msg); break;
    }
}

TEST_F(TestSmem, smem_log_set_ex_logger)
{
    char fuzzName[] = "smem_log_set_ex_logger";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        MFFUzzCountNumber(fuzzName);
        int32_t logLevel = *(int32_t *)DT_SetGetS32(&g_Element[0], 0);
        smem_init(0);
        auto ret = smem_set_extern_logger(UtExLoggerFunc);
        EXPECT_EQ(ret, 0);
        ret = smem_set_log_level(logLevel);
        if (logLevel >= 0 && logLevel < 5) {
            EXPECT_EQ(ret, 0);
        } else {
            EXPECT_NE(ret, 0);
        }
        ret = smem_set_log_level(-1);
        ASSERT_NE(ret, 0);
        auto errStr = smem_get_last_err_msg();
        EXPECT_NE(errStr, nullptr);
        auto len = strlen(errStr);
        EXPECT_NE(len, 0);
        ret = smem_set_extern_logger(nullptr);
        EXPECT_NE(ret, 0);
        smem_uninit();
    }
    DT_FUZZ_END()
}

TEST_F(TestSmem, smem_shm_topology_can_reach_falied_invalid_params)
{
    char fuzzName[] = "smem_shm_topology_can_reach_falied_invalid_params";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        MFFUzzCountNumber(fuzzName);
        int32_t rankTest = *(int32_t *)DT_SetGetS32(&g_Element[0], 0);
        int32_t ptrTest = *(int32_t *)DT_SetGetS32(&g_Element[1], 0);
        char tmp[] = "test";
        smem_shm_t handle = reinterpret_cast<smem_shm_t>(tmp);
        uint32_t rank = 0;
        auto ret = smem_shm_topology_can_reach(nullptr, rank, &rank);
        EXPECT_NE(ret, 0);
        ret = smem_shm_topology_can_reach(handle, rank, nullptr);
        EXPECT_NE(ret, 0);
        ret = smem_shm_topology_can_reach(handle, rank, &rank);
        EXPECT_NE(ret, 0);
        smem_shm_topology_can_reach(handle, rankTest, &ptrTest);
    }
    DT_FUZZ_END()
}

TEST_F(TestSmem, smem_bm_get_rank_id_failed)
{
    char fuzzName[] = "smem_bm_get_rank_id_failed";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        MFFUzzCountNumber(fuzzName);
        MOCKER_CPP(&SmemBmEntryManager::GetRankId, uint32_t(*)(SmemBmEntryManager *))
            .stubs()
            .will(returnValue(UINT32_MAX));
        auto ret = smem_bm_get_rank_id();
        EXPECT_EQ(ret, UINT32_MAX);
    }
    DT_FUZZ_END()
}

TEST_F(TestSmem, smem_bm_join_failed_invalid_params)
{
    char fuzzName[] = "smem_bm_join_failed_invalid_params";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        MFFUzzCountNumber(fuzzName);
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
    DT_FUZZ_END()
}

TEST_F(TestSmem, smem_bm_leave_failed_invalid_params)
{
    char fuzzName[] = "smem_bm_leave_failed_invalid_params";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        MFFUzzCountNumber(fuzzName);
        char tmp[] = "test";
        smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
        auto ret = smem_bm_leave(nullptr, 0);
        EXPECT_NE(ret, 0);
        ret = smem_bm_leave(handle, 0);
        EXPECT_NE(ret, 0);
    }
    DT_FUZZ_END()
}

TEST_F(TestSmem, smem_bm_get_local_mem_size_failed_invalid_params)
{
    char fuzzName[] = "smem_bm_leave_failed_entry_error";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        MFFUzzCountNumber(fuzzName);
        char tmp[] = "test";
        smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
        auto ret = smem_bm_get_local_mem_size(nullptr);
        EXPECT_EQ(ret, 0);
        ret = smem_bm_get_local_mem_size(handle);
        EXPECT_EQ(ret, 0);
    }
    DT_FUZZ_END()
}

TEST_F(TestSmem, smem_trans_write)
{
    char fuzzName[] = "smem_trans_write";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        MFFUzzCountNumber(fuzzName);
        MOCKER(MemSegmentDevice::SetDeviceInfo).stubs().will(returnValue(0));

        uint32_t rankSize = 2;
        int *sender_buffer = new int[500];
        int *recv_buffer = new int[500];
        size_t capacities = 500 * sizeof(int);
        smem_trans_config_t sender_trans_options = {SMEM_TRANS_SENDER, SMEM_DEFAUT_WAIT_TIME, 0, 0};
        smem_trans_config_t recv_trans_options = {SMEM_TRANS_RECEIVER, SMEM_DEFAUT_WAIT_TIME, 1, 0};

        auto func = [](uint32_t rank, uint32_t rankCount, smem_trans_config_t trans_options,
                       std::vector<int *> addrPtrs, size_t capacities, const std::array<const char *, 2> unique_ids) {
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

            std::this_thread::sleep_for(std::chrono::seconds(8));
            if (rank == 0) {
                ret = smem_trans_write(handle, addrPtrs[0], unique_ids[1], addrPtrs[1], capacities);
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
    DT_FUZZ_END()
}

TEST_F(TestSmem, smem_trans_batch_write)
{
    char fuzzName[] = "smem_trans_batch_write";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        MFFUzzCountNumber(fuzzName);
        MOCKER(MemSegmentDevice::SetDeviceInfo).stubs().will(returnValue(0));

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

            std::this_thread::sleep_for(std::chrono::seconds(8));
            if (rank == 0) {
                const void *srcAddr[] = {addrPtrs[0][0]};
                ret = smem_trans_batch_write(handle, srcAddr, unique_ids[1], addrPtrs[1].data(), capacities.data(), 1);
                if (ret != SM_OK) {
                    exit(4);
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
    DT_FUZZ_END()
}

TEST_F(TestSmem, smem_set_config_store_tls_key)
{
    char fuzzName[] = "smem_set_config_store_tls_key";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        MFFUzzCountNumber(fuzzName);
        std::string pk_stub = "private key";
        std::string pk_pwd_stb = "private key password stub";
        int32_t ret = smem_set_config_store_tls_key(pk_stub.c_str(), pk_stub.size(), pk_pwd_stb.c_str(),
                                                    pk_pwd_stb.size(), smem_decrypt_handler_stub);
        StoreFactory::ShutDownCleanupThread();
    }
    DT_FUZZ_END()
}

TEST_F(TestSmem, smem_get_and_clear_last_err_msg)
{
    char fuzzName[] = "smem_get_and_clear_last_err_msg";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        MFFUzzCountNumber(fuzzName);
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
    DT_FUZZ_END()
}

TEST_F(TestSmem, smem_bm_copy_2d_failed_invalid_params)
{
    char fuzzName[] = "smem_bm_copy_2d_failed_invalid_params";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        MFFUzzCountNumber(fuzzName);
        char tmp[] = "test";
        smem_bm_t handle = reinterpret_cast<smem_bm_t>(tmp);
        smem_copy_2d_params copyParams = {nullptr, 0, nullptr, 0, 0, 0};
        auto ret = smem_bm_copy_2d(nullptr, &copyParams, SMEMB_COPY_L2G, 0);
        EXPECT_NE(ret, 0);
        ret = smem_bm_copy_2d(handle, &copyParams, SMEMB_COPY_L2G, 0);
        EXPECT_NE(ret, 0);
    }
    DT_FUZZ_END()
}

TEST_F(TestSmem, smem_trans_config_init)
{
    char fuzzName[] = "smem_trans_config_init";
    uint64_t seed = 0;
    DT_FUZZ_START(seed, CAPI_FUZZ_COUNT, fuzzName, 0)
    {
        MFFUzzCountNumber(fuzzName);
        int32_t initTimeout = *(int32_t *)DT_SetGetS32(&g_Element[0], 0);
        int32_t deviceId = *(int32_t *)DT_SetGetS32(&g_Element[1], 0);
        int32_t flags = *(int32_t *)DT_SetGetS32(&g_Element[2], 0);
        smem_trans_config_t config;
        config.initTimeout = initTimeout;
        config.deviceId = deviceId;
        config.flags = flags;
        smem_trans_config_init(&config);
    }
    DT_FUZZ_END()
}
