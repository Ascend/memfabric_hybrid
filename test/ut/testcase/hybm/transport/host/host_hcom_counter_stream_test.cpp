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
#include <gtest/gtest.h>

#include "host_hcom_counter_stream.h"

using namespace ock::mf;

// SubmitTasks + FinishOne + Synchronize 的正常流程：无失败时应返回 0。
TEST(HostHcomCounterStreamTest, SubmitAndFinishThenSynchronizeOk)
{
    HostHcomCounterStream stream(0);

    stream.SubmitTasks(2);
    stream.FinishOne(); // notify = true
    stream.FinishOne(); // num_ 递减到 0，会唤醒等待者

    int ret = stream.Synchronize(0);
    EXPECT_EQ(ret, 0);
}

// 有 FailedOne（notify = true）时，Synchronize 应返回非 0。
TEST(HostHcomCounterStreamTest, FailedOneMakesSynchronizeReturnError)
{
    HostHcomCounterStream stream(0);

    stream.SubmitTasks(1);
    stream.FailedOne(); // failedCount_++，num_-- 到 0 并唤醒

    int ret = stream.Synchronize(0);
    EXPECT_NE(ret, 0);
}

// Reset 应清理计数和失败状态，使后续 Synchronize 再次返回 0。
TEST(HostHcomCounterStreamTest, ResetClearsState)
{
    HostHcomCounterStream stream(0);

    stream.SubmitTasks(1);
    stream.FailedOne(false); // failedCount_++，num_--，但不唤醒

    stream.Reset(); // num_ 和 failedCount_ 清零

    int ret = stream.Synchronize(0);
    EXPECT_EQ(ret, 0);
}