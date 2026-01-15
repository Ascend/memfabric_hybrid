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
#ifndef ACC_LINKS_ACC_TCP_LINK_DELAY_CLEANUP_H
#define ACC_LINKS_ACC_TCP_LINK_DELAY_CLEANUP_H

#include <condition_variable>
#include <list>

#include "acc_def.h"
#include "acc_tcp_link.h"

namespace ock {
namespace acc {
struct AccTcpLinkCleanupItem {
    bool stop = false;
    struct timeval enqueueTime{};
    AccTcpLinkPtr link;

    AccTcpLinkCleanupItem() = default;

    explicit AccTcpLinkCleanupItem(const AccTcpLinkPtr &l) : link(l)
    {
        gettimeofday(&enqueueTime, nullptr);
    }
};

class AccTcpLinkDelayCleanup : public AccReferable {
public:
    ~AccTcpLinkDelayCleanup() override
    {
        Stop();
    }

    Result Start();
    void Stop(bool afterFork = false);

    void Enqueue(const AccTcpLinkPtr &link);

private:
    void RunInThread(std::atomic<bool> *started);

    bool CheckAndPop(uint32_t periodSecond, AccTcpLinkCleanupItem &item);

private:
    std::mutex mutex_;
    std::mutex queueMutex_;
    std::list<AccTcpLinkCleanupItem> queue_;
    std::atomic<bool> started_{false};
    std::atomic<bool> threadStarted_{false};
    std::thread cleanupThread_;
};
using AccTcpLinkDelayCleanupPtr = AccRef<AccTcpLinkDelayCleanup>;

inline void AccTcpLinkDelayCleanup::Enqueue(const AccTcpLinkPtr &link)
{
    std::lock_guard<std::mutex> guard(queueMutex_);
    queue_.emplace_back(link);
}

inline Result AccTcpLinkDelayCleanup::Start()
{
    bool expected = false;
    if (!started_.compare_exchange_strong(expected, true)) {
        return ACC_OK;
    }

    threadStarted_.store(false);
    std::thread tmpThread(&AccTcpLinkDelayCleanup::RunInThread, this, &threadStarted_);

    while (!threadStarted_.load()) {
        usleep(UNO_32);
    }

    cleanupThread_.swap(tmpThread);
    return ACC_OK;
}

inline void AccTcpLinkDelayCleanup::Stop(bool afterFork)
{
    bool expected = true;
    if (!started_.compare_exchange_strong(expected, false)) {
        return;
    }

    if (cleanupThread_.joinable()) {
        if (afterFork) {
            cleanupThread_.detach();
        } else {
            AccTcpLinkCleanupItem item;
            item.stop = true;
            {
                std::lock_guard<std::mutex> guardQueue(queueMutex_);
                queue_.emplace_front(item);
            }

            cleanupThread_.join();
        }
    }
    queue_.clear();
}

inline void AccTcpLinkDelayCleanup::RunInThread(std::atomic<bool> *started)
{
    pthread_setname_np(pthread_self(), "AccDelayClean");
    started->store(true);

    LOG_INFO("AccDelay cleanup thread thread started");

    AccTcpLinkCleanupItem item;
    bool stop = false;
    while (!stop) {
        auto gotItem = CheckAndPop(UNO_7, item);
        if (!gotItem) {
            sleep(UNO_1);
        } else if (item.stop) {
            stop = true;
        } else {
            item.link = nullptr;
        }
    }

    LOG_DEBUG("AccDelay cleanup thread thread exiting");
}

inline bool AccTcpLinkDelayCleanup::CheckAndPop(uint32_t periodSecond, AccTcpLinkCleanupItem &item)
{
    std::unique_lock<std::mutex> lk(queueMutex_);
    if (queue_.empty()) {
        return false;
    }

    auto &frontItem = queue_.front();
    if (frontItem.stop) {
        item = frontItem;
        return true;
    }

    struct timeval currentTime{};
    gettimeofday(&currentTime, nullptr);
    if (currentTime.tv_sec - frontItem.enqueueTime.tv_sec >= periodSecond) {
        item = frontItem;
        queue_.pop_front();
        return true;
    }

    return false;
}
} // namespace acc
} // namespace ock

#endif // ACC_LINKS_ACC_TCP_LINK_DELAY_CLEANUP_H
