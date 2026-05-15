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
#include <unordered_map>
#include <deque>

#include "zbal_sma_device_pool.h"

using namespace zbal;
using namespace zbal::sma;
using namespace zbal::sma::device;

/* ================================================================
 * DeviceBlock
 * ================================================================ */

class TestDeviceBlock : public testing::Test {
public:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestDeviceBlock, DefaultConstructor)
{
    DeviceBlock block(0, nullptr, 1024, nullptr, nullptr, BT_SMALL);
    EXPECT_EQ(block.deviceId_, 0);
    EXPECT_EQ(block.stream_, nullptr);
    EXPECT_EQ(block.size_, 1024u);
    EXPECT_EQ(block.ptr_, nullptr);
    EXPECT_EQ(block.block_type_, BT_SMALL);
    EXPECT_EQ(block.allocated_, false);
    EXPECT_EQ(block.event_count_, 0);
    EXPECT_EQ(block.gc_count_, 0);
    EXPECT_EQ(block.prev_, nullptr);
    EXPECT_EQ(block.next_, nullptr);
    EXPECT_FALSE(block.isSplit());
}

TEST_F(TestDeviceBlock, SearchKeyConstructor)
{
    DeviceBlock searchKey(3, reinterpret_cast<aclrtStream>(0x1000), 2048);
    EXPECT_EQ(searchKey.deviceId_, 3);
    EXPECT_EQ(searchKey.size_, 2048u);
}

TEST_F(TestDeviceBlock, LargeBlockType)
{
    DeviceBlock block(0, nullptr, kLargeBuffer, nullptr, nullptr, BT_BIG);
    EXPECT_EQ(block.block_type_, BT_BIG);
}

TEST_F(TestDeviceBlock, IsSplitFalseWhenNoNeighbors)
{
    DeviceBlock a(0, nullptr, 512, nullptr, nullptr, BT_SMALL);
    EXPECT_FALSE(a.isSplit());
}

TEST_F(TestDeviceBlock, IsSplitTrueWithPrev)
{
    DeviceBlock a(0, nullptr, 512, nullptr, nullptr, BT_SMALL);
    DeviceBlock b(0, nullptr, 512, nullptr, nullptr, BT_SMALL);
    a.prev_ = &b;
    EXPECT_TRUE(a.isSplit());
}

TEST_F(TestDeviceBlock, IsSplitTrueWithNext)
{
    DeviceBlock a(0, nullptr, 512, nullptr, nullptr, BT_SMALL);
    DeviceBlock b(0, nullptr, 512, nullptr, nullptr, BT_SMALL);
    a.next_ = &b;
    EXPECT_TRUE(a.isSplit());
}

TEST_F(TestDeviceBlock, IsSplitTrueWithBothNeighbors)
{
    DeviceBlock before(0, nullptr, 512, nullptr, nullptr, BT_SMALL);
    DeviceBlock mid(0, nullptr, 1024, nullptr, nullptr, BT_SMALL);
    DeviceBlock after(0, nullptr, 512, nullptr, nullptr, BT_SMALL);
    before.next_ = &mid;
    mid.prev_ = &before;
    mid.next_ = &after;
    after.prev_ = &mid;
    EXPECT_TRUE(mid.isSplit());
}

/* ================================================================
 * DeviceBlock::splice
 * ================================================================ */

TEST_F(TestDeviceBlock, SpliceInsertsBetween)
{
    DeviceBlock before(0, nullptr, 512, nullptr, nullptr, BT_SMALL);
    DeviceBlock mid(0, nullptr, 1024, nullptr, nullptr, BT_SMALL);
    DeviceBlock after(0, nullptr, 512, nullptr, nullptr, BT_SMALL);
    before.next_ = &after;
    after.prev_ = &before;

    mid.splice(&before, &after);

    EXPECT_EQ(before.next_, &mid);
    EXPECT_EQ(mid.prev_, &before);
    EXPECT_EQ(mid.next_, &after);
    EXPECT_EQ(after.prev_, &mid);
}

TEST_F(TestDeviceBlock, SpliceAtHead)
{
    DeviceBlock mid(0, nullptr, 1024, nullptr, nullptr, BT_SMALL);
    DeviceBlock after(0, nullptr, 512, nullptr, nullptr, BT_SMALL);

    mid.splice(nullptr, &after);

    EXPECT_EQ(mid.prev_, nullptr);
    EXPECT_EQ(mid.next_, &after);
    EXPECT_EQ(after.prev_, &mid);
}

TEST_F(TestDeviceBlock, SpliceAtTail)
{
    DeviceBlock before(0, nullptr, 512, nullptr, nullptr, BT_SMALL);
    DeviceBlock mid(0, nullptr, 1024, nullptr, nullptr, BT_SMALL);
    before.next_ = nullptr; // before is tail

    mid.splice(&before, nullptr);

    EXPECT_EQ(before.next_, &mid);
    EXPECT_EQ(mid.prev_, &before);
    EXPECT_EQ(mid.next_, nullptr);
}

TEST_F(TestDeviceBlock, SpliceBothNull)
{
    DeviceBlock mid(0, nullptr, 1024, nullptr, nullptr, BT_SMALL);

    mid.splice(nullptr, nullptr);

    EXPECT_EQ(mid.prev_, nullptr);
    EXPECT_EQ(mid.next_, nullptr);
}

/* ================================================================
 * DeviceBlock compare
 * ================================================================ */

TEST_F(TestDeviceBlock, CompareByStream)
{
    DeviceBlock a(0, reinterpret_cast<aclrtStream>(0x100), 512, nullptr, nullptr, BT_SMALL);
    DeviceBlock b(0, reinterpret_cast<aclrtStream>(0x200), 512, nullptr, nullptr, BT_SMALL);
    EXPECT_TRUE(DeviceBlockCompareBySize(&a, &b));  // a.stream < b.stream
    EXPECT_FALSE(DeviceBlockCompareBySize(&b, &a));
}

TEST_F(TestDeviceBlock, CompareBySizeWhenSameStream)
{
    DeviceBlock a(0, reinterpret_cast<aclrtStream>(0x100), 256, nullptr, nullptr, BT_SMALL);
    DeviceBlock b(0, reinterpret_cast<aclrtStream>(0x100), 512, nullptr, nullptr, BT_SMALL);
    EXPECT_TRUE(DeviceBlockCompareBySize(&a, &b));  // a.size < b.size
    EXPECT_FALSE(DeviceBlockCompareBySize(&b, &a));
}

TEST_F(TestDeviceBlock, CompareByPtrWhenSameStreamAndSize)
{
    DeviceBlock a(0, nullptr, 512, nullptr, reinterpret_cast<void *>(0x1000), BT_SMALL);
    DeviceBlock b(0, nullptr, 512, nullptr, reinterpret_cast<void *>(0x2000), BT_SMALL);
    EXPECT_TRUE(DeviceBlockCompareBySize(&a, &b));  // a.ptr < b.ptr
    EXPECT_FALSE(DeviceBlockCompareBySize(&b, &a));
}

/* ================================================================
 * DeviceBlockPool
 * ================================================================ */

class TestDeviceBlockPool : public testing::Test {
public:
    void SetUp() override
    {
        blockSM_ = new DeviceBlock(0, nullptr, 512, &pool_, reinterpret_cast<void *>(0x1000), BT_SMALL);
        blockLG_ = new DeviceBlock(0, nullptr, kLargeBuffer, &pool_, reinterpret_cast<void *>(0x2000), BT_BIG);
    }

    void TearDown() override
    {
        delete blockSM_;
        delete blockLG_;
    }

    DeviceBlockPool pool_;
    DeviceBlock *blockSM_;
    DeviceBlock *blockLG_;
};

TEST_F(TestDeviceBlockPool, DefaultIsNotPrivate)
{
    EXPECT_FALSE(pool_.is_private_);
    EXPECT_EQ(pool_.use_count_, 1);
    EXPECT_EQ(pool_.npuMalloc_count_, 0);
}

TEST_F(TestDeviceBlockPool, PrivatePool)
{
    DeviceBlockPool privatePool(true);
    EXPECT_TRUE(privatePool.is_private_);
}

TEST_F(TestDeviceBlockPool, InsertSmallBlock)
{
    pool_.insertBlock(BT_SMALL, blockSM_);
    EXPECT_EQ(pool_.small_blocks_.size(), 1u);
    EXPECT_EQ(pool_.large_blocks_.size(), 0u);
    EXPECT_TRUE(pool_.small_blocks_.find(blockSM_) != pool_.small_blocks_.end());
}

TEST_F(TestDeviceBlockPool, InsertLargeBlock)
{
    pool_.insertBlock(BT_BIG, blockLG_);
    EXPECT_EQ(pool_.small_blocks_.size(), 0u);
    EXPECT_EQ(pool_.large_blocks_.size(), 1u);
    EXPECT_TRUE(pool_.large_blocks_.find(blockLG_) != pool_.large_blocks_.end());
}

TEST_F(TestDeviceBlockPool, InsertBothTypes)
{
    pool_.insertBlock(BT_SMALL, blockSM_);
    pool_.insertBlock(BT_BIG, blockLG_);
    EXPECT_EQ(pool_.small_blocks_.size(), 1u);
    EXPECT_EQ(pool_.large_blocks_.size(), 1u);
}

TEST_F(TestDeviceBlockPool, EraseSmallBlock)
{
    pool_.insertBlock(BT_SMALL, blockSM_);
    EXPECT_EQ(pool_.small_blocks_.size(), 1u);

    pool_.eraseBlock(BT_SMALL, blockSM_);
    EXPECT_EQ(pool_.small_blocks_.size(), 0u);
}

TEST_F(TestDeviceBlockPool, EraseLargeBlock)
{
    pool_.insertBlock(BT_BIG, blockLG_);
    EXPECT_EQ(pool_.large_blocks_.size(), 1u);

    pool_.eraseBlock(BT_BIG, blockLG_);
    EXPECT_EQ(pool_.large_blocks_.size(), 0u);
}

TEST_F(TestDeviceBlockPool, InsertMultipleBlocksSorted)
{
    DeviceBlock b1(0, nullptr, 256, &pool_, reinterpret_cast<void *>(0x1000), BT_SMALL);
    DeviceBlock b2(0, nullptr, 128, &pool_, reinterpret_cast<void *>(0x2000), BT_SMALL);
    DeviceBlock b3(0, nullptr, 512, &pool_, reinterpret_cast<void *>(0x3000), BT_SMALL);

    pool_.insertBlock(BT_SMALL, &b1);
    pool_.insertBlock(BT_SMALL, &b2);
    pool_.insertBlock(BT_SMALL, &b3);

    EXPECT_EQ(pool_.small_blocks_.size(), 3u);

    // verify set ordering by size (via custom comparator)
    auto it = pool_.small_blocks_.begin();
    EXPECT_EQ((*it)->size_, 128u); // smallest
    ++it;
    EXPECT_EQ((*it)->size_, 256u);
    ++it;
    EXPECT_EQ((*it)->size_, 512u); // largest

    pool_.eraseBlock(BT_SMALL, &b1);
    pool_.eraseBlock(BT_SMALL, &b2);
    pool_.eraseBlock(BT_SMALL, &b3);
}

/* ================================================================
 * DeviceAllocParams
 * ================================================================ */

TEST_F(TestDeviceBlock, AllocParamsConstruction)
{
    DeviceBlockPool pool;
    DeviceAllocParams params(2, 1024, reinterpret_cast<aclrtStream>(0x500), &pool, 2048, BT_SMALL);

    EXPECT_EQ(params.device(), 2);
    EXPECT_EQ(params.stream(), reinterpret_cast<aclrtStream>(0x500));
    EXPECT_EQ(params.size(), 1024u);
    EXPECT_EQ(params.alloc_size_, 2048u);
    EXPECT_EQ(params.block_type_, BT_SMALL);
    EXPECT_EQ(params.result_, Z_OK);
    EXPECT_EQ(params.block_, nullptr);
}
