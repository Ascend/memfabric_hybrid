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

// Need access to private static members for deeper coverage
#define private public
#define protected public
#include "zbal_communicator.h"
#undef private
#undef protected

#include "zbal_communicator_dummy.h"
#include "zbal_comm_group_meta.h"
#include "zbal_comm_types.h"

using namespace zbal;
using namespace zbal::operators;

class TestZBALCommunicatorDummy : public testing::Test {
public:
    void SetUp() override
    {
        opt_.name = "test_dummy";
        opt_.worldSize = 8;
        opt_.groupSize = 4;
        opt_.myWorldRank = 2;
        opt_.myGroupRank = 1;
        opt_.deviceId = 0;
    }

    void TearDown() override
    {
        Communicator::DestroyAll();
    }

    CommGroupOptions opt_;
};

/* ================================================================
 * Construction: world group flag is stored in base class
 * ================================================================ */

TEST_F(TestZBALCommunicatorDummy, IsWorldGroupReflectsCtorArg)
{
    CommunicatorDummy world(opt_, true, nullptr);
    EXPECT_TRUE(world.IsWorldGroup());

    CommunicatorDummy nonWorld(opt_, false, nullptr);
    EXPECT_FALSE(nonWorld.IsWorldGroup());
}

/* ================================================================
 * Construction: Name() reads from options_ set by base ctor
 * ================================================================ */

TEST_F(TestZBALCommunicatorDummy, NameReadsFromOptions)
{
    const char *testNames[] = {"g", "moe_ep_12", "a_very_long_communicator_name"};
    for (auto *nm : testNames) {
        opt_.name = nm;
        CommunicatorDummy comm(opt_, false, nullptr);
        EXPECT_EQ(comm.Name(), std::string(nm));
    }
}

TEST_F(TestZBALCommunicatorDummy, NameEmptyString)
{
    opt_.name = "";
    CommunicatorDummy comm(opt_, false, nullptr);
    EXPECT_EQ(comm.Name(), "");
}

/* ================================================================
 * Construction: GroupId() default is UINT16_MAX (not assigned)
 * ================================================================ */

TEST_F(TestZBALCommunicatorDummy, GroupIdDefaultUnassigned)
{
    CommunicatorDummy comm(opt_, false, nullptr);
    EXPECT_EQ(comm.GroupId(), UINT16_MAX);
}

/* ================================================================
 * Construction: Meta info starts zeroed
 * ================================================================ */

TEST_F(TestZBALCommunicatorDummy, MetaInfoDefaultsZeroed)
{
    CommunicatorDummy comm(opt_, false, nullptr);
    const auto &m = comm.GetMetaInfo();
    EXPECT_EQ(m.groupSize, 0);
    EXPECT_EQ(m.myGroupRank, 0);
    EXPECT_EQ(m.groupIndex, 0);
    EXPECT_EQ(m.waitSymbol, 0u);
    EXPECT_EQ(m.myMetaGva, 0u);
}

/* ================================================================
 * Construction: worldGroup_ ptr stored correctly
 * ================================================================ */

TEST_F(TestZBALCommunicatorDummy, WorldGroupPtrStored)
{
    CommunicatorDummy worldGrp(opt_, true, nullptr);
    EXPECT_TRUE(worldGrp.IsWorldGroup());

    CommunicatorPtr worldRef = &worldGrp;
    CommunicatorDummy subGrp(opt_, false, worldRef);
    EXPECT_FALSE(subGrp.IsWorldGroup());
}

/* ================================================================
 * Initialize / UnInitialize round-trip
 * ================================================================ */

TEST_F(TestZBALCommunicatorDummy, InitializeReturnsOk)
{
    CommunicatorDummy comm(opt_, false, nullptr);
    EXPECT_EQ(comm.Initialize(), Z_OK);
}

TEST_F(TestZBALCommunicatorDummy, InitUninitReinit)
{
    CommunicatorDummy comm(opt_, false, nullptr);
    EXPECT_EQ(comm.Initialize(), Z_OK);
    comm.UnInitialize();
    EXPECT_EQ(comm.Initialize(), Z_OK);
    comm.UnInitialize();
    SUCCEED();
}

/* ================================================================
 * Meta info reference stability:
 * GetMetaInfo returns the same object across calls
 * ================================================================ */

TEST_F(TestZBALCommunicatorDummy, MetaInfoSameReference)
{
    CommunicatorDummy comm(opt_, false, nullptr);
    const auto &m1 = comm.GetMetaInfo();
    const auto &m2 = comm.GetMetaInfo();
    EXPECT_EQ(&m1, &m2);
}

/* ================================================================
 * Multiple independent dummy communicators
 * ================================================================ */

TEST_F(TestZBALCommunicatorDummy, IndependentInstances)
{
    CommGroupOptions optA;
    optA.name = "comm_a";
    optA.groupSize = 4;
    optA.myGroupRank = 0;

    CommGroupOptions optB;
    optB.name = "comm_b";
    optB.groupSize = 8;
    optB.myGroupRank = 3;

    CommunicatorDummy commA(optA, true, nullptr);
    CommunicatorDummy commB(optB, false, nullptr);

    EXPECT_TRUE(commA.IsWorldGroup());
    EXPECT_FALSE(commB.IsWorldGroup());
    EXPECT_EQ(commA.Name(), "comm_a");
    EXPECT_EQ(commB.Name(), "comm_b");
    EXPECT_NE(&commA.GetMetaInfo(), &commB.GetMetaInfo());
}

/* ================================================================
 * Create communicator via static Communicator::Create
 * exercises CreateInner factory with ZBAL_BACK_BUTT backend
 * ================================================================ */

TEST_F(TestZBALCommunicatorDummy, CreateViaFactory)
{
    Communicator::DestroyAll();

    ZBALInitStateExt ext;
    ext.worldSize = 1;
    ext.worldRankId = 0;
    ext.deviceId = 0;
    ext.commMetaSpaceSize = 512;
    ext.commGroupCap = 4;
    uintptr_t base = 0x10000;
    ext.myCommMetaDeviceGva = reinterpret_cast<void *>(base);
    ext.metaSizeOfDevice = 512u * 1024u * 4u;
    ext.gvaDevice = reinterpret_cast<void *>(0x20000);

    GroupMetaArranger::Instance().UnInitialize();

    zbal_comm_options_t apiOpt{};
    apiOpt.name = const_cast<char *>("world_dummy");
    apiOpt.backendType = ZBAL_BACK_BUTT;
    apiOpt.isWorldGroup = true;
    apiOpt.groupSize = 1;
    apiOpt.groupRankId = 0;

    zbal_comm_t comm = nullptr;
    // Create will fail because bootstrap is not initialized
    auto result = Communicator::Create(apiOpt, &comm, ext);
    EXPECT_NE(result, Z_OK);

    Communicator::DestroyAll();
}

/* ================================================================
 * Communicator::Destroy with nullptr
 * ================================================================ */

TEST_F(TestZBALCommunicatorDummy, DestroyNullComm)
{
    auto result = Communicator::Destroy(nullptr, 0);
    EXPECT_NE(result, Z_OK);
}

/* ================================================================
 * Communicator::Lookup with empty map
 * ================================================================ */

TEST_F(TestZBALCommunicatorDummy, LookupNonexistent)
{
    Communicator::DestroyAll();
    zbal_comm_t comm = nullptr;
    auto result = Communicator::Lookup("no_such_comm", &comm);
    EXPECT_NE(result, Z_OK);
}

/* ================================================================
 * Communicator::GetGlobalComm without world group
 * ================================================================ */

TEST_F(TestZBALCommunicatorDummy, GetGlobalCommFailsWithoutWorld)
{
    Communicator::DestroyAll();
    zbal_comm_t comm = nullptr;
    auto result = Communicator::GetGlobalComm(&comm);
    EXPECT_NE(result, Z_OK);
}

/* ================================================================
 * Communicator::Count reflects actual state
 * ================================================================ */

TEST_F(TestZBALCommunicatorDummy, CountStartsAtZero)
{
    Communicator::DestroyAll();
    EXPECT_EQ(Communicator::Count(), 0u);
}

/* ================================================================
 * Communicator::DumpAllComm with no comms (smoke test)
 * ================================================================ */

TEST_F(TestZBALCommunicatorDummy, DumpAllCommEmpty)
{
    Communicator::DestroyAll();
    EXPECT_NO_THROW(Communicator::DumpAllComm());
}

/* ================================================================
 * Communicator::GetCommProperty with null comm
 * ================================================================ */

TEST_F(TestZBALCommunicatorDummy, GetCommPropertyNullComm)
{
    zbal_comm_property_t prop{};
    auto result = Communicator::GetCommProperty(nullptr, &prop);
    EXPECT_NE(result, Z_OK);
}

/* ================================================================
 * CreateInner: non-world group without world group returns nullptr
 * ================================================================ */

TEST_F(TestZBALCommunicatorDummy, CreateInnerSubGroupWithoutWorldGroup)
{
    Communicator::DestroyAll();
    EXPECT_EQ(Communicator::gWorldCommunicator.Get(), nullptr);
    auto result = Communicator::CreateInner(ZBAL_BACK_BUTT, opt_, false);
    EXPECT_EQ(result, nullptr);
}

/* ================================================================
 * DestroyInner: null comm
 * ================================================================ */

TEST_F(TestZBALCommunicatorDummy, DestroyInnerNullComm)
{
    CommunicatorPtr nullComm(nullptr);
    auto result = Communicator::DestroyInner(nullComm);
    EXPECT_NE(result, Z_OK);
}

/* ================================================================
 * DestroyInner: non-world group not in map
 * ================================================================ */

TEST_F(TestZBALCommunicatorDummy, DestroyNonWorldGroupNotInMap)
{
    Communicator::DestroyAll();
    CommunicatorDummy comm(opt_, false, nullptr);
    CommunicatorPtr commPtr(&comm);
    auto result = Communicator::DestroyInner(commPtr);
    EXPECT_EQ(result, Z_OK);
    // manually reset ref count to prevent double-free on stack object
    commPtr = nullptr;
}
