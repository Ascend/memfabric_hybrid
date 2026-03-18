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
#include <iostream>
#include <string>

#define private public
#include "mf_out_logger.h"
#undef private

using namespace ock::mf;

static uint16_t g_testCode = 0;
static std::string g_alarmMsg;

void AlarmHandle(uint16_t code, const char *msg)
{
    g_testCode = code;
    g_alarmMsg = msg;
    std::cout << "[UT Alarm] " << "[" << g_testCode << "] " << g_alarmMsg << std::endl;
}

void ResumeHandle(uint16_t code)
{
    std::cout << "[UT Resume] " << "[" << code << "]" << std::endl;
    g_testCode = 0;
    g_alarmMsg.clear();
}

class OutLoggerTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
        OutLogger::Instance().SetAlarmLogFunction(AlarmHandle, ResumeHandle, true);
    }

    static void TearDownTestCase() {}

    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(OutLoggerTest, SetAlarmLogFunction)
{
    OutLogger::Instance().SetAlarmLogFunction(AlarmHandle, ResumeHandle, true);
    EXPECT_EQ(OutLogger::Instance().alarmFunc_, AlarmHandle);
    EXPECT_EQ(OutLogger::Instance().resumeFunc_, ResumeHandle);

    OutLogger::Instance().SetAlarmLogFunction(nullptr, nullptr, false);
    EXPECT_EQ(OutLogger::Instance().alarmFunc_, AlarmHandle);
    EXPECT_EQ(OutLogger::Instance().resumeFunc_, ResumeHandle);

    OutLogger::Instance().SetAlarmLogFunction(nullptr, nullptr, true);
    EXPECT_EQ(OutLogger::Instance().alarmFunc_, nullptr);
    EXPECT_EQ(OutLogger::Instance().resumeFunc_, nullptr);
}

TEST_F(OutLoggerTest, Alarm)
{
    OutLogger::Instance().SetAlarmLogFunction(AlarmHandle, ResumeHandle, true);
    std::string msg = "test alarm";
    MF_ALARM_LOG("", INNER_ERROR_CODE, msg.c_str());
    EXPECT_EQ(g_testCode, INNER_ERROR_CODE);

    MF_RESUME_LOG(INNER_ERROR_CODE);
    EXPECT_EQ(g_testCode, 0);

    MF_ALARM_LOG_LIMIT("", INNER_ERROR_CODE, msg.c_str());
    EXPECT_EQ(g_testCode, INNER_ERROR_CODE);
}

