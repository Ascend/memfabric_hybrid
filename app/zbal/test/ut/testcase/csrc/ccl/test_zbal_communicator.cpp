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
#include <sstream>

#define private public
#include "zbal_communicator.h"
#undef private

#include "zbal_communicator_dummy.h"
#include "zbal_comm_group_meta.h"
#include "test_zbal_def.h"

using namespace zbal;
using namespace zbal::operators;

class TestZBALCommunicator : public testing::Test {
public:
    static void SetUpTestCase() {}

    static void TearDownTestCase() {}

    void SetUp() override
    {
        CleanupStaticState();

        /* for group meta */
        uintptr_t baseAddress = 1024;
        stateExt_.commMetaSpaceSize = ZBAL_TEST_SIZE_512;
        stateExt_.commGroupCap = ZBAL_TEST_NUMBER_FOUR;
        stateExt_.myCommMetaDeviceGva = reinterpret_cast<void *>(baseAddress);
        stateExt_.metaSizeOfDevice = stateExt_.commMetaSpaceSize * ZBAL_TEST_SIZE_1KB * stateExt_.commGroupCap;
        /* for comm create */
        stateExt_.worldSize = 1;
        stateExt_.worldRankId = 0;
        stateExt_.gvaDevice = reinterpret_cast<void *>(ZBAL_TEST_SIZE_1KB);
        stateExt_.deviceId = 0;

        GroupMetaArranger::Instance().UnInitialize();
        GroupMetaArranger::Instance().Initialize(stateExt_);
    }

    void TearDown() override
    {
        CleanupStaticState();
    }

    static void CleanupStaticState()
    {
        Communicator::gCommLookupMap.clear();
        Communicator::gCommLookupMapByName.clear();
        Communicator::gWorldCommunicator = nullptr;
        ZBALInitState::Instance().Reset();
    }

    CommunicatorPtr CreateDummyComm(const std::string &name, bool isWorldGroup)
    {
        CommGroupOptions options;
        options.name = name;
        options.worldSize = 1;
        options.groupSize = 1;
        options.myWorldRank = 0;
        options.myGroupRank = 0;
        return CommunicatorPtr(new CommunicatorDummy(options, isWorldGroup, Communicator::gWorldCommunicator));
    }

    ZBALInitStateExt stateExt_;
};

TEST_F(TestZBALCommunicator, CommunicatorCreate)
{
    Communicator::DestroyAll();

    zbal_comm_options_t commOptApi;
    static std::string name; /* case1: name is empty */
    commOptApi.name = const_cast<char *>(name.c_str());
    zbal_comm_t communicator = nullptr;
    auto result = Communicator::Create(commOptApi, &communicator, stateExt_);
    EXPECT_TRUE(result != Z_OK);

    commOptApi.name = nullptr; /* case2: name is nullptr */
    communicator = nullptr;
    result = Communicator::Create(commOptApi, &communicator, stateExt_);
    EXPECT_TRUE(result != Z_OK);

    name = "moeep";
    commOptApi.name = const_cast<char *>(name.c_str());
    commOptApi.backendType = ZBAL_BACK_BUTT;

    commOptApi.isWorldGroup = false; /* case3: create non-world group firstly */
    commOptApi.groupSize = 1;
    commOptApi.groupRankId = 0;
    result = Communicator::Create(commOptApi, &communicator, stateExt_);
    EXPECT_TRUE(result != Z_OK);
    Communicator::DestroyAll();

    EXPECT_TRUE(GroupMetaArranger::Instance().Initialized() == false);
}

TEST_F(TestZBALCommunicator, CreateInner_DuplicateName)
{
    CommGroupOptions options;
    options.name = "dup_comm";
    options.worldSize = 1;
    options.groupSize = 1;

    auto comm = CreateDummyComm("dup_comm", false);
    Communicator::gCommLookupMapByName.emplace("dup_comm", comm);

    auto result = Communicator::CreateInner(ZBAL_BACK_BUTT, options, false);
    EXPECT_TRUE(result == nullptr);
}

TEST_F(TestZBALCommunicator, CreateInner_InvalidBackend)
{
    CommGroupOptions options;
    options.name = "test_invalid_backend";
    options.worldSize = 1;
    options.groupSize = 1;

    auto result = Communicator::CreateInner(static_cast<zbal_backend_t>(999), options, false);
    EXPECT_TRUE(result == nullptr);
}

TEST_F(TestZBALCommunicator, CreateInner_WorldGroup_Success)
{
    CommGroupOptions options;
    options.name = "world_comm";
    options.worldSize = 1;
    options.groupSize = 1;

    auto result = Communicator::CreateInner(ZBAL_BACK_BUTT, options, true);
    EXPECT_TRUE(result != nullptr);
    EXPECT_TRUE(Communicator::gWorldCommunicator != nullptr);
    EXPECT_EQ(Communicator::gCommLookupMapByName.size(), 1);
}

TEST_F(TestZBALCommunicator, CreateInner_WorldGroup_AlreadyExists)
{
    CommGroupOptions options;
    options.name = "world_comm2";
    options.worldSize = 1;
    options.groupSize = 1;

    auto first = Communicator::CreateInner(ZBAL_BACK_BUTT, options, true);
    EXPECT_TRUE(first != nullptr);

    CommGroupOptions options2;
    options2.name = "world_comm3";
    options2.worldSize = 1;
    options2.groupSize = 1;
    auto second = Communicator::CreateInner(ZBAL_BACK_BUTT, options2, true);
    EXPECT_TRUE(second == nullptr);
}

TEST_F(TestZBALCommunicator, CreateInner_NonWorld_NoWorld)
{
    CommGroupOptions options;
    options.name = "sub_no_world";
    options.worldSize = 1;
    options.groupSize = 1;

    auto result = Communicator::CreateInner(ZBAL_BACK_BUTT, options, false);
    EXPECT_TRUE(result == nullptr);
}

TEST_F(TestZBALCommunicator, CreateInner_NonWorld_Success)
{
    CommGroupOptions worldOpts;
    worldOpts.name = "world_for_sub";
    worldOpts.worldSize = 1;
    worldOpts.groupSize = 1;
    auto worldComm = Communicator::CreateInner(ZBAL_BACK_BUTT, worldOpts, true);
    ASSERT_TRUE(worldComm != nullptr);

    CommGroupOptions subOpts;
    subOpts.name = "sub_comm";
    subOpts.worldSize = 1;
    subOpts.groupSize = 1;
    auto subComm = Communicator::CreateInner(ZBAL_BACK_BUTT, subOpts, false);
    EXPECT_TRUE(subComm != nullptr);
    EXPECT_EQ(Communicator::gCommLookupMap.size(), 1);
    EXPECT_EQ(Communicator::gCommLookupMapByName.size(), ZBAL_TEST_NUMBER_TWO);
}

TEST_F(TestZBALCommunicator, GetGlobalComm_NotFound)
{
    zbal_comm_t comm = nullptr;
    auto result = Communicator::GetGlobalComm(&comm);
    EXPECT_EQ(result, Z_COMM_NOT_FOUND);
    EXPECT_TRUE(comm == nullptr);
}

TEST_F(TestZBALCommunicator, GetGlobalComm_Found)
{
    auto worldComm = CreateDummyComm("global_test", true);
    Communicator::gWorldCommunicator = worldComm;

    zbal_comm_t comm = nullptr;
    auto result = Communicator::GetGlobalComm(&comm);
    EXPECT_EQ(result, Z_OK);
    EXPECT_TRUE(comm != nullptr);
    EXPECT_EQ(comm, worldComm.Get());
}

TEST_F(TestZBALCommunicator, GetCommProperty_NullComm)
{
    zbal_comm_property_t property;
    auto result = Communicator::GetCommProperty(nullptr, &property);
    EXPECT_EQ(result, Z_INVALID_PARAM);
}

TEST_F(TestZBALCommunicator, GetCommProperty_WorldComm)
{
    auto worldComm = CreateDummyComm("prop_world", true);
    Communicator::gWorldCommunicator = worldComm;

    zbal_comm_property_t property;
    auto result = Communicator::GetCommProperty(worldComm.Get(), &property);
    EXPECT_EQ(result, Z_OK);
    EXPECT_EQ(property.groupSize, 0);
}

TEST_F(TestZBALCommunicator, GetCommProperty_LookupMap)
{
    auto worldComm = CreateDummyComm("prop_world2", true);
    Communicator::gWorldCommunicator = worldComm;

    auto subComm = CreateDummyComm("prop_sub", false);
    Communicator::gCommLookupMap.emplace(reinterpret_cast<uintptr_t>(subComm.Get()), subComm);
    Communicator::gCommLookupMapByName.emplace("prop_sub", subComm);

    zbal_comm_property_t property;
    auto result = Communicator::GetCommProperty(subComm.Get(), &property);
    EXPECT_EQ(result, Z_OK);
}

TEST_F(TestZBALCommunicator, GetCommProperty_NotFound)
{
    auto dummyComm = CreateDummyComm("dummy", false);

    zbal_comm_property_t property;
    auto result = Communicator::GetCommProperty(dummyComm.Get(), &property);
    EXPECT_EQ(result, Z_COMM_NOT_EXIST_BY_HANDLE);
}

TEST_F(TestZBALCommunicator, Count_Empty)
{
    EXPECT_EQ(Communicator::Count(), 0);
}

TEST_F(TestZBALCommunicator, Count_WithEntries)
{
    auto comm = CreateDummyComm("count_test", false);
    Communicator::gCommLookupMapByName.emplace("count_test", comm);

    EXPECT_EQ(Communicator::Count(), 1);

    auto comm2 = CreateDummyComm("count_test2", false);
    Communicator::gCommLookupMapByName.emplace("count_test2", comm2);

    EXPECT_EQ(Communicator::Count(), ZBAL_TEST_NUMBER_TWO);
}

TEST_F(TestZBALCommunicator, Lookup_NotFound)
{
    zbal_comm_t comm = nullptr;
    auto result = Communicator::Lookup("nonexistent", &comm);
    EXPECT_EQ(result, Z_COMM_NOT_EXIST_BY_NAME);
    EXPECT_TRUE(comm == nullptr);
}

TEST_F(TestZBALCommunicator, Lookup_Found)
{
    auto comm = CreateDummyComm("lookup_test", false);
    Communicator::gCommLookupMapByName.emplace("lookup_test", comm);

    zbal_comm_t found = nullptr;
    auto result = Communicator::Lookup("lookup_test", &found);
    EXPECT_EQ(result, Z_OK);
    EXPECT_EQ(found, comm.Get());
}

TEST_F(TestZBALCommunicator, Destroy_NullComm)
{
    auto result = Communicator::Destroy(nullptr, 0);
    EXPECT_EQ(result, Z_INVALID_PARAM);
}

TEST_F(TestZBALCommunicator, Destroy_NonWorldComm)
{
    auto worldComm = CreateDummyComm("destroy_world", true);
    Communicator::gWorldCommunicator = worldComm;

    auto subComm = CreateDummyComm("destroy_sub", false);
    Communicator::gCommLookupMap.emplace(reinterpret_cast<uintptr_t>(subComm.Get()), subComm);
    Communicator::gCommLookupMapByName.emplace("destroy_sub", subComm);

    EXPECT_EQ(Communicator::gCommLookupMap.size(), 1);
    EXPECT_EQ(Communicator::gCommLookupMapByName.size(), 1);

    auto result = Communicator::Destroy(subComm.Get(), 0);
    EXPECT_EQ(result, Z_OK);
    EXPECT_EQ(Communicator::gCommLookupMap.size(), 0);
    EXPECT_EQ(Communicator::gCommLookupMapByName.size(), 0);
}

TEST_F(TestZBALCommunicator, Destroy_WorldCommWithSubComms)
{
    auto worldComm = CreateDummyComm("destroy_world2", true);
    Communicator::gWorldCommunicator = worldComm;

    auto subComm = CreateDummyComm("destroy_sub2", false);
    Communicator::gCommLookupMap.emplace(reinterpret_cast<uintptr_t>(subComm.Get()), subComm);
    Communicator::gCommLookupMapByName.emplace("destroy_sub2", subComm);

    auto result = Communicator::Destroy(worldComm.Get(), 0);
    EXPECT_EQ(result, Z_COMM_DESTROY_GLOBAL_LAST);
}

TEST_F(TestZBALCommunicator, Destroy_WorldCommAlone)
{
    auto worldComm = CreateDummyComm("destroy_world3", true);
    Communicator::gWorldCommunicator = worldComm;
    Communicator::gCommLookupMapByName.emplace("destroy_world3", worldComm);

    EXPECT_TRUE(Communicator::gWorldCommunicator != nullptr);

    auto result = Communicator::Destroy(worldComm.Get(), 0);
    EXPECT_EQ(result, Z_OK);
    EXPECT_TRUE(Communicator::gWorldCommunicator == nullptr);
}

TEST_F(TestZBALCommunicator, Destroy_NotFound)
{
    auto dummyComm = CreateDummyComm("destroy_notfound", false);

    auto result = Communicator::Destroy(dummyComm.Get(), 0);
    EXPECT_EQ(result, Z_OK);
}

TEST_F(TestZBALCommunicator, DumpAllComm_Empty)
{
    Communicator::DumpAllComm();
}

TEST_F(TestZBALCommunicator, DumpAllComm_WithEntries)
{
    auto comm = CreateDummyComm("dump_test", false);
    Communicator::gCommLookupMapByName.emplace("dump_test", comm);

    Communicator::DumpAllComm();
}

TEST_F(TestZBALCommunicator, DestroyAll_WithWorldComm)
{
    auto worldComm = CreateDummyComm("destroyall_world", true);
    Communicator::gWorldCommunicator = worldComm;
    Communicator::gCommLookupMapByName.emplace("destroyall_world", worldComm);

    auto subComm = CreateDummyComm("destroyall_sub", false);
    Communicator::gCommLookupMap.emplace(reinterpret_cast<uintptr_t>(subComm.Get()), subComm);
    Communicator::gCommLookupMapByName.emplace("destroyall_sub", subComm);

    Communicator::DestroyAll();

    EXPECT_TRUE(Communicator::gWorldCommunicator == nullptr);
    EXPECT_EQ(Communicator::gCommLookupMap.size(), 0);
    EXPECT_EQ(Communicator::gCommLookupMapByName.size(), 0);
}

TEST_F(TestZBALCommunicator, DestroyAll_WithoutWorldComm)
{
    auto subComm = CreateDummyComm("destroyall_sub2", false);
    Communicator::gCommLookupMap.emplace(reinterpret_cast<uintptr_t>(subComm.Get()), subComm);
    Communicator::gCommLookupMapByName.emplace("destroyall_sub2", subComm);

    Communicator::DestroyAll();

    EXPECT_EQ(Communicator::gCommLookupMap.size(), 0);
    EXPECT_EQ(Communicator::gCommLookupMapByName.size(), 0);
}

TEST_F(TestZBALCommunicator, InlineGetters)
{
    CommGroupOptions options;
    options.name = "getter_test";
    options.worldSize = ZBAL_TEST_NUMBER_EIGHT;
    options.groupSize = ZBAL_TEST_NUMBER_FOUR;
    options.myWorldRank = ZBAL_TEST_NUMBER_THREE;
    options.myGroupRank = 1;

    auto comm = CommunicatorPtr(new CommunicatorDummy(options, true, Communicator::gWorldCommunicator));

    EXPECT_TRUE(comm->IsWorldGroup());
    EXPECT_EQ(comm->Name(), "getter_test");
    EXPECT_EQ(comm->GroupId(), UINT16_MAX);
    EXPECT_EQ(comm->GetMetaInfo().groupSize, 0);
}

TEST_F(TestZBALCommunicator, IsWorldGroup_False)
{
    CommGroupOptions options;
    options.name = "non_world";
    auto comm = CommunicatorPtr(new CommunicatorDummy(options, false, Communicator::gWorldCommunicator));

    EXPECT_FALSE(comm->IsWorldGroup());
}

TEST_F(TestZBALCommunicator, Destroy_WorldCommNotSetAsGlobal)
{
    auto worldComm = CreateDummyComm("world_not_global", true);
    Communicator::gCommLookupMapByName.emplace("world_not_global", worldComm);

    auto result = Communicator::Destroy(worldComm.Get(), 0);
    EXPECT_EQ(result, Z_OK);
}

TEST_F(TestZBALCommunicator, LookupInner_NullValueInMap)
{
    Communicator::gCommLookupMapByName.emplace("null_entry", CommunicatorPtr(nullptr));

    zbal_comm_t found = nullptr;
    auto result = Communicator::Lookup("null_entry", &found);
    EXPECT_EQ(result, Z_COMM_NOT_EXIST_BY_NAME);
}