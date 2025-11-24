/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <gtest/gtest.h>

#include "mf_num_util.h"

using namespace ock::mf;

class MFNumUtilTest : public testing::Test {
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

TEST_F(MFNumUtilTest, IsDigit_1)
{
    std::string str1 = "aaaa";
    EXPECT_FALSE(ock::mf::NumUtil::IsDigit(str1));

    std::string str2 = "";
    EXPECT_FALSE(ock::mf::NumUtil::IsDigit(str2));

    std::string str3 = "   ";
    EXPECT_FALSE(ock::mf::NumUtil::IsDigit(str3));

    std::string str4 = "\t  ";
    EXPECT_FALSE(ock::mf::NumUtil::IsDigit(str4));

    std::string str5 = "  123 ";
    EXPECT_FALSE(ock::mf::NumUtil::IsDigit(str5));

    std::string str6 = "  123";
    EXPECT_TRUE(ock::mf::NumUtil::IsDigit(str6));

    std::string str7 = "1234";
    EXPECT_TRUE(ock::mf::NumUtil::IsDigit(str7));
}