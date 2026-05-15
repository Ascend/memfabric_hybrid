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
#include <thread>
#include <vector>

#include "zbal_init_state.h"

using namespace zbal;

class TestZBALInitState : public testing::Test {
public:
    void SetUp() override
    {
        ZBALInitState::Instance().Reset();
    }

    void TearDown() override
    {
        ZBALInitState::Instance().Reset();
    }
};

/*
 * Reset truly zeroes all state including ext_ fields.
 */
TEST_F(TestZBALInitState, ResetClearsAllState)
{
    auto &state = ZBALInitState::Instance();
    constexpr uint16_t worldSize = 8;
    constexpr uint16_t worldRankId = 3;
    constexpr uint16_t deviceId = 2;
    state.Bootstrapped(true);
    state.SmaInitialized(true);
    state.CommunicatorCreated(5);
    state.ext_.worldSize = worldSize;
    state.ext_.worldRankId = worldRankId;
    state.ext_.deviceId = deviceId;
    state.ext_.commMetaSpaceSize = 512;
    state.ext_.commGroupCap = 128;
    state.ext_.localDeviceMemSize = 1024 * 1024;

    state.Reset();

    EXPECT_FALSE(state.Bootstrapped());
    EXPECT_FALSE(state.SmaInitialized());
    EXPECT_FALSE(state.HasCommunicator());
    EXPECT_EQ(state.ext_.worldSize, 0);
    EXPECT_EQ(state.ext_.worldRankId, 0);
    EXPECT_EQ(state.ext_.deviceId, 0);
}

/*
 * Full lifecycle: bootstrap -> create comms -> destroy comms -> reset.
 */
TEST_F(TestZBALInitState, FullLifecycle)
{
    auto &state = ZBALInitState::Instance();

    state.Bootstrapped(true);
    EXPECT_TRUE(state.Bootstrapped());

    state.SmaInitialized(true);
    EXPECT_TRUE(state.SmaInitialized());

    state.CommunicatorCreated();
    state.CommunicatorCreated(3);
    EXPECT_TRUE(state.HasCommunicator());

    state.CommunicatorDestroy();
    state.CommunicatorDestroy(2);
    state.CommunicatorDestroy();
    EXPECT_FALSE(state.HasCommunicator());

    EXPECT_TRUE(state.Bootstrapped());
    EXPECT_TRUE(state.SmaInitialized());

    state.Reset();
    EXPECT_FALSE(state.Bootstrapped());
}

/*
 * CommunicatorDestroy when count is already 0: goes negative,
 * HasCommunicator returns false (> 0 check).
 */
TEST_F(TestZBALInitState, DestroyBelowZero)
{
    auto &state = ZBALInitState::Instance();

    state.CommunicatorDestroy();
    state.CommunicatorDestroy();
    EXPECT_FALSE(state.HasCommunicator());
}

/*
 * Concurrent CommunicatorCreated/CommunicatorDestroy from multiple threads
 * verifies atomic counter integrity.
 */
TEST_F(TestZBALInitState, ConcurrentCommunicatorOps)
{
    auto &state = ZBALInitState::Instance();
    constexpr int kNumThreads = 4;
    constexpr int kOpsPerThread = 1000;

    std::vector<std::thread> threads;
    for (int i = 0; i < kNumThreads; i++) {
        threads.emplace_back([&state]() {
            for (int j = 0; j < kOpsPerThread; j++) {
                state.CommunicatorCreated();
            }
            for (int j = 0; j < kOpsPerThread; j++) {
                state.CommunicatorDestroy();
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    EXPECT_FALSE(state.HasCommunicator());
}

/*
 * Bootstrap flag is independent of communicator state.
 */
TEST_F(TestZBALInitState, BootstrapIndependentOfCommunicator)
{
    auto &state = ZBALInitState::Instance();

    state.Bootstrapped(true);
    EXPECT_FALSE(state.HasCommunicator());

    state.CommunicatorCreated();
    state.Bootstrapped(false);
    EXPECT_TRUE(state.HasCommunicator());
    EXPECT_FALSE(state.Bootstrapped());
}
