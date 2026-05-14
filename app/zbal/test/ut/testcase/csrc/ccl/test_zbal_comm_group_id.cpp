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

#include "zbal_comm_group_id.h"

using namespace zbal;
using namespace zbal::operators;

class TestZBALCommGroupId : public testing::Test {
public:
    static void SetUpTestCase() {}

    static void TearDownTestCase() {}

    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(TestZBALCommGroupId, DefaultConstructor)
{
    AutoReleaseGroupId groupId;
    EXPECT_EQ(groupId.Id(), UINT16_MAX);
    EXPECT_TRUE(groupId.GatheredGroupInfo().empty());
}

TEST_F(TestZBALCommGroupId, ParameterizedConstructor)
{
    AutoReleaseGroupId groupId(128, 4, 0, 0, "test_group");
    EXPECT_EQ(groupId.Id(), UINT16_MAX); /* not acquired yet */
    EXPECT_TRUE(groupId.GatheredGroupInfo().empty());
}

TEST_F(TestZBALCommGroupId, AcquireWithoutBootstrap)
{
    AutoReleaseGroupId groupId(128, 4, 0, 0, "test_group");
    /* Acquire should fail without bootstrap initialized */
    auto result = groupId.Acquire();
    EXPECT_NE(result, Z_OK);
}

TEST_F(TestZBALCommGroupId, ReleaseWithoutBootstrap)
{
    AutoReleaseGroupId groupId(128, 4, 0, 0, "test_group");
    /* Release without acquire should not crash */
    groupId.Release();
    EXPECT_EQ(groupId.Id(), static_cast<uint16_t>(-1));
}

TEST_F(TestZBALCommGroupId, MoveIdAndGatheredInfo)
{
    AutoReleaseGroupId gid1(128, 4, 0, 0, "g1");
    AutoReleaseGroupId gid2(128, 4, 1, 1, "g2");

    /* Move from gid2 to gid1 (both default, no acquired id) */
    gid1.MoveIdAndGatheredInfo(gid2);
    /* gid2's id should be reset to UINT16_MAX */
    EXPECT_EQ(gid2.Id(), UINT16_MAX);
}

TEST_F(TestZBALCommGroupId, MoveIdAndGatheredInfoSelf)
{
    AutoReleaseGroupId gid1(128, 4, 0, 0, "g1");
    /* self-move should be a no-op */
    gid1.MoveIdAndGatheredInfo(gid1);
    EXPECT_EQ(gid1.Id(), UINT16_MAX);
}

TEST_F(TestZBALCommGroupId, ReleaseMultipleTimes)
{
    AutoReleaseGroupId groupId(128, 4, 0, 0, "test");
    /* multiple releases without acquire should not crash */
    groupId.Release();
    groupId.Release();
    groupId.Release();
    EXPECT_EQ(groupId.Id(), static_cast<uint16_t>(-1));
}

TEST_F(TestZBALCommGroupId, AcquireWithZeroGroupSize)
{
    AutoReleaseGroupId groupId(0, 4, 0, 0, "test");
    auto result = groupId.Acquire();
    EXPECT_NE(result, Z_OK);
}

TEST_F(TestZBALCommGroupId, AcquireWithZeroRankCount)
{
    AutoReleaseGroupId groupId(128, 0, 0, 0, "test");
    auto result = groupId.Acquire();
    EXPECT_NE(result, Z_OK);
}

TEST_F(TestZBALCommGroupId, DefaultGroupInfo)
{
    AutoReleaseGroupId groupId;
    EXPECT_EQ(groupId.Id(), UINT16_MAX);
    EXPECT_TRUE(groupId.GatheredGroupInfo().empty());
}
