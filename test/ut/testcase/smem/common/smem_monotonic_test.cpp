/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <gtest/gtest.h>

#include "common/smem_monotonic_time.h"

using namespace ock::smem;

class SmMonotonicTest : public testing::Test {
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

TEST_F(SmMonotonicTest, time_test)
{
    ASSERT_EQ(MonotonicTime::TimeUs() != 0, true);
    ASSERT_EQ(MonotonicTime::TimeNs() != 0, true);
}