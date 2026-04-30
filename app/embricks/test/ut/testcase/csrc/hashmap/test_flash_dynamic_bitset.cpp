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

#include "emb_common_includes.h"
#include "emb_flash_dynamic_bitset.h"

using namespace ock::emb;
using namespace ock::emb::hashmap;

class TestFlashDynamicBitset : public testing::Test {
public:
    static void SetUpTestCase() {}

    static void TearDownTestCase() {}

    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(TestFlashDynamicBitset, FlashDynamicBitSetInitialize)
{
    FlashDynamicBitSet bitset;
    auto result = bitset.Initialize(0);
    EXPECT_TRUE(result == EM_INVALID_PARAM);
    EXPECT_TRUE(bitset.Capacity() == 0);

    result = bitset.Initialize(512L);
    EXPECT_TRUE(result == EM_OK);
    EXPECT_TRUE(bitset.Capacity() == 512L);

    bitset.UnInitialize();

    result = bitset.Initialize(1023L);
    EXPECT_TRUE(result == EM_OK);
    EXPECT_TRUE(bitset.Capacity() == 1023L);

    return;
}

TEST_F(TestFlashDynamicBitset, FlashDynamicBitSetRepeatOp)
{
    FlashDynamicBitSet bitset;

    auto result = bitset.Initialize(512L);
    EXPECT_TRUE(result == EM_OK);

    bitset.Set(0);
    EXPECT_TRUE(bitset.Count() == UN1);

    bitset.Set(0);
    EXPECT_TRUE(bitset.Count() == UN1);

    bitset.Clear(UN1);
    EXPECT_TRUE(bitset.Count() == UN1);

    bitset.Clear(0);
    EXPECT_TRUE(bitset.Count() == 0);

    bitset.Clear(0);
    EXPECT_TRUE(bitset.Count() == 0);

    return;
}

TEST_F(TestFlashDynamicBitset, FlashDynamicBitSetOp)
{
    FlashDynamicBitSet bitset;

    auto result = bitset.Initialize(512L);
    EXPECT_TRUE(result == EM_OK);

    /* set lower boundary */
    bitset.Set(0);
    EXPECT_TRUE(bitset.Test(0));
    EXPECT_TRUE(!bitset.Full());
    EXPECT_TRUE(bitset.Count() == UN1);

    /* clear lower boundary */
    bitset.Clear(0);
    EXPECT_TRUE(!bitset.Test(0));
    EXPECT_TRUE(!bitset.Full());
    EXPECT_TRUE(bitset.Count() == 0);

    /* set upper boundary */
    bitset.Set(511L);
    EXPECT_TRUE(bitset.Test(511L));
    EXPECT_TRUE(!bitset.Full());
    EXPECT_TRUE(bitset.Count() == UN1);

    /* out of bound */
    bitset.Clear(512L);
    EXPECT_TRUE(!bitset.Test(512L));
    EXPECT_TRUE(!bitset.Full());
    EXPECT_TRUE(bitset.Count() == 1);

    /* out of bound */
    bitset.Clear(513L);
    EXPECT_TRUE(!bitset.Full());
    EXPECT_TRUE(bitset.Count() == 1);
}

TEST_F(TestFlashDynamicBitset, FlashDynamicBitSetFindSet)
{
    EM_LOG_DEBUG("FlashDynamicBitSetFindSet");

    FlashDynamicBitSet bitset;

    auto result = bitset.Initialize(512L);
    EXPECT_TRUE(result == EM_OK);

    /* set lower boundary */
    bitset.Set(0);
    EXPECT_TRUE(bitset.Test(0));
    EXPECT_TRUE(!bitset.Full());
    EXPECT_TRUE(bitset.Count() == UN1);

    uint32_t outPos = 0;
    EXPECT_TRUE(bitset.FindAndSet(0, outPos));
    EXPECT_TRUE(outPos == UN1);
    EXPECT_TRUE(bitset.FindAndSet(UN1, outPos));
    EXPECT_TRUE(outPos == UN2);

    bitset.Clear(0);
    bitset.Clear(UN1);

    for (auto i = 0; i < 512L; i++) {
        bitset.Set(i);
        EXPECT_TRUE(bitset.Test(i));
    }
    EXPECT_TRUE(bitset.Full());
    EXPECT_TRUE(bitset.Count() == 512L);

    for (auto i = 0; i < 512L; i++) {
        bitset.Clear(i);
        EXPECT_TRUE(!bitset.Test(i));
    }
    EXPECT_TRUE(!bitset.Full());
    EXPECT_TRUE(bitset.Count() == 0);

    for (auto i = 0; i < 512L; i++) {
        bitset.FindAndSet(i, outPos);
        EXPECT_TRUE(outPos == i);
    }
    EXPECT_TRUE(bitset.Full());
    EXPECT_TRUE(bitset.Count() == 512L);

    EM_LOG_DEBUG("FlashDynamicBitSetFindSet exit");
}

TEST_F(TestFlashDynamicBitset, FlashDynamicBitSetFindSet1023)
{
    EM_LOG_DEBUG("FlashDynamicBitSetFindSet1023");

    FlashDynamicBitSet bitset;

    const uint32_t capacity = 1023L;

    auto result = bitset.Initialize(capacity);
    EXPECT_TRUE(result == EM_OK);

    /* set lower boundary */
    bitset.Set(0);
    EXPECT_TRUE(bitset.Test(0));
    EXPECT_TRUE(!bitset.Full());
    EXPECT_TRUE(bitset.Count() == UN1);

    uint32_t outPos = 0;
    EXPECT_TRUE(bitset.FindAndSet(0, outPos));
    EXPECT_TRUE(outPos == UN1);
    EXPECT_TRUE(bitset.FindAndSet(UN1, outPos));
    EXPECT_TRUE(outPos == UN2);

    bitset.Clear(0);
    bitset.Clear(UN1);

    for (auto i = 0; i < capacity; i++) {
        bitset.Set(i);
        EXPECT_TRUE(bitset.Test(i));
    }
    EXPECT_TRUE(bitset.Full());
    EXPECT_TRUE(bitset.Count() == capacity);

    for (auto i = 0; i < capacity; i++) {
        bitset.Clear(i);
        EXPECT_TRUE(!bitset.Test(i));
    }
    EXPECT_TRUE(!bitset.Full());
    EXPECT_TRUE(bitset.Count() == 0);

    for (auto i = 0; i < capacity; i++) {
        bitset.FindAndSet(i, outPos);
        EXPECT_TRUE(outPos == i);
    }
    EXPECT_TRUE(bitset.Full());
    EXPECT_TRUE(bitset.Count() == capacity);
}

TEST_F(TestFlashDynamicBitset, FlashDynamicBitSetGetMemSize)
{
    EXPECT_TRUE(FlashDynamicBitSet::GetMemSize(0) == 0);
    EXPECT_TRUE(FlashDynamicBitSet::GetMemSize(UN64) == sizeof(uint64_t));
    EXPECT_TRUE(FlashDynamicBitSet::GetMemSize(UN64 - 1) == sizeof(uint64_t));
    EXPECT_TRUE(FlashDynamicBitSet::GetMemSize(UN64 + 1) == sizeof(uint64_t) * UN2);
}

TEST_F(TestFlashDynamicBitset, FlashDynamicBitSetInitialize2AndSet)
{
    FlashDynamicBitSet bitset;
    uint64_t rawMemory[UN4]{};
    uintptr_t memAddress = reinterpret_cast<uintptr_t>(&(rawMemory[0]));

    EXPECT_TRUE(bitset.Initialize(0, UN64, UN64, true) == EM_INVALID_PARAM);
    EXPECT_TRUE(bitset.Initialize(memAddress, 0, UN64, true) == EM_INVALID_PARAM);
    EXPECT_TRUE(bitset.Initialize(memAddress, UN64, 0, true) == EM_INVALID_PARAM);
    EXPECT_TRUE(bitset.Initialize(memAddress, UN64 * UN4, UN64 * UN3, true) == EM_INVALID_PARAM);

    EXPECT_TRUE(bitset.Initialize(memAddress, UN64 * UN4 / UN8, UN64 * UN4, true) == EM_OK);
    EXPECT_TRUE(bitset.Count() == 0);

    bitset.Set(1);
    bitset.Set(254L);

    FlashDynamicBitSet bitset1;
    EXPECT_TRUE(bitset1.Initialize(memAddress, UN64 * UN4 / UN8, UN64 * UN4, false) == EM_OK);
    EXPECT_TRUE(bitset1.Count() == UN2);

    bitset1.ClearAll();
    EXPECT_TRUE(bitset1.Count() == 0);
}