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

#include "zbal_comm_group_meta.h"

using namespace zbal;
using namespace zbal::operators;

constexpr uint16_t kMetaSpaceSize = 512;
constexpr uint16_t kGroupCap = 4;
constexpr uintptr_t kBaseAddress = 0x10000;

class TestZBALGroupMetaEdge : public testing::Test {
public:
    void SetUp() override
    {
        arranger_.UnInitialize();
    }

    void TearDown() override
    {
        arranger_.UnInitialize();
    }

    void InitArranger(uint16_t groupCap = kGroupCap, uint64_t extraBytes = 0)
    {
        ZBALInitStateExt ext;
        ext.commMetaSpaceSize = kMetaSpaceSize;
        ext.commGroupCap = groupCap;
        ext.myCommMetaDeviceGva = reinterpret_cast<void *>(kBaseAddress);
        uint64_t required = static_cast<uint64_t>(kMetaSpaceSize) * 1024 * groupCap;
        ext.metaSizeOfDevice = required + extraBytes;
        ASSERT_EQ(arranger_.Initialize(ext), Z_OK);
    }

    GroupMetaArranger &arranger_ = GroupMetaArranger::Instance();
};

/* ================================================================
 * Before Initialize, Initialized() returns false
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, NotInitializedBeforeInit)
{
    EXPECT_FALSE(arranger_.Initialized());
}

/* ================================================================
 * UnInitialize clears state so Initialized() returns false again
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, UnInitializeClearsState)
{
    InitArranger();
    EXPECT_TRUE(arranger_.Initialized());

    arranger_.UnInitialize();
    EXPECT_FALSE(arranger_.Initialized());
}

/* ================================================================
 * Initialize with exactly the required meta space (boundary)
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, InitializeExactBoundary)
{
    ZBALInitStateExt ext;
    ext.commMetaSpaceSize = kMetaSpaceSize;
    ext.commGroupCap = 1;
    ext.myCommMetaDeviceGva = reinterpret_cast<void *>(kBaseAddress);
    ext.metaSizeOfDevice = static_cast<uint64_t>(kMetaSpaceSize) * 1024; /* exact */

    EXPECT_EQ(arranger_.Initialize(ext), Z_OK);
    EXPECT_TRUE(arranger_.Initialized());
}

/* ================================================================
 * Initialize fails when metaSizeOfDevice < required
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, InitializeFailsInsufficientMeta)
{
    ZBALInitStateExt ext;
    ext.commMetaSpaceSize = kMetaSpaceSize;
    ext.commGroupCap = kGroupCap;
    ext.myCommMetaDeviceGva = reinterpret_cast<void *>(kBaseAddress);
    uint64_t required = static_cast<uint64_t>(kMetaSpaceSize) * 1024 * kGroupCap;
    ext.metaSizeOfDevice = required - 1; /* one byte short */

    EXPECT_NE(arranger_.Initialize(ext), Z_OK);
    EXPECT_FALSE(arranger_.Initialized());
}

/* ================================================================
 * Initialize zero commGroupCap fails
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, InitializeZeroGroupCap)
{
    ZBALInitStateExt ext;
    ext.commMetaSpaceSize = kMetaSpaceSize;
    ext.commGroupCap = 0;
    ext.myCommMetaDeviceGva = reinterpret_cast<void *>(kBaseAddress);
    ext.metaSizeOfDevice = 1024; /* any positive value */

    EXPECT_NE(arranger_.Initialize(ext), Z_OK);
    EXPECT_FALSE(arranger_.Initialized());
}

/* ================================================================
 * GetGroupByIndex out of bounds before init
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, GetGroupByIndexBeforeInit)
{
    uint32_t idx = 0;
    uintptr_t gva = 0, param = 0, exch = 0;
    EXPECT_NE(arranger_.GetGroupByIndex(0, gva, param, exch), Z_OK);
}

/* ================================================================
 * CurrentGroup after exhausting all groups
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, ExhaustGroupsThenFail)
{
    InitArranger(kGroupCap);
    uint32_t idx;
    uintptr_t gva;
    uintptr_t param;
    uintptr_t exch;

    for (uint32_t i = 0; i < kGroupCap; i++) {
        EXPECT_EQ(arranger_.CurrentGroup(idx, gva, param, exch), Z_OK);
        EXPECT_EQ(idx, i);
        arranger_.Move2NextGroup();
    }

    /* one past the end */
    EXPECT_NE(arranger_.CurrentGroup(idx, gva, param, exch), Z_OK);
}

/* ================================================================
 * GetGroupByIndex boundary values
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, GetGroupByIndexLastValid)
{
    InitArranger(kGroupCap);
    uintptr_t gva;
    uintptr_t param;
    uintptr_t exch;

    EXPECT_EQ(arranger_.GetGroupByIndex(kGroupCap - 1, gva, param, exch), Z_OK);
    EXPECT_NE(arranger_.GetGroupByIndex(kGroupCap, gva, param, exch), Z_OK);
}

/* ================================================================
 * Address calculation: consecutive groups are adjacent
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, AddressAdjacencyAcrossGroups)
{
    InitArranger(kGroupCap);
    uintptr_t gva0, param0, exch0;
    uintptr_t gva1, param1, exch1;

    ASSERT_EQ(arranger_.GetGroupByIndex(0, gva0, param0, exch0), Z_OK);
    ASSERT_EQ(arranger_.GetGroupByIndex(1, gva1, param1, exch1), Z_OK);

    uint64_t stride = arranger_.GetSingleMetaSpaceSize();
    EXPECT_EQ(gva1, gva0 + stride);
    EXPECT_EQ(param1, param0 + stride);
    EXPECT_EQ(exch1, exch0 + stride);
}

/* ================================================================
 * Address layout within single group: param is right after meta,
 * exchange is after param at ZBAL_OPERATE_PARAM_SIZE offset
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, IntraGroupAddressLayout)
{
    InitArranger(1);
    uintptr_t gva;
    uintptr_t param;
    uintptr_t exch;

    ASSERT_EQ(arranger_.GetGroupByIndex(0, gva, param, exch), Z_OK);

    EXPECT_EQ(param, gva + sizeof(CommGroupInfo));
    EXPECT_EQ(exch, gva + ZBAL_OPERATE_PARAM_SIZE);
}

/* ================================================================
 * Space size invariants
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, SpaceSizeInvariants)
{
    InitArranger(kGroupCap);

    uint64_t single = arranger_.GetSingleMetaSpaceSize();
    uint64_t commInfo = arranger_.GetCommGroupInfoSpaceSize();
    uint64_t param = arranger_.GetParamSpaceSize();
    uint64_t exch = arranger_.GetExchangeSpaceSize();

    EXPECT_EQ(commInfo, sizeof(CommGroupInfo));
    EXPECT_EQ(param, ZBAL_OPERATE_PARAM_SIZE - sizeof(CommGroupInfo));
    EXPECT_EQ(exch, single - ZBAL_OPERATE_PARAM_SIZE);
    EXPECT_EQ(commInfo + param, ZBAL_OPERATE_PARAM_SIZE);
    EXPECT_EQ(commInfo + param + exch, single);
}

/* ================================================================
 * Initialize is idempotent — second Initialize returns Z_OK
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, InitializeIdempotent)
{
    InitArranger(kGroupCap);
    uint32_t idx;
    uintptr_t gvaBefore;
    uintptr_t param;
    uintptr_t exch;
    ASSERT_EQ(arranger_.CurrentGroup(idx, gvaBefore, param, exch), Z_OK);

    /* second init is no-op */
    EXPECT_EQ(arranger_.Initialize(ZBALInitStateExt{}), Z_OK);

    uintptr_t gvaAfter;
    ASSERT_EQ(arranger_.CurrentGroup(idx, gvaAfter, param, exch), Z_OK);
    EXPECT_EQ(gvaBefore, gvaAfter); /* state preserved */
}

/* ================================================================
 * Re-initialize after UnInitialize works
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, ReinitAfterUninit)
{
    InitArranger(kGroupCap);
    arranger_.UnInitialize();

    /* state is cleared */
    EXPECT_FALSE(arranger_.Initialized());

    /* re-init with different params */
    ZBALInitStateExt ext;
    ext.commMetaSpaceSize = 1024; /* different size */
    ext.commGroupCap = 2;
    ext.myCommMetaDeviceGva = reinterpret_cast<void *>(0x20000);
    ext.metaSizeOfDevice = static_cast<uint64_t>(1024) * 1024 * 2;

    EXPECT_EQ(arranger_.Initialize(ext), Z_OK);
    EXPECT_EQ(arranger_.GetSingleMetaSpaceSize(), 1024u * 1024u);
}

/* ================================================================
 * CurrentGroup index increments with Move2NextGroup
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, CurrentGroupIndexProgression)
{
    InitArranger(kGroupCap);
    uint32_t idx;
    uintptr_t gva;
    uintptr_t param;
    uintptr_t exch;

    EXPECT_EQ(arranger_.CurrentGroup(idx, gva, param, exch), Z_OK);
    EXPECT_EQ(idx, 0u);

    arranger_.Move2NextGroup();
    EXPECT_EQ(arranger_.CurrentGroup(idx, gva, param, exch), Z_OK);
    EXPECT_EQ(idx, 1u);

    arranger_.Move2NextGroup();
    EXPECT_EQ(arranger_.CurrentGroup(idx, gva, param, exch), Z_OK);
    EXPECT_EQ(idx, 2u);

    arranger_.Move2NextGroup();
    EXPECT_EQ(arranger_.CurrentGroup(idx, gva, param, exch), Z_OK);
    EXPECT_EQ(idx, 3u);

    arranger_.Move2NextGroup();
    EXPECT_NE(arranger_.CurrentGroup(idx, gva, param, exch), Z_OK);
}

/* ================================================================
 * Single group capacity — CurrentGroup works once
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, SingleGroupCapWorksOnce)
{
    InitArranger(1);
    uint32_t idx;
    uintptr_t gva;
    uintptr_t param;
    uintptr_t exch;

    EXPECT_EQ(arranger_.CurrentGroup(idx, gva, param, exch), Z_OK);
    EXPECT_EQ(idx, 0u);

    arranger_.Move2NextGroup();
    EXPECT_NE(arranger_.CurrentGroup(idx, gva, param, exch), Z_OK);
}

/* ================================================================
 * All groups produce addresses within the declared meta region
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, AllAddressesWithinRegion)
{
    InitArranger(kGroupCap);
    uintptr_t gva;
    uintptr_t param;
    uintptr_t exch;
    uint64_t single = arranger_.GetSingleMetaSpaceSize();
    uintptr_t regionEnd = kBaseAddress + single * kGroupCap;

    for (uint16_t i = 0; i < kGroupCap; i++) {
        ASSERT_EQ(arranger_.GetGroupByIndex(i, gva, param, exch), Z_OK);
        EXPECT_GE(gva, kBaseAddress);
        EXPECT_LT(gva, regionEnd);
        EXPECT_GE(param, kBaseAddress);
        EXPECT_LT(param, regionEnd);
        EXPECT_GE(exch, kBaseAddress);
        EXPECT_LT(exch, regionEnd);
    }
}

/* ================================================================
 * Initialize: commMetaSpaceSize zero
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, InitializeZeroMetaSpaceSize)
{
    ZBALInitStateExt ext;
    ext.commMetaSpaceSize = 0; // zero → singleMetaSpaceSize_ = 0
    ext.commGroupCap = 4;
    ext.myCommMetaDeviceGva = reinterpret_cast<void *>(kBaseAddress);
    ext.metaSizeOfDevice = 1;

    EXPECT_EQ(arranger_.Initialize(ext), Z_OK); // single=0 * cap=0 ≥ required
    arranger_.UnInitialize();
}

/* ================================================================
 * Move2NextGroup before initialization — no crash
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, Move2NextGroupBeforeInit)
{
    EXPECT_NO_THROW(arranger_.Move2NextGroup());
}

/* ================================================================
 * GetSingleMetaSpaceSize before init
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, SpaceSizeBeforeInit)
{
    // singleMetaSpaceSize_ is 0 before init
    EXPECT_EQ(arranger_.GetSingleMetaSpaceSize(), 0u);
    // sizeof(CommGroupInfo) is compile-time constant, always available
    EXPECT_EQ(arranger_.GetCommGroupInfoSpaceSize(), sizeof(CommGroupInfo));
    // These are computed from ZBAL_OPERATE_PARAM_SIZE, available even before init
    EXPECT_EQ(arranger_.GetParamSpaceSize(), ZBAL_OPERATE_PARAM_SIZE - sizeof(CommGroupInfo));
    // exchange = singleMetaSpaceSize_ - ZBAL_OPERATE_PARAM_SIZE, underflows to a huge value
    EXPECT_EQ(arranger_.GetExchangeSpaceSize(), static_cast<uint64_t>(-static_cast<int64_t>(ZBAL_OPERATE_PARAM_SIZE)));
}

/* ================================================================
 * Initialize with metaSizeOfDevice overflow check
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, InitializeWithLargeValues)
{
    ZBALInitStateExt ext;
    ext.commMetaSpaceSize = kMetaSpaceSize;
    ext.commGroupCap = kGroupCap;
    ext.myCommMetaDeviceGva = reinterpret_cast<void *>(kBaseAddress);
    ext.metaSizeOfDevice = static_cast<uint64_t>(kMetaSpaceSize) * 1024 * kGroupCap + 1024; // extra

    EXPECT_EQ(arranger_.Initialize(ext), Z_OK);
    EXPECT_TRUE(arranger_.Initialized());
    arranger_.UnInitialize();
}

/* ================================================================
 * UnInitialize multiple times is safe
 * ================================================================ */

TEST_F(TestZBALGroupMetaEdge, UnInitMultipleTimes)
{
    InitArranger(1);
    arranger_.UnInitialize();
    EXPECT_FALSE(arranger_.Initialized());
    arranger_.UnInitialize(); // second time
    EXPECT_FALSE(arranger_.Initialized());
}
