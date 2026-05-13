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
#include <string>

#include "zbal_logger.h"

using namespace zbal;

class TestZBALLogger : public testing::Test {
public:
    void SetUp() override
    {
        OutLogger::Instance().SetLogLevel(ERROR_LEVEL);
        OutLogger::Instance().SetExternalLogFunction(nullptr, true);
    }

    void TearDown() override
    {
        OutLogger::Instance().SetExternalLogFunction(nullptr, true);
    }
};

/*
 * ValidateLevel: all defined levels are valid, out-of-bounds are not.
 */
TEST_F(TestZBALLogger, ValidateLevelBounds)
{
    EXPECT_TRUE(OutLogger::ValidateLevel(DEBUG_LEVEL));
    EXPECT_TRUE(OutLogger::ValidateLevel(INFO_LEVEL));
    EXPECT_TRUE(OutLogger::ValidateLevel(WARN_LEVEL));
    EXPECT_TRUE(OutLogger::ValidateLevel(ERROR_LEVEL));
    EXPECT_TRUE(OutLogger::ValidateLevel(FATAL_LEVEL));

    EXPECT_FALSE(OutLogger::ValidateLevel(-1));
    EXPECT_FALSE(OutLogger::ValidateLevel(BUTT_LEVEL));
    EXPECT_FALSE(OutLogger::ValidateLevel(100));
}

/*
 * Log level set and get round-trip through all defined levels.
 */
TEST_F(TestZBALLogger, LogLevelRoundTrip)
{
    auto &logger = OutLogger::Instance();

    logger.SetLogLevel(DEBUG_LEVEL);
    EXPECT_EQ(logger.GetLogLevel(), DEBUG_LEVEL);

    logger.SetLogLevel(WARN_LEVEL);
    EXPECT_EQ(logger.GetLogLevel(), WARN_LEVEL);

    logger.SetLogLevel(FATAL_LEVEL);
    EXPECT_EQ(logger.GetLogLevel(), FATAL_LEVEL);
}

/*
 * External log function: set, no-force-update skips overwrite, force overwrites.
 */
TEST_F(TestZBALLogger, ExternalLogFunctionLifecycle)
{
    auto &logger = OutLogger::Instance();

    ExternalLog f1 = [](int, const char *) {};
    logger.SetExternalLogFunction(f1);
    EXPECT_EQ(logger.GetExternalLogFunction(), f1);

    ExternalLog f2 = [](int, const char *) {};
    logger.SetExternalLogFunction(f2, false);
    EXPECT_EQ(logger.GetExternalLogFunction(), f1);

    logger.SetExternalLogFunction(f2, true);
    EXPECT_EQ(logger.GetExternalLogFunction(), f2);

    logger.SetExternalLogFunction(nullptr, true);
    EXPECT_EQ(logger.GetExternalLogFunction(), nullptr);
}

/*
 * ZBAL_ASSERT_RETURN: returns given value on false, continues on true.
 */
TEST_F(TestZBALLogger, AssertReturnMacro)
{
    auto failCase = []() -> int {
        ZBAL_ASSERT_RETURN(false, -100);
        return 0;
    };
    EXPECT_EQ(failCase(), -100);

    auto passCase = []() -> int {
        ZBAL_ASSERT_RETURN(true, -200);
        return 42;
    };
    EXPECT_EQ(passCase(), 42);
}
