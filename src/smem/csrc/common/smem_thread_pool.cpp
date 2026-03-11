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
#include "smem_logger.h"
#include "smem_thread_pool.h"

namespace ock {
namespace smem {
ExecutorService::ExecutorService(uint32_t threadNum, uint32_t queueCapacity) noexcept
    : threadNum_{threadNum}, maxWaitingTaskNum_{queueCapacity}, started_{false}, stopped_{false}, startedThreadNum_{0U}
{}

ExecutorService::~ExecutorService() noexcept
{
    if (!stopped_) {
        Stop();
    }
}

bool ExecutorService::Start()
{
    if (started_) {
        return true;
    }

    for (auto i = 0U; i < threadNum_; i++) {
        auto thr = new (std::nothrow) std::thread(&ExecutorService::RunInThread, this);
        if (thr == nullptr) {
            SM_LOG_ERROR("Failed to create executor thread " << i);
            ClearExistWorkerThread();
            return false;
        }

        workerThreads_.push_back(thr);
    }

    while (startedThreadNum_ < threadNum_) {
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
    started_ = true;
    return true;
}

void ExecutorService::Stop()
{
    if (!started_ || stopped_) {
        return;
    }

    ClearExistWorkerThread();
    stopped_ = true;
    started_ = false;
}

bool ExecutorService::Execute(const Runnable &runnable)
{
    std::unique_lock<std::mutex> locker{tasksMutex_};
    if (tasks_.size() >= maxWaitingTaskNum_) {
        return false;
    }
    tasks_.push(runnable);
    locker.unlock();
    tasksCond_.notify_one();
    return true;
}

bool ExecutorService::Execute(const std::function<void()> &task)
{
    return Execute(Runnable(task));
}

void ExecutorService::DoRunnable(const Runnable &runnable, bool &flag)
{
    try {
        if (runnable.Type() == RunnableType::STOP) {
            flag = false;
        } else {
            runnable.Run();
        }
    } catch (std::runtime_error &ex) {
        SM_LOG_ERROR("Caught error " << ex.what() << " when execute a task, continue");
    } catch (...) {
        SM_LOG_ERROR("Caught unknown error when execute a task, continue");
    }
}

void ExecutorService::ClearExistWorkerThread()
{
    Runnable stopTask;
    stopTask.Type(RunnableType::STOP);

    std::unique_lock<std::mutex> locker{tasksMutex_};
    for (auto i = 0U; i < workerThreads_.size(); i++) {
        tasks_.push(stopTask);
    }
    locker.unlock();
    tasksCond_.notify_all();

    for (auto &thr : workerThreads_) {
        if (thr != nullptr) {
            thr->join();
        }
    }

    startedThreadNum_ = 0;
    for (auto thr : workerThreads_) {
        delete thr;
    }
    workerThreads_.clear();
}

void ExecutorService::RunInThread()
{
    bool runFlag = true;
    auto index = startedThreadNum_++;
    auto threadName = name_.empty() ? "executor" : name_;
    threadName += std::to_string(index);
    pthread_setname_np(pthread_self(), threadName.c_str());
    SM_LOG_INFO("thread " << threadName << " started.");
    while (runFlag) {
        std::unique_lock<std::mutex> locker{tasksMutex_};
        while (tasks_.empty()) {
            tasksCond_.wait(locker);
        }
        Runnable task(std::move(tasks_.front()));
        tasks_.pop();
        locker.unlock();

        DoRunnable(task, runFlag);
    }
    SM_LOG_INFO("thread " << threadName << " finished.");
}
} // namespace smem
} // namespace ock