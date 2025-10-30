/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <gtest/gtest.h>

#include "common/smem_last_error.h"

using namespace ock::smem;

class SmLastErrorTest : public testing::Test {
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

TEST_F(SmLastErrorTest, last_error_set_get)
{
    std::string str = "aaaa";
    SmLastError::Set(str);
    ASSERT_EQ(str == SmLastError::GetAndClear(false), true);
    ASSERT_EQ(str == SmLastError::GetAndClear(true), true);
    ASSERT_EQ(std::string(SmLastError::GetAndClear(false)).empty(), true);

    std::string str1 = "bbbb";
    SmLastError::Set(str1);
    ASSERT_EQ(str1 == SmLastError::GetAndClear(false), true);
}