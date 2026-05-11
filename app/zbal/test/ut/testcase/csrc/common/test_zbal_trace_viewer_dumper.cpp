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

#include "zbal_trace_viewer_dumper.h"

using namespace zbal;

constexpr uint16_t ZBAL_TEST_NUMBER_FIVE = 5;
constexpr uint16_t ZBAL_TEST_NUMBER_TEN = 10;

class TestZBALTraceViewerDumper : public testing::Test {
public:
    static void SetUpTestCase() {}

    static void TearDownTestCase() {}

    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(TestZBALTraceViewerDumper, WRONG_DIR)
{
    TraceViewerFormatFileWriter writer("/haha1", "f.json", "main");
    auto result = writer.Open();
    EXPECT_TRUE(result != Z_OK);
}

TEST_F(TestZBALTraceViewerDumper, WRITE_FILE)
{
    TraceViewerFormatFileWriter writer("/tmp/", "f.json", "main");
    auto result = writer.Open();
    EXPECT_TRUE(result == Z_OK);
    result = writer.AppendBegin("haha1", "aiv0", 0);
    EXPECT_TRUE(result == Z_OK);
    result = writer.AppendEnd("haha1", "aiv0", ZBAL_TEST_NUMBER_FIVE);
    EXPECT_TRUE(result == Z_OK);
    result = writer.AppendDuration("haha2", "aiv0", ZBAL_TEST_NUMBER_FIVE, ZBAL_TEST_NUMBER_TEN);
    EXPECT_TRUE(result == Z_OK);
    writer.Close();
}