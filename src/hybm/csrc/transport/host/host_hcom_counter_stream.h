/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_HCOM_WAITER_H
#define MEMFABRIC_HYBRID_HCOM_WAITER_H

#include <mutex>
#include <atomic>
#include <condition_variable>

class HostHcomCounterStream {
public:
    explicit HostHcomCounterStream(const int32_t num) : num_{num} {}

    void FinishOne(bool notify = true);
    void SubmitTasks(int32_t taskNum = 1);
    void Abort();
    void Reset();
    void Synchronize(int32_t task);

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int32_t num_;
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

inline void HostHcomCounterStream::SubmitTasks(int32_t taskNum)
{
    std::unique_lock<std::mutex> lock(mutex_);
    num_ += taskNum;
}

inline void HostHcomCounterStream::Reset()
{
    std::unique_lock<std::mutex> lock(mutex_);
    num_ = 0;
}

inline void HostHcomCounterStream::Abort()
{
    cv_.notify_all();
}

inline void HostHcomCounterStream::Synchronize(int32_t task)
{
    (void)task;
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return num_ <= 0; });
    num_ = 0;
}

#endif // MEMFABRIC_HYBRID_HCOM_WAITER_H