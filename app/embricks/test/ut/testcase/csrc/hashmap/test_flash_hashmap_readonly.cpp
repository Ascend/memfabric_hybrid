/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Embricks is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <gtest/gtest.h>

#include "emb_flash_hashmap_readonly.h"

using namespace ock::emb::hashmap;
class TestFlashHashmapReadonly : public testing::Test {
public:
    static void SetUpTestCase() {}

    static void TearDownTestCase() {}

    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(TestFlashHashmapReadonly, StructSize)
{
    EM_LOG_DEBUG("size of ReadonlyHashBucket: " << sizeof(ReadonlyHashBucket));

    EXPECT_TRUE(sizeof(ReadonlyHashBucket) == 64L);
}