/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include <thread>
#include <gtest/gtest.h>
#include "smem.h"
#include "smem_shm.h"

const int32_t UT_SMEM_ID = 1;
const char UT_IP_PORT[] = "tcp://127.0.0.1:7758";
const uint32_t UT_CREATE_MEM_SIZE = 1024;

class TestSmem : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp() override;
    void TearDown() override;

};

void TestSmem::SetUpTestCase() {}

void TestSmem::TearDownTestCase() {}

void TestSmem::SetUp() {}

void TestSmem::TearDown() {}

TEST_F(TestSmem, twoCardGroupTest)
{
    smem_set_log_level(0);
    uint32_t rankSize = 2;
    std::thread ts[rankSize];
    auto func = [](uint32_t rank, uint32_t rankCount) {
        void *gva;
        int32_t ret = smem_init(0);
        ASSERT_EQ(ret, 0);

        smem_shm_config_t config;
        ret = smem_shm_config_init(&config);
        ASSERT_EQ(ret, 0);
        ret = smem_shm_init(UT_IP_PORT, rankCount, rank, rank, &config);
        ASSERT_EQ(ret, 0);

        auto handle = smem_shm_create(UT_SMEM_ID, rankCount, rank, UT_CREATE_MEM_SIZE, SMEMS_DATA_OP_MTE, 0, &gva);
        ASSERT_NE(handle, nullptr);
    };

    for (uint32_t  rk = 0; rk < rankSize; rk++) {
        ts[rk] = std::thread(func, rk, rankSize);
    }

    for (uint32_t  rk = 0; rk < rankSize; rk++) {
        ts[rk].join();
    }
}
