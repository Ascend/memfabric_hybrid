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

#include "zbal_communicator.h"
#include "zbal_comm_group_meta.h"

using namespace zbal;
using namespace zbal::operators;

constexpr uint16_t ZBAL_TEST_NUMBER_TWO = 2;
constexpr uint16_t ZBAL_TEST_NUMBER_FOUR = 4;
constexpr uint16_t ZBAL_TEST_SIZE_128 = 128;
constexpr uint16_t ZBAL_TEST_SIZE_512 = 512;
constexpr uint16_t ZBAL_TEST_SIZE_1KB = 1024;

class TestZBALCommunicator : public testing::Test {
public:
    static void SetUpTestCase() {}

    static void TearDownTestCase() {}

    void SetUp() override
    {
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

    void TearDown() override {}

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