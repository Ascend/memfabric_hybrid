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

constexpr uint16_t ZBAL_TEST_NUMBER_TWO = 2;
constexpr uint16_t ZBAL_TEST_NUMBER_FOUR = 4;
constexpr uint16_t ZBAL_TEST_SIZE_128 = 128;
constexpr uint16_t ZBAL_TEST_SIZE_512 = 512;
constexpr uint16_t ZBAL_TEST_SIZE_1KB = 1024;

class TestZBALCommGroupMeta : public testing::Test {
public:
    static void SetUpTestCase() {}

    static void TearDownTestCase() {}

    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(TestZBALCommGroupMeta, Initialization)
{
    ZBALInitStateExt stateExt;
    stateExt.commMetaSpaceSize = ZBAL_TEST_SIZE_512;
    stateExt.commGroupCap = ZBAL_TEST_SIZE_128;
    stateExt.myCommMetaDeviceGva = reinterpret_cast<void *>(ZBAL_TEST_SIZE_1KB);
    stateExt.metaSizeOfDevice = ZBAL_TEST_SIZE_1KB;

    auto &arranger = GroupMetaArranger::Instance();
    /* case1: meta space is not enough */
    auto result = arranger.Initialize(stateExt);
    EXPECT_TRUE(result != Z_OK);

    /* case2: meta space is just ok */
    stateExt.metaSizeOfDevice = ZBAL_TEST_SIZE_512 * ZBAL_TEST_SIZE_1KB * ZBAL_TEST_SIZE_128; /* 512KB * 128 */
    result = arranger.Initialize(stateExt);
    EXPECT_TRUE(result == Z_OK);

    result = arranger.Initialize(stateExt);
    EXPECT_TRUE(result == Z_OK);

    arranger.UnInitialize();

    /* case3: meta space is larger a little bit */
    stateExt.metaSizeOfDevice = ZBAL_TEST_SIZE_512 * ZBAL_TEST_SIZE_1KB * ZBAL_TEST_SIZE_128 + 1; /* 512KB * 128 */
    result = arranger.Initialize(stateExt);
    EXPECT_TRUE(result == Z_OK);
}

TEST_F(TestZBALCommGroupMeta, GetGroupIndex)
{
    ZBALInitStateExt stateExt;
    uintptr_t baseAddress = 1024;
    stateExt.commMetaSpaceSize = ZBAL_TEST_SIZE_512;
    stateExt.commGroupCap = ZBAL_TEST_NUMBER_TWO;
    stateExt.myCommMetaDeviceGva = reinterpret_cast<void *>(baseAddress);
    stateExt.metaSizeOfDevice = stateExt.commMetaSpaceSize * ZBAL_TEST_SIZE_1KB * stateExt.commGroupCap;

    auto &arranger = GroupMetaArranger::Instance();
    arranger.UnInitialize();
    auto result = arranger.Initialize(stateExt);
    EXPECT_TRUE(result == Z_OK);

    uint32_t index = 100;
    uintptr_t address = 0;
    uintptr_t addressParam = 0;
    uintptr_t addressExchange = 0;
    uintptr_t addressProfiling = 0;
    /* get one */
    result = arranger.CurrentGroup(index, address, addressParam, addressExchange, addressProfiling);
    EXPECT_TRUE(result == Z_OK);
    EXPECT_TRUE(index == 0);
    EXPECT_TRUE(address == baseAddress);
    EXPECT_TRUE(addressParam == (baseAddress + sizeof(CommGroupInfo)));
    EXPECT_TRUE(addressExchange == (baseAddress + ZBAL_OPERATE_PARAM_SIZE));

    arranger.Move2NextGroup();

    /* get two */
    result = arranger.CurrentGroup(index, address, addressParam, addressExchange, addressProfiling);
    EXPECT_TRUE(result == Z_OK);
    EXPECT_TRUE(index == 1);
    auto metaSpaceSizeInBytes = stateExt.commMetaSpaceSize * 1024;
    EXPECT_TRUE(address == (baseAddress + metaSpaceSizeInBytes));
    EXPECT_TRUE(addressParam == (baseAddress + metaSpaceSizeInBytes + sizeof(CommGroupInfo)));
    EXPECT_TRUE(addressExchange == (baseAddress + metaSpaceSizeInBytes + ZBAL_OPERATE_PARAM_SIZE));

    arranger.Move2NextGroup();

    result = arranger.CurrentGroup(index, address, addressParam, addressExchange, addressProfiling);
    EXPECT_TRUE(result != Z_OK);
}

TEST_F(TestZBALCommGroupMeta, GetSpaceSize)
{
    ZBALInitStateExt stateExt;
    uintptr_t baseAddress = 1024;
    stateExt.commMetaSpaceSize = ZBAL_TEST_SIZE_512;
    stateExt.commGroupCap = ZBAL_TEST_NUMBER_TWO;
    stateExt.myCommMetaDeviceGva = reinterpret_cast<void *>(baseAddress);
    stateExt.metaSizeOfDevice = stateExt.commMetaSpaceSize * ZBAL_TEST_SIZE_1KB * stateExt.commGroupCap;

    auto &arranger = GroupMetaArranger::Instance();
    arranger.UnInitialize();
    auto result = arranger.Initialize(stateExt);
    EXPECT_TRUE(result == Z_OK);

    EXPECT_TRUE(arranger.GetExchangeSpaceSize() ==
                (stateExt.commMetaSpaceSize * ZBAL_TEST_SIZE_1KB - ZBAL_OPERATE_PARAM_SIZE));
    EXPECT_TRUE(arranger.GetSingleMetaSpaceSize() == stateExt.commMetaSpaceSize * ZBAL_TEST_SIZE_1KB);
    EXPECT_TRUE(arranger.GetParamSpaceSize() == (ZBAL_OPERATE_PARAM_SIZE - sizeof(CommGroupInfo)));
    EXPECT_TRUE(arranger.GetCommGroupInfoSpaceSize() == sizeof(CommGroupInfo));
}
