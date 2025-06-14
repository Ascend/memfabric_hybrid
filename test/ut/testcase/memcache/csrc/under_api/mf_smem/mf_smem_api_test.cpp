/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include <gtest/gtest.h>

#include "smem_bm_api.h"

using namespace ock::mmc;

class MFSmemApiTest : public testing::Test {
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

TEST_F(MFSmemApiTest, smem_bm_api_test)
{
    std::string outLibPath = OUT_LIB_PATH;
    ASSERT_EQ(MFSmemApi::LoadLibrary("") != 0, true);
    ASSERT_EQ(MFSmemApi::LoadLibrary(outLibPath + "/smem/lib64/") == 0, true);

    ASSERT_EQ(MFSmemApi::SmemInit(0) != 0, true);
}

