/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <gtest/gtest.h>
#include <stdlib.h>
#include "smem_shm_api.h"

using namespace shm;

class ShmemWrapperTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }

    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

TEST_F(ShmemWrapperTest, smem_api_test)
{
    std::string outLibPath = OUT_LIB_PATH;
    ASSERT_EQ(SmemApi::LoadLibrary("") != 0, true);
    ASSERT_EQ(SmemApi::LoadLibrary(outLibPath + "/smem/lib64/") == 0, true);

    ASSERT_EQ(SmemApi::SmemInit(0) == 0, true);
}

