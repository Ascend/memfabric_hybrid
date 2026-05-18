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

#include "zbal_def.h"
#include "test_zbal_def.h"


class TestZBALDef : public testing::Test {
public:
    static void SetUpTestCase() {}

    static void TearDownTestCase()
    {
    }

    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(TestZBALDef, VERIFY_STRUCT)
{
    auto size = sizeof(zbal_tensor_info_t);
    EXPECT_TRUE(size == ZBAL_TEST_NUMBER_SIXTYFOUR);
}
