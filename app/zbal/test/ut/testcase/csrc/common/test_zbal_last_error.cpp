/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <gtest/gtest.h>
#include <thread>
#include <string>

#include "zbal_last_error.h"

using namespace zbal;

class TestZBLastError : public testing::Test {
public:
    void SetUp() override
    {
        ZBLastError::GetAndClear(true);
    }

    void TearDown() override
    {
        ZBLastError::GetAndClear(true);
    }
};

/*
 * GetAndClear(false): error persists across multiple calls.
 */
TEST_F(TestZBLastError, GetPreservesErrorWhenNotClearing)
{
    ZBLastError::Set("persistent error");
    EXPECT_STREQ(ZBLastError::GetAndClear(false), "persistent error");
    EXPECT_STREQ(ZBLastError::GetAndClear(false), "persistent error");
}

/*
 * GetAndClear(true): error is cleared after retrieval.
 */
TEST_F(TestZBLastError, ClearWipesErrorAfterGet)
{
    ZBLastError::Set("one-shot error");
    EXPECT_STREQ(ZBLastError::GetAndClear(true), "one-shot error");
    EXPECT_STREQ(ZBLastError::GetAndClear(false), "");
}

/*
 * Set via std::string and verify retrieval.
 */
TEST_F(TestZBLastError, SetStdString)
{
    std::string msg = "std::string error message";
    ZBLastError::Set(msg);
    EXPECT_STREQ(ZBLastError::GetAndClear(true), msg.c_str());
}

/*
 * Set via const char* with empty string.
 */
TEST_F(TestZBLastError, SetEmptyCStr)
{
    ZBLastError::Set("");
    EXPECT_STREQ(ZBLastError::GetAndClear(false), "");
}

/*
 * Successive Set calls overwrite the previous error.
 */
TEST_F(TestZBLastError, SetOverwritesPreviousError)
{
    ZBLastError::Set("first");
    ZBLastError::Set("second");
    ZBLastError::Set("third");
    EXPECT_STREQ(ZBLastError::GetAndClear(true), "third");
}

/*
 * GetAndClear without any prior Set returns empty string.
 */
TEST_F(TestZBLastError, GetWithoutSetReturnsEmpty)
{
    EXPECT_STREQ(ZBLastError::GetAndClear(false), "");
    EXPECT_STREQ(ZBLastError::GetAndClear(true), "");
}

/*
 * Thread-local isolation: each thread has its own error state.
 */
TEST_F(TestZBLastError, ThreadLocalIsolation)
{
    ZBLastError::Set("main-thread-error");

    std::string threadError;
    std::thread t([&threadError]() {
        threadError = std::string(ZBLastError::GetAndClear(false));
        ZBLastError::Set("child-thread-error");
    });
    t.join();

    EXPECT_TRUE(threadError.empty());
    EXPECT_STREQ(ZBLastError::GetAndClear(true), "main-thread-error");
}
