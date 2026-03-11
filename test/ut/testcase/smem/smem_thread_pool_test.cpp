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
#include <semaphore.h>
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <memory>

#include "smem_thread_pool.h"
#include "smem_logger.h"

class ExecutorServiceTest : public testing::Test {
public:
    void SetUp() override
    {
        service = std::make_shared<ock::smem::ExecutorService>(1U, 128U);
        service->SetThreadName("tt");
        ASSERT_TRUE(service->Start());
    }

    void TearDown() override
    {
        service->Stop();
    }

protected:
    std::shared_ptr<ock::smem::ExecutorService> service;
};

TEST_F(ExecutorServiceTest, use_lambda_expression)
{
    struct timespec ts {};
    sem_t waitSem{};

    auto ret = clock_gettime(CLOCK_REALTIME, &ts);
    ASSERT_EQ(0, ret) << "get system time failed: " << errno << ": " << strerror(errno);

    ret = sem_init(&waitSem, 0, 0);
    ASSERT_EQ(0, ret) << "initialize sem failed: " << errno << ": " << strerror(errno);

    auto task = [&waitSem]() { sem_post(&waitSem); };
    auto success = service->Execute(task);
    EXPECT_TRUE(success);

    ts.tv_sec += 5U;
    ret = sem_timedwait(&waitSem, &ts);
    EXPECT_EQ(0, ret) << "wait sem failed: " << errno << ": " << strerror(errno);

    sem_destroy(&waitSem);
}

TEST_F(ExecutorServiceTest, task_run_count)
{
    auto taskCount = 32U;
    std::atomic<uint32_t> counter{0U};
    auto task = [&counter]() { counter.fetch_add(1U); };
    for (auto i = 0U; i < taskCount; i++) {
        auto success = service->Execute(task);
        EXPECT_TRUE(success) << "failed execute task : " << i;
    }
    service->Stop();
    ASSERT_EQ(taskCount, counter.load());
}

TEST_F(ExecutorServiceTest, task_queue_full)
{
    auto threadNum = 2U;
    auto queueCapacity = 32U;
    service->Stop();
    service = std::make_shared<ock::smem::ExecutorService>(threadNum, queueCapacity);
    ASSERT_TRUE(service->Start());

    std::atomic<uint32_t> startedCount{0U};
    std::atomic<bool> finished{false};
    auto initTask = [&finished, &startedCount]() {
        startedCount.fetch_add(1U);
        for (auto i = 0U; i < 10000U && !finished; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    for (auto i = 0U; i < threadNum; i++) {
        auto success = service->Execute(initTask);
        EXPECT_TRUE(success) << "failed execute init task : " << i;
    }

    uint32_t retryTimes = 0;
    while (startedCount.load() < 2U && ++retryTimes < 10000U) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::atomic<uint32_t> waitingRunCount{0U};
    auto waitingTask = [&waitingRunCount]() { waitingRunCount++; };
    for (auto i = 0U; i < queueCapacity; i++) {
        auto success = service->Execute(waitingTask);
        EXPECT_TRUE(success) << "failed execute waiting task : " << i;
    }

    auto success = service->Execute(waitingTask);
    EXPECT_FALSE(success);
    finished = true;
    service->Stop();
    EXPECT_EQ(queueCapacity, waitingRunCount.load());
}