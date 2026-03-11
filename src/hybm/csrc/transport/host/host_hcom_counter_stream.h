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
#ifndef MEMFABRIC_HYBRID_HCOM_WAITER_H
#define MEMFABRIC_HYBRID_HCOM_WAITER_H

#include <mutex>
#include <atomic>
#include <condition_variable>
#include "hybm_types.h"

namespace ock {
namespace mf {
class HostHcomCounterStream {
public:
    explicit HostHcomCounterStream(const int32_t num) : num_{num} {}

    void FinishOne(bool notify = true);
    void FailedOne(bool notify = true);
    void SubmitTasks(int32_t taskNum = 1);
    void Abort();
    void Reset();
    int32_t Synchronize(int32_t task);

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int32_t num_;
    int32_t failedCount_{0};
};

using HcomCounterStreamPtr = std::shared_ptr<HostHcomCounterStream>;

inline void HostHcomCounterStream::FinishOne(const bool notify)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (!notify) {
        num_--;
        return;
    }
    if (--num_ <= 0) {
        cv_.notify_all();
    }
}

inline void HostHcomCounterStream::FailedOne(bool notify)
{
    std::unique_lock<std::mutex> lock(mutex_);
    failedCount_++;
    if (!notify) {
        num_--;
        return;
    }
    if (--num_ <= 0) {
        cv_.notify_all();
    }
}

inline void HostHcomCounterStream::SubmitTasks(int32_t taskNum)
{
    std::unique_lock<std::mutex> lock(mutex_);
    num_ += taskNum;
}

inline void HostHcomCounterStream::Reset()
{
    std::unique_lock<std::mutex> lock(mutex_);
    num_ = 0;
    failedCount_ = 0;
}

inline void HostHcomCounterStream::Abort()
{
    cv_.notify_all();
}

inline int32_t HostHcomCounterStream::Synchronize(int32_t task)
{
    (void)task;
    int32_t result = 0;
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return num_ <= 0; });
    if (failedCount_ > 0) {
        result = BM_ERROR;
    }
    failedCount_ = 0;
    num_ = 0;
    return result;
}
}
}
#endif // MEMFABRIC_HYBRID_HCOM_WAITER_H