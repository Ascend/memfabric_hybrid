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

#define private public
#include "dl_cann_api.h"
#include "zbal_sma_device_pool.h"
#undef private

#include "test_zbal_def.h"
#include "zbal_sma_device.h"

using namespace zbal;
using namespace zbal::sma;
using namespace zbal::sma::device;

static int stubAclrtSynchronizeEventSuccess(void *)
{
    return 0;
}

class TestEventController : public testing::Test {
protected:
    void SetUp() override
    {
        zbal::underapi::DlCannApi::pAclrtSynchronizeEvent = nullptr;
    }

    void TearDown() override
    {
        zbal::underapi::DlCannApi::pAclrtSynchronizeEvent = nullptr;
    }

    DeviceSMACachingAllocator allocator_;
    EventController controller_;
    DeviceBlockPool pool_;
};

/* ================================================================
* EventController::get
* ================================================================ */

TEST_F(TestEventController, Get_BasicAndCache)
{
    auto e0 = controller_.get(0);
    EXPECT_NE(e0.get(), nullptr);
    EXPECT_EQ(controller_.pools_[0].event_pool_.size(), 0u);

    auto e1 = controller_.get(1);
    EXPECT_NE(e1.get(), nullptr);
    EXPECT_NE(e0.get(), e1.get());
}

TEST_F(TestEventController, Get_CacheHit)
{
    {
        auto e = controller_.get(0);
    }
    EXPECT_EQ(controller_.pools_[0].event_pool_.size(), 1u);

    auto e = controller_.get(0);
    EXPECT_NE(e.get(), nullptr);
    EXPECT_EQ(controller_.pools_[0].event_pool_.size(), 0u);
}

TEST_F(TestEventController, Get_InvalidDevice)
{
    EXPECT_THROW(controller_.get(-1), std::runtime_error);

    int deviceCount = static_cast<int>(controller_.pools_.size());
    EXPECT_THROW(controller_.get(deviceCount), std::runtime_error);
}

/* ================================================================
* EventController::emptyCache
* ================================================================ */

TEST_F(TestEventController, EmptyCache)
{
    std::vector<ZEvent> events;
    for (int i = 0; i < ZBAL_TEST_NUMBER_THREE; i++) {
        events.push_back(controller_.get(0));
    }
    EXPECT_EQ(controller_.pools_[0].event_pool_.size(), 0u);

    events.clear();
    EXPECT_EQ(controller_.pools_[0].event_pool_.size(), 3u);

    controller_.emptyCache();
    EXPECT_EQ(controller_.pools_[0].event_pool_.size(), 0u);
}

/* ================================================================
* EventController::synchronizeAndFreeEvents
* ================================================================ */

TEST_F(TestEventController, SynchronizeAndFreeEvents)
{
    EXPECT_NO_THROW(controller_.synchronizeAndFreeEvents(&allocator_, false, nullptr));

    DeviceBlock block(0, nullptr, ZBAL_TEST_SIZE_1KB, &pool_, nullptr, BT_SMALL);
    block.allocated_ = false;
    block.event_count_ = 1;
    c10_npu::NPUStream stream0(0);
    c10_npu::NPUStream stream1(1);
    block.stream_uses_.insert(stream0);
    block.stream_uses_.insert(stream1);

    controller_.insertEvents(&allocator_, &block);
    EXPECT_EQ(block.event_count_, ZBAL_TEST_NUMBER_THREE);

    controller_.synchronizeAndFreeEvents(&allocator_, false, nullptr);

    EXPECT_TRUE(controller_.npu_events_.empty());
    EXPECT_EQ(block.event_count_, 1);
}

TEST_F(TestEventController, SynchronizeAndFreeEvents_AclrtSuccess)
{
    zbal::underapi::DlCannApi::pAclrtSynchronizeEvent = stubAclrtSynchronizeEventSuccess;

    DeviceBlock block(0, nullptr, ZBAL_TEST_SIZE_1KB, &pool_, nullptr, BT_SMALL);
    block.allocated_ = false;
    c10_npu::NPUStream stream(0);
    block.stream_uses_.insert(stream);

    controller_.insertEvents(&allocator_, &block);
    EXPECT_EQ(block.event_count_, 1);

    controller_.synchronizeAndFreeEvents(&allocator_, false, nullptr);

    EXPECT_TRUE(controller_.npu_events_.empty());
    EXPECT_EQ(block.event_count_, 0);
}

/* ================================================================
* EventController::processEvents
* ================================================================ */

TEST_F(TestEventController, ProcessEvents_QueryTrue)
{
    EXPECT_NO_THROW(controller_.processEvents(&allocator_, nullptr));

    DeviceBlock block(0, nullptr, ZBAL_TEST_SIZE_1KB, &pool_, nullptr, BT_SMALL);
    block.allocated_ = false;
    block.event_count_ = 1;
    c10_npu::NPUStream stream0(0);
    c10_npu::NPUStream stream1(1);
    block.stream_uses_.insert(stream0);
    block.stream_uses_.insert(stream1);

    controller_.insertEvents(&allocator_, &block);
    EXPECT_EQ(block.event_count_, ZBAL_TEST_NUMBER_THREE);

    controller_.processEvents(&allocator_, nullptr);

    EXPECT_TRUE(controller_.npu_events_.empty());
    EXPECT_EQ(block.event_count_, 1);
}

/* ================================================================
* EventController::cleanEvents
* ================================================================ */

TEST_F(TestEventController, CleanEvents)
{
    EXPECT_NO_THROW(controller_.cleanEvents(&allocator_));

    DeviceBlock block(0, nullptr, ZBAL_TEST_SIZE_1KB, &pool_, nullptr, BT_SMALL);
    block.allocated_ = false;
    block.event_count_ = 1;
    c10_npu::NPUStream stream0(0);
    c10_npu::NPUStream stream1(1);
    block.stream_uses_.insert(stream0);
    block.stream_uses_.insert(stream1);

    controller_.insertEvents(&allocator_, &block);
    EXPECT_EQ(block.event_count_, ZBAL_TEST_NUMBER_THREE);

    controller_.cleanEvents(&allocator_);

    EXPECT_TRUE(controller_.npu_events_.empty());
    EXPECT_EQ(block.event_count_, 1);
}

/* ================================================================
* EventController::cleanStream
* ================================================================ */

TEST_F(TestEventController, CleanStream)
{
    DeviceBlock block1(0, nullptr, ZBAL_TEST_SIZE_1KB, &pool_, nullptr, BT_SMALL);
    DeviceBlock block2(0, nullptr, ZBAL_TEST_SIZE_2KB, &pool_, nullptr, BT_SMALL);
    block1.allocated_ = false;
    block2.allocated_ = false;
    c10_npu::NPUStream stream0(0);
    c10_npu::NPUStream stream1(1);
    block1.stream_uses_.insert(stream0);
    block2.stream_uses_.insert(stream1);

    controller_.insertEvents(&allocator_, &block1);
    controller_.insertEvents(&allocator_, &block2);
    EXPECT_EQ(block1.event_count_, 1);
    EXPECT_EQ(block2.event_count_, 1);

    controller_.cleanStream(&allocator_, &block1, stream1);

    EXPECT_EQ(controller_.npu_events_[stream0].size(), 1u);
    EXPECT_EQ(controller_.npu_events_[stream1].size(), 1u);
    EXPECT_EQ(block1.event_count_, 1);
    EXPECT_EQ(block2.event_count_, 1);

    controller_.cleanStream(&allocator_, &block1, stream0);

    EXPECT_EQ(controller_.npu_events_[stream0].size(), 0u);
    EXPECT_EQ(controller_.npu_events_[stream1].size(), 1u);
    EXPECT_EQ(block1.event_count_, 0);
    EXPECT_EQ(block2.event_count_, 1);
}

TEST_F(TestEventController, CleanStream_MultiEventSameBlock)
{
    DeviceBlock block(0, nullptr, ZBAL_TEST_SIZE_1KB, &pool_, nullptr, BT_SMALL);
    block.allocated_ = false;
    block.event_count_ = 1;
    c10_npu::NPUStream stream(0);

    block.stream_uses_.insert(stream);
    controller_.insertEvents(&allocator_, &block);
    block.stream_uses_.insert(stream);
    controller_.insertEvents(&allocator_, &block);
    EXPECT_EQ(block.event_count_, ZBAL_TEST_NUMBER_THREE);
    EXPECT_EQ(controller_.npu_events_[stream].size(), 2u);

    controller_.cleanStream(&allocator_, &block, stream);

    EXPECT_EQ(controller_.npu_events_[stream].size(), 0u);
    EXPECT_EQ(block.event_count_, 1);
}