/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
* MemFabric_Hybrid is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include <gtest/gtest.h>
#include "smem_shm.h"
#include "smem_types.h"
#include "ut_barrier_util.h"
#include "smem_net_common.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace ock::smem;

class SmemNetTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp() override;
    void TearDown() override;
};

void SmemNetTest::SetUpTestCase() {}

void SmemNetTest::TearDownTestCase() {}

void SmemNetTest::SetUp()
{
    GlobalMockObject::reset();
}

void SmemNetTest::TearDown()
{
    GlobalMockObject::verify();
    GlobalMockObject::reset();
}

TEST_F(SmemNetTest, GetLocalIpWithTarget)
{
    std::string target{};
    auto ret = GetLocalIpWithTarget("::1", target);
    EXPECT_EQ(ret, 0);
}