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
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include <gtest/gtest.h>
#include "smem.h"
#include "smem_shm.h"
#include "smem_bm.h"
#include "smem_types.h"
#include "ut_barrier_util.h"
#include "hybm.h"

#include "smem_tcp_config_store.h"
#include "smem_net_group_engine.h"

#define private public
#include "smem_bm_entry.h"
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

class SmemBmTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp() override;
    void TearDown() override;
};

void SmemBmTest::SetUpTestCase() {}

void SmemBmTest::TearDownTestCase() {}

void SmemBmTest::SetUp()
{
    GlobalMockObject::reset();
    auto ret = smem_init(0);
    EXPECT_EQ(ret, 0);
}

void SmemBmTest::TearDown()
{
    GlobalMockObject::verify();
    GlobalMockObject::reset();
    smem_uninit();
}

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

void GenerateData(void *ptr, int32_t rank, uint32_t len = COPY_SIZE)
{
    if (ptr == nullptr) {
        return;
    }
    int32_t *arr = (int32_t *)ptr;
    static int32_t mod = INT16_MAX;
    int32_t base = rank;
    for (uint32_t i = 0; i < len / sizeof(int); i++) {
        base = (base * RANDOM_MULTIPLIER + RANDOM_INCREMENT) % mod;
        if ((i + rank) % NEGATIVE_RATIO_DIVISOR == 0) {
            arr[i] = -base + i + 1; // 构造三分之一的负数
        } else {
            arr[i] = base + i + 1;
        }
    }
}

TEST_F(SmemBmTest, smem_bm_init_success)
{
    std::string ipPort = "tcp://192.168.100.101:8570";
    uint32_t rankId = 0;
    uint32_t rkSize = 2;
    uint32_t deviceId = 0;
    auto ret = smem_init(0);
    EXPECT_EQ(ret, 0);

    smem_set_log_level(1);
    smem_bm_config_t config;
    (void)smem_bm_config_init(&config);
    std::string url = "tcp://192.168.100.101/24:10005"; // tcp://192.168.100.100:8570
    config.autoRanking = false;
    config.rankId = rankId;
    config.startConfigStoreServer = true;

    MOCKER_CPP(&TcpConfigStore::Startup, int32_t (*)(const smem_tls_config &, int)).stubs().will(returnValue(0));
    ret = smem_bm_init(ipPort.c_str(), rkSize, deviceId, &config);
    EXPECT_EQ(ret, 0);
}

TEST_F(SmemBmTest, smem_bm_create_success)
{
    smem_bm_data_op_type optype = SMEMB_DATA_OP_HOST_URMA;
    MOCKER_CPP(&SmemBmEntry::Initialize, int32_t (*)(const hybm_options &)).stubs().will(returnValue(0));
    smem_bm_t handle = smem_bm_create(0, 0, optype, GVA_SIZE, 0, 0);
    EXPECT_NE(handle, nullptr);

    smem_bm_destroy(handle);
}

TEST_F(SmemBmTest, smem_bm_create_failed)
{
    smem_bm_data_op_type optype = SMEMB_DATA_OP_HOST_URMA;
    smem_bm_t handle = smem_bm_create(0, 0, optype, GVA_SIZE, 0, 0);
    EXPECT_EQ(handle, nullptr);
    smem_bm_destroy(handle);
}

TEST_F(SmemBmTest, smem_bm_join_failed)
{
    smem_bm_data_op_type optype = SMEMB_DATA_OP_HOST_URMA;
    MOCKER_CPP(&SmemBmEntry::Initialize, int32_t (*)(const hybm_options &)).stubs().will(returnValue(0));
    smem_bm_t handle = smem_bm_create(1, 0, optype, GVA_SIZE, 0, 0); // 1
    auto ret = smem_bm_join(handle, 0);
    EXPECT_NE(ret, 0);
    smem_bm_destroy(handle);
}

TEST_F(SmemBmTest, smem_bm_join_success)
{
    smem_bm_data_op_type optype = SMEMB_DATA_OP_HOST_URMA;
    MOCKER_CPP(&SmemBmEntry::Initialize, int32_t (*)(const hybm_options &)).stubs().will(returnValue(0));
    smem_bm_t handle = smem_bm_create(2, 0, optype, GVA_SIZE, 0, 0); // 2

    MOCKER_CPP(&SmemBmEntry::Join, int32_t (*)(uint32_t)).stubs().will(returnValue(0));
    auto ret = smem_bm_join(handle, 0);
    EXPECT_EQ(ret, 0);

    MOCKER_CPP(&SmemBmEntry::Leave, int32_t (*)(uint32_t)).stubs().will(returnValue(0));
    ret = smem_bm_leave(handle, 0);
    EXPECT_EQ(ret, 0);

    smem_bm_destroy(handle);
}
TEST_F(SmemBmTest, smem_bm_ptr_by_mem_type_failed)
{
    std::string ipPort = "tcp://192.168.100.101:8570";
    uint32_t rankId = 0;
    uint32_t rkSize = 2;
    uint32_t deviceId = 0;

    smem_set_log_level(1);
    smem_bm_config_t config;
    (void)smem_bm_config_init(&config);
    std::string url = "tcp://192.168.100.101/24:10005"; // tcp://192.168.100.100:8570
    config.autoRanking = false;
    config.rankId = rankId;
    config.startConfigStoreServer = true;

    MOCKER_CPP(&TcpConfigStore::Startup, int32_t (*)(const smem_tls_config &, int)).stubs().will(returnValue(0));
    auto ret = smem_bm_init(ipPort.c_str(), rkSize, deviceId, &config);
    EXPECT_EQ(ret, 0);

    smem_bm_data_op_type optype = SMEMB_DATA_OP_HOST_URMA;
    MOCKER_CPP(&SmemBmEntry::Initialize, int32_t (*)(const hybm_options &)).stubs().will(returnValue(0));
    smem_bm_t handle = smem_bm_create(3, 0, optype, GVA_SIZE, 0, 0); // 3
    EXPECT_NE(handle, nullptr);

    void *host = smem_bm_ptr_by_mem_type(handle, SMEM_MEM_TYPE_HOST, rankId % rkSize);
    EXPECT_EQ(host, nullptr);
    smem_bm_destroy(handle);
}

TEST_F(SmemBmTest, smem_batch_copy_success)
{
    uint32_t rankId = 0;
    uint32_t rkSize = 2;
    uint32_t deviceId = 0;
    std::string ipPort = "tcp://192.168.100.101:8570";

    smem_bm_data_op_type optype = SMEMB_DATA_OP_HOST_URMA;
    MOCKER_CPP(&SmemBmEntry::Initialize, int32_t (*)(const hybm_options &)).stubs().will(returnValue(0));
    smem_bm_t handle = smem_bm_create(4, 0, optype, GVA_SIZE, 0, 0); // 4
    EXPECT_NE(handle, nullptr);

    char* mock_host = static_cast<char*>(malloc(BATCH_SIZE * COPY_SIZE));
    uint64_t sizes[BATCH_SIZE] = {COPY_SIZE, COPY_SIZE, COPY_SIZE, COPY_SIZE, COPY_SIZE};
    smem_batch_copy_params param = {};
    param.sources = (void**)malloc(BATCH_SIZE * sizeof(void*));
    param.destinations = (void**)malloc(BATCH_SIZE * sizeof(void*));
    param.dataSizes= sizes;;
    param.batchSize = BATCH_SIZE;

    for (uint32_t i = 0; i < BATCH_SIZE; ++i) {
        param.sources[i] = malloc(COPY_SIZE);
        GenerateData(param.sources[i], rankId, COPY_SIZE);
        param.destinations[i] = mock_host + i * COPY_SIZE;
    }

    MOCKER_CPP(&SmemBmEntry::DataCopyBatch, int32_t (*)(smem_batch_copy_params *, smem_bm_copy_type, uint32_t))
        .stubs()
        .will(returnValue(0));
    auto ret = smem_bm_copy_batch(handle, &param, SMEMB_COPY_H2GH, 0);
    EXPECT_EQ(ret, 0);

    for (uint32_t i = 0; i < BATCH_SIZE; ++i) {
        free(param.sources[i]);
    }
    free(param.sources);
    free(param.destinations);
    free(mock_host);

    smem_bm_destroy(handle);
}

smem_bm_t MockInitAndCreateHandle(uint32_t id)
{
    uint32_t rankId = 0;
    uint32_t rkSize = 2;
    uint32_t deviceId = 0;
    std::string ipPort = "tcp://192.168.100.101:8570";

    smem_set_log_level(1);
    smem_bm_config_t config;
    (void)smem_bm_config_init(&config);
    std::string url = "tcp://192.168.100.101/24:10005"; // tcp://192.168.100.100:8570
    config.autoRanking = false;
    config.rankId = rankId;
    config.startConfigStoreServer = true;

    MOCKER_CPP(&TcpConfigStore::Startup, int32_t (*)(const smem_tls_config &, int)).stubs().will(returnValue(0));
    auto ret = smem_bm_init(ipPort.c_str(), rkSize, deviceId, &config);
    EXPECT_EQ(ret, 0);

    smem_bm_data_op_type optype = SMEMB_DATA_OP_HOST_URMA;
    MOCKER_CPP(&SmemBmEntry::Initialize, int32_t (*)(const hybm_options &)).stubs().will(returnValue(0));
    smem_bm_t handle = smem_bm_create(id, 0, optype, GVA_SIZE, 0, 0);
    EXPECT_NE(handle, nullptr);
    return handle;
}

TEST_F(SmemBmTest, smem_bm_wait_success)
{
    smem_bm_t handle = MockInitAndCreateHandle(5); // 5

    smem_bm_wait(handle);
    smem_bm_destroy(handle);
    smem_bm_uninit(0);
}

TEST_F(SmemBmTest, smem_bm_register_user_mem_success)
{
    smem_bm_t handle = MockInitAndCreateHandle(6); // 6

    char* mock_host = static_cast<char*>(malloc(BATCH_SIZE * COPY_SIZE));
    EXPECT_NE(mock_host, nullptr);
    MOCKER_CPP(&SmemBmEntry::RegisterMem, int32_t (*)(uint64_t, uint64_t)).stubs().will(returnValue(0));
    auto ret = smem_bm_register_user_mem(handle, reinterpret_cast<uint64_t>(mock_host), BATCH_SIZE * COPY_SIZE);
    EXPECT_EQ(ret, 0);

    MOCKER_CPP(&SmemBmEntry::UnRegisterMem, int32_t (*)(uint64_t)).stubs().will(returnValue(0));
    ret = smem_bm_unregister_user_mem(handle, reinterpret_cast<uint64_t>(mock_host));
    EXPECT_EQ(ret, 0);

    smem_bm_destroy(handle);
    smem_bm_uninit(0);
    free(mock_host);
}

TEST_F(SmemBmTest, smem_set_extern_logger_failed)
{
    auto ret = smem_set_extern_logger(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(SmemBmTest, smem_set_extern_logger_success)
{
    auto my_logger = [](int code, const char* msg) {
        std::cout << "Code: " << code << ", Message: " << msg << std::endl;
    };
    auto ret = smem_set_extern_logger(my_logger);
    EXPECT_EQ(ret, 0);
}

TEST_F(SmemBmTest, smem_set_log_level_failed)
{
    auto ret = smem_set_log_level(111); // 111
    EXPECT_EQ(ret, -1);
}

TEST_F(SmemBmTest, smem_set_log_level_success)
{
    auto ret = smem_set_log_level(0);
    EXPECT_EQ(ret, 0);
}

TEST_F(SmemBmTest, smem_get_last_err_msg)
{
    auto ret = smem_get_last_err_msg();
    EXPECT_NE(ret, "");
}

TEST_F(SmemBmTest, smem_bm_get_rank_id_by_gva_success)
{
    smem_bm_t handle = MockInitAndCreateHandle(5); // 5

    MOCKER_CPP(&SmemBmEntry::GetRankIdByGva, int32_t (*)(void *)).stubs().will(returnValue(0));
    auto ret = smem_bm_get_rank_id_by_gva(handle, nullptr);
    EXPECT_EQ(ret, 0);
    smem_bm_destroy(handle);
    smem_bm_uninit(0);
}

TEST_F(SmemBmTest, smem_create_config_store_success)
{
    smem_bm_t handle = MockInitAndCreateHandle(5); // 5
    std::string url = "tcp://192.168.100.101:8570";
    auto ret = smem_create_config_store(url.c_str());
    EXPECT_EQ(ret, 0);
    smem_bm_destroy(handle);
    smem_bm_uninit(0);
}

TEST_F(SmemBmTest, smem_bm_copy_failed)
{
    smem_bm_uninit(0);
    smem_bm_t handle = malloc(COPY_SIZE);
    void *base = malloc(COPY_SIZE);
    smem_copy_params params1 = {base, base, COPY_SIZE};
    auto ret = smem_bm_copy(handle, nullptr, SMEMB_COPY_H2G, 0);
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    ret = smem_bm_copy(nullptr, &params1, SMEMB_COPY_H2G, 0);
    EXPECT_EQ(ret, SM_INVALID_PARAM);

    ret = smem_bm_copy(handle, &params1, SMEMB_COPY_H2G, 0);
    EXPECT_EQ(ret, SM_NOT_INITIALIZED);

    free(handle);
    free(base);
}

TEST_F(SmemBmTest, smem_bm_copy_success)
{
    uint32_t rankId = 0;
    smem_bm_t handle = MockInitAndCreateHandle(7); // 7

    void *local_dev_mock = malloc(COPY_SIZE);
    EXPECT_NE(local_dev_mock, nullptr);
    void *base = malloc(COPY_SIZE);
    EXPECT_NE(base, nullptr);

    GenerateData(base, rankId, COPY_SIZE);
    smem_copy_params params1 = {base, local_dev_mock, COPY_SIZE};

    MOCKER_CPP(&SmemBmEntry::DataCopy, int32_t (*)(const void *, void *, uint64_t, smem_bm_copy_type, uint32_t))
        .stubs()
        .will(returnValue(0));
    auto ret = smem_bm_copy(handle, &params1, SMEMB_COPY_H2G, 0);
    EXPECT_EQ(ret, 0);

    smem_bm_destroy(handle);
    smem_bm_uninit(0);
    free(local_dev_mock);
    free(base);
}

TEST_F(SmemBmTest, smem_bm_get_local_mem_size_invalid_handle)
{
    uint64_t size = smem_bm_get_local_mem_size_by_mem_type(nullptr, SMEM_MEM_TYPE_HOST);
    EXPECT_EQ(size, 0UL);
}

TEST_F(SmemBmTest, smem_bm_get_local_mem_size_by_mem_type)
{
    smem_bm_t handle = MockInitAndCreateHandle(8); // 8

    uint64_t size = smem_bm_get_local_mem_size_by_mem_type(handle, SMEM_MEM_TYPE_DEVICE);
    EXPECT_EQ(size, 0UL);

    size = smem_bm_get_local_mem_size_by_mem_type(handle, SMEM_MEM_TYPE_HOST);
    EXPECT_EQ(size, 0UL);

    size = smem_bm_get_local_mem_size_by_mem_type(handle, SMEM_MEM_TYPE_BUTT);
    EXPECT_EQ(size, 0UL);

    smem_bm_destroy(handle);
    smem_bm_uninit(0);
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