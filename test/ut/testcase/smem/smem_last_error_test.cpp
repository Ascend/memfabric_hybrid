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
#include <gtest/gtest.h>

#include "common/smem_last_error.h"

using namespace ock::smem;

class SmLastErrorTest : public testing::Test {
public:
    static void SetUpTestCase() {}

    static void TearDownTestCase() {}

    void SetUp() override {}

    void TearDown() override {}
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

TEST_F(SmLastErrorTest, last_error_set_char_ptr)
{
    const char *msg = "test error message";
    SmLastError::Set(msg);
    ASSERT_EQ(std::string(msg) == SmLastError::GetAndClear(false), true);
}

TEST_F(SmLastErrorTest, last_error_empty_string)
{
    std::string empty = "";
    SmLastError::Set(empty);
    ASSERT_EQ(empty == SmLastError::GetAndClear(false), true);
}

TEST_F(SmLastErrorTest, last_error_long_string)
{
    std::string longStr(1000, 'a');
    SmLastError::Set(longStr);
    ASSERT_EQ(longStr == SmLastError::GetAndClear(false), true);
}

TEST_F(SmLastErrorTest, last_error_get_without_clear)
{
    std::string str = "test1";
    SmLastError::Set(str);
    
    const char *result1 = SmLastError::GetAndClear(false);
    ASSERT_EQ(std::string(result1) == str, true);
    
    const char *result2 = SmLastError::GetAndClear(false);
    ASSERT_EQ(std::string(result2) == str, true);
    
    const char *result3 = SmLastError::GetAndClear(true);
    ASSERT_EQ(std::string(result3) == str, true);
    
    const char *result4 = SmLastError::GetAndClear(false);
    ASSERT_EQ(std::string(result4).empty(), true);
}

TEST_F(SmLastErrorTest, last_error_get_with_clear)
{
    std::string str = "test2";
    SmLastError::Set(str);
    
    const char *result1 = SmLastError::GetAndClear(true);
    ASSERT_EQ(std::string(result1) == str, true);
    
    const char *result2 = SmLastError::GetAndClear(false);
    ASSERT_EQ(std::string(result2).empty(), true);
}

TEST_F(SmLastErrorTest, last_error_multiple_sets)
{
    std::string str1 = "error1";
    std::string str2 = "error2";
    std::string str3 = "error3";
    
    SmLastError::Set(str1);
    ASSERT_EQ(std::string(SmLastError::GetAndClear(true)) == str1, true);
    
    SmLastError::Set(str2);
    ASSERT_EQ(std::string(SmLastError::GetAndClear(true)) == str2, true);
    
    SmLastError::Set(str3);
    ASSERT_EQ(std::string(SmLastError::GetAndClear(true)) == str3, true);
}

TEST_F(SmLastErrorTest, last_error_special_characters)
{
    std::string special = "error: with special chars !@#$%^&*()";
    SmLastError::Set(special);
    ASSERT_EQ(special == SmLastError::GetAndClear(false), true);
}

TEST_F(SmLastErrorTest, last_error_newline_characters)
{
    std::string withNewline = "error\nwith\nnewlines";
    SmLastError::Set(withNewline);
    ASSERT_EQ(withNewline == SmLastError::GetAndClear(false), true);
}