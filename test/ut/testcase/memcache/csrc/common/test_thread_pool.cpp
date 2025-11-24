/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include <iostream>
#include <thread>
#include <vector>
#include "gtest/gtest.h"
#include "mmc_ref.h"
#include "mmc_thread_pool.h"

using namespace testing;
using namespace std;
using namespace ock::mmc;

class TestMmcThreadPool : public testing::Test {
public:
    TestMmcThreadPool() {};

    void SetUp() override;

    void TearDown() override;

protected:
};

void TestMmcThreadPool::SetUp() {}

void TestMmcThreadPool::TearDown() {}

TEST_F(TestMmcThreadPool, MultiThreadTest)
{
    MmcThreadPool pool("mmc", 4);  // 创建4个线程的线程池
    auto ret = pool.Start();
    ASSERT_EQ(ret, MMC_OK);

    // 提交带返回值的任务
    auto future1 = pool.Enqueue([](int x) { return x * x; }, 5);
    auto future2 = pool.Enqueue([](const std::string& s) { return "Hello, " + s + "!"; }, "World");

    // 获取结果
    EXPECT_EQ(future1.get(), 25);
    EXPECT_EQ(future2.get(), "Hello, World!");

    // 提交无返回值的任务
    auto future3 = pool.Enqueue(
        [](int id) { std::cout << "Task " << id << " running on thread " << std::this_thread::get_id() << std::endl; },
        1);
    EXPECT_TRUE(future3.valid());

    pool.Shutdown();

    auto future4 = pool.Enqueue([](int id) { return id; }, 2);
    EXPECT_FALSE(future4.valid());
}
