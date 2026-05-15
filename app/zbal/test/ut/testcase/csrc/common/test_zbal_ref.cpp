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
#include <vector>

#include "zbal_ref.h"

using namespace zbal;

class TestObj : public ZReferable {
public:
    explicit TestObj(int v) : value(v) {}
    int value = 0;
};

class TestZBALRef : public testing::Test {
public:
    void SetUp() override {}
    void TearDown() override {}
};

/*
 * ZMakeRef creates object and ZRef manages its lifetime.
 */
TEST_F(TestZBALRef, ZMakeRefOwnership)
{
    auto ref = ZMakeRef<TestObj>(100);
    EXPECT_EQ(ref->value, 100);
}

/*
 * Copy constructor: two ZRefs share the same object.
 */
TEST_F(TestZBALRef, CopyConstructorSharesOwnership)
{
    auto ref1 = ZMakeRef<TestObj>(42);
    {
        ZRef<TestObj> ref2(ref1);
        EXPECT_EQ(ref1.Get(), ref2.Get());
    }
    EXPECT_NE(ref1.Get(), nullptr);
    EXPECT_EQ(ref1->value, 42);
}

/*
 * Move constructor: ownership transfers, source becomes nullptr.
 */
TEST_F(TestZBALRef, MoveConstructorTransfersOwnership)
{
    auto ref1 = ZMakeRef<TestObj>(77);
    TestObj *raw = ref1.Get();

    ZRef<TestObj> ref2(std::move(ref1));
    EXPECT_EQ(ref1.Get(), nullptr);
    EXPECT_EQ(ref2.Get(), raw);
}

/*
 * Move assignment: old target of destination is released, source becomes null.
 */
TEST_F(TestZBALRef, MoveAssignmentReleasesOldTarget)
{
    auto ref1 = ZMakeRef<TestObj>(10);
    auto ref2 = ZMakeRef<TestObj>(20);

    ref1 = std::move(ref2);
    EXPECT_EQ(ref2.Get(), nullptr);
    EXPECT_EQ(ref1->value, 20);
}

/*
 * Self copy-assignment is a no-op.
 */
TEST_F(TestZBALRef, SelfCopyAssignment)
{
    auto ref = ZMakeRef<TestObj>(55);
    TestObj *raw = ref.Get();
    ref = ref;
    EXPECT_EQ(ref.Get(), raw);
}

/*
 * Assign nullptr to release ownership.
 */
TEST_F(TestZBALRef, AssignNullptr)
{
    auto ref = ZMakeRef<TestObj>(1);
    ref = static_cast<TestObj *>(nullptr);
    EXPECT_EQ(ref.Get(), nullptr);
}

/*
 * Set method: same pointer is no-op, different pointer releases old.
 */
TEST_F(TestZBALRef, SetSamePointerNoOp)
{
    auto ref = ZMakeRef<TestObj>(88);
    TestObj *raw = ref.Get();
    ref.Set(raw);
    EXPECT_EQ(ref.Get(), raw);
}

TEST_F(TestZBALRef, SetDifferentPointerReleasesOld)
{
    auto ref = ZMakeRef<TestObj>(1);
    auto *newObj = new TestObj(2);
    ref.Set(newObj);
    EXPECT_EQ(ref->value, 2);
}

/*
 * Equality operators with nullptr.
 */
TEST_F(TestZBALRef, EqualityWithNullptr)
{
    ZRef<TestObj> empty;
    EXPECT_EQ(empty, nullptr);
    EXPECT_FALSE(empty != nullptr);

    auto ref = ZMakeRef<TestObj>(5);
    EXPECT_NE(ref, nullptr);
}

/*
 * Multiple copies in a container stress-test reference counting.
 */
TEST_F(TestZBALRef, MultipleCopiesInContainer)
{
    auto ref = ZMakeRef<TestObj>(99);
    std::vector<ZRef<TestObj>> refs;
    for (int i = 0; i < 100; i++) {
        refs.push_back(ref);
    }
    refs.clear();
    EXPECT_EQ(ref->value, 99);
}
