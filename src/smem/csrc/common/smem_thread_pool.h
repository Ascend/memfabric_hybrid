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

#ifndef MEMFABRIC_HYBRID_SMEM_THREAD_POOL_H
#define MEMFABRIC_HYBRID_SMEM_THREAD_POOL_H

#include <cstdint>
#include <queue>
#include <mutex>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <functional>

namespace ock {
namespace smem {

enum class RunnableType {
    NORMAL = 0,
    STOP = 1,
};

class Runnable {
public:
    Runnable() : mTask{nullptr} {}
    explicit Runnable(const std::function<void()> &task) : mTask{task} {}
    virtual ~Runnable() = default;

    virtual void Run() const
    {
        if (mTask != nullptr) {
            mTask();
        }
    }

private:
    inline void Type(RunnableType type)
    {
        mType = type;
    }

    inline RunnableType Type() const
    {
        return mType;
    }

    RunnableType mType = RunnableType::NORMAL;
    std::function<void()> mTask;
    friend class ExecutorService;
};

class ExecutorService {
public:
    explicit ExecutorService(uint32_t threadNum, uint32_t queueCapacity = 10000U) noexcept;
    virtual ~ExecutorService() noexcept;

    bool Start();
    void Stop();
    bool Execute(const Runnable &runnable);
    bool Execute(const std::function<void()> &task);
    inline void SetThreadName(const std::string &name)
    {
        name_ = name;
    }

private:
    void RunInThread();
    void DoRunnable(const Runnable &runnable, bool &flag);
    void ClearExistWorkerThread();

private:
    const uint32_t threadNum_;
    const uint32_t maxWaitingTaskNum_;
    std::atomic<bool> started_;
    std::atomic<bool> stopped_;
    std::atomic<uint32_t> startedThreadNum_;
    std::queue<Runnable> tasks_;
    std::mutex tasksMutex_;
    std::condition_variable tasksCond_;
    std::vector<std::thread *> workerThreads_;
    std::string name_;
};
} // namespace smem
} // namespace ock

#endif // MEMFABRIC_HYBRID_SMEM_THREAD_POOL_H
