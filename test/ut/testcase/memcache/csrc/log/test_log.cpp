/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include <iostream>
#include <thread>
#include <vector>
#include "gtest/gtest.h"
#include "mmc_read_write_lock.h"
#include "mmc_ref.h"
#include "spdlogger4c.h"

using namespace testing;
using namespace std;
using namespace ock::mmc;

class TestLog : public testing::Test {
public:
    TestLog();

    void SetUp() override;

    void TearDown() override;

protected:
};
TestLog::TestLog() {}

void TestLog::SetUp() {}

void TestLog::TearDown() {}

TEST_F(TestLog, normal_log_test)
{
    ASSERT_TRUE(SPDLOG_Init("/tmp/log.log", 8, 3 * 1024 * 1024, 20) != 0);
    std::cout << SPDLOG_GetLastErrorMessage() << std::endl;
    ASSERT_TRUE(SPDLOG_Init("/tmp/log.log", 1, 1 * 1024, 20) != 0);
    std::cout << SPDLOG_GetLastErrorMessage() << std::endl;
    ASSERT_TRUE(SPDLOG_GetLastErrorMessage() != nullptr);
    ASSERT_TRUE(SPDLOG_Init("/tmp/log.log", 1, 3 * 1024 * 1024, 10) == 0);
    ASSERT_TRUE(SPDLOG_AuditInit("/tmp/log2.log", 3 * 1024 * 1024, 20) == 0);
    SPDLOG_LogMessage(1, "test");
    SPDLOG_AuditLogMessage("test");
    ASSERT_TRUE(SPDLOG_ResetLogLevel(1) == 0);
}