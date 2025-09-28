/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef __MEMFABRIC_HYBRID_MMC_THREAD_POOL_H__
#define __MEMFABRIC_HYBRID_MMC_THREAD_POOL_H__

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <stdexcept>
#include <type_traits>
#include <pthread.h>
#include <string>
#include "mmc_logger.h"
#include "mmc_types.h"
#include "mmc_ref.h"

namespace ock {
namespace mmc {
// 兼容 C++11/C++14
#if __cplusplus < 201703L
template<typename F, typename... Args>
using invoke_result_t = typename std::result_of<F(Args...)>::type;
#else
template<typename F, typename... Args>
using invoke_result_t = typename std::invoke_result<F, Args...>::type;
#endif

class MmcThreadPool  : public MmcReferable {
public:
    MmcThreadPool(std::string name, size_t numThreads) : poolName_(name), numThreads_(numThreads), stop_(false) {}

    int32_t Start()
    {
        if (numThreads_ == 0 || numThreads_ > MMC_THREAD_POOL_MAX_THREADS) {
            MMC_LOG_ERROR("Number of threads must be greater than 0 and less than " << MMC_THREAD_POOL_MAX_THREADS);
            return MMC_ERROR;
        }
        for (size_t i = 0; i < numThreads_; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queueMutex_);
                        condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) {
                            break;
                        }
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
                MMC_LOG_INFO("worker thread :" << std::this_thread::get_id() << " exit");
            });

            std::string threadName = poolName_ + std::to_string(i);
            int ret = pthread_setname_np(workers_.back().native_handle(), threadName.c_str());
            if (ret != 0) {
                MMC_LOG_ERROR("set thread name failed, i:" << i << ", ret:" << ret);
            }
        }
        return MMC_OK;
    }

    template<typename F, typename... Args>
    auto Enqueue(F&& f, Args&&... args) -> std::future<invoke_result_t<F, Args...>>
    {
        using return_type = invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<return_type> result = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            if (stop_) {
                MMC_LOG_ERROR("thread pool stopped");
                return std::future<return_type>{}; // 返回无效的 future
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        condition_.notify_one();
        return result;
    }

    void Shutdown()
    {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    ~MmcThreadPool() { Shutdown(); }

    MmcThreadPool(const MmcThreadPool&) = delete;
    MmcThreadPool& operator=(const MmcThreadPool&) = delete;

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
    std::string poolName_;
    size_t numThreads_;
    bool stop_;
};

using MmcThreadPoolPtr = MmcRef<MmcThreadPool>;
}  // namespace mmc
}  // namespace ock

#endif