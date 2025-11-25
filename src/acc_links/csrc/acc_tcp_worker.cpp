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
#include <pthread.h>
#include <sys/resource.h>

#include "acc_tcp_worker.h"

namespace ock {
namespace acc {
Result AccTcpWorker::Start()
{
    bool expected = false;
    if (!started_.compare_exchange_strong(expected, true)) {
        return ACC_OK;
    }

    auto result = ValidateOptions();
    if (result != ACC_OK) {
        started_.store(false);
        return result;
    }

    if ((epollFD_ = epoll_create(8192L)) < 0) {
        LOG_ERROR("Failed to create epoll in worker " << options_.Name() << ", errno " << errno);
        started_.store(false);
        return ACC_EPOLL_ERROR;
    }

    threadStarted_.store(false);

    std::thread tmpThread(&AccTcpWorker::RunInThread, this, &threadStarted_);
    epollThread_ = std::move(tmpThread);

    while (!threadStarted_.load()) {
        usleep(UNO_32);
    }

    return ACC_OK;
}

void AccTcpWorker::Stop(bool afterFork)
{
    bool expected = true;
    if (!started_.compare_exchange_strong(expected, false)) {
        return;
    }

    StopInner(afterFork);
}

void AccTcpWorker::StopInner(bool afterFork)
{
    LOG_TRACE("Try to stop worker " << options_.Name());
    needStop_ = true;
    if (epollThread_.joinable()) {
        if (afterFork) {
            epollThread_.detach();
        } else {
            epollThread_.join();
        }
    }

    if (epollFD_ != -1) {
        SafeCloseFd(epollFD_, !afterFork);
    }
}

Result AccTcpWorker::AddLink(const AccTcpLinkComplexDefaultPtr &link, uint32_t events) noexcept
{
    ASSERT_RETURN(link.Get(), ACC_INVALID_PARAM);
    ASSERT_RETURN(link->fd_ != -1, ACC_INVALID_PARAM);

    struct epoll_event evNewFd {};
    evNewFd.data.ptr = link.Get();
    evNewFd.events = events;

    LOG_TRACE("Adding link " << link->ShortName() << " into sock worker " << options_.Name());

    if (UNLIKELY(epoll_ctl(epollFD_, EPOLL_CTL_ADD, link->fd_, &evNewFd) != 0)) {
        LOG_ERROR("Failed to add link " << link->ShortName() << " into worker " << options_.Name() << ", errno "
                                        << errno);
        return ACC_EPOLL_ERROR;
    }

    link->IncreaseRef(); /* increase ref and remove ref when remove */
    return ACC_OK;
}

Result AccTcpWorker::RemoveLink(const AccTcpLinkComplexDefaultPtr &link) noexcept
{
    ASSERT_RETURN(link.Get(), ACC_INVALID_PARAM);
    ASSERT_RETURN(link->fd_ != -1, ACC_INVALID_PARAM);

    LOG_TRACE("Try to modify link " << link->ShortName() << " in sock worker " << options_.Name());

    if (UNLIKELY(epoll_ctl(epollFD_, EPOLL_CTL_DEL, link->fd_, nullptr) != 0)) {
        LOG_ERROR("Failed to remove " << link->ShortName() << " from sock worker " << options_.Name()
                                      << ", errno:" << errno);
        return ACC_EPOLL_ERROR;
    }

    link->DecreaseRef(); /* decrease ref as increased in add */
    return ACC_OK;
}

Result AccTcpWorker::ValidateOptions()
{
    ASSERT_RETURN(newRequestHandle_ != nullptr, ACC_INVALID_PARAM);
    ASSERT_RETURN(requestSentHandle_ != nullptr, ACC_INVALID_PARAM);
    ASSERT_RETURN(linkBrokenHandle_ != nullptr, ACC_INVALID_PARAM);

    if (options_.name_.empty()) {
        LOG_ERROR("Invalid options, name is empty");
        return ACC_INVALID_PARAM;
    }

    return ACC_OK;
}

void AccTcpWorker::SetPropertiesForThread()
{
    cpu_set_t cpuSet;
    if (options_.cpuId != -1) {
        CPU_ZERO(&(cpuSet));
        CPU_SET(options_.cpuId, &(cpuSet));
        if (pthread_setaffinity_np(pthread_self(), sizeof(cpuSet), &(cpuSet)) != 0) {
            LOG_WARN("Failed to bind worker " << options_.Name() << " to cpu " << options_.cpuId);
        }
    }
    
    /* set thread name */
    pthread_setname_np(pthread_self(), options_.Name().c_str());

    if (options_.threadPriority != 0) {
        if (setpriority(PRIO_PROCESS, 0, options_.threadPriority) != 0) {
            LOG_WARN("Failed to set thread priority of worker " << options_.Name() << ", errno:" << errno);
        }
    }
}

void AccTcpWorker::RunInThread(std::atomic<bool> *started)
{
    SetPropertiesForThread();
    started->store(true);
    LOG_INFO("Worker [" << options_.ToString() << "] progress thread started");

    const uint16_t pollBatchSize = 16L;
    const uint32_t timeout = options_.pollingTimeoutMs;

    struct epoll_event ev[pollBatchSize];

    while (!needStop_) {
        /* do epoll wait with timeout */
        int count = epoll_wait(epollFD_, ev, pollBatchSize, timeout);
        if (count > 0) {
            /* there are events, handle it */
            LOG_TRACE("Got " << count << " in worker " << mName);
            for (uint16_t i = 0; i < static_cast<uint16_t>(count); ++i) {
                struct epoll_event &oneEv = (ev)[i];
                ProcessEvent(oneEv);
            }
        } else if (count == 0) {
            LOG_TRACE("Got " << count << " in worker " << mName);
            continue;
        } else if (errno == EINTR) {
            LOG_TRACE("Got error no EINTR in worker " << options_.Name());
            continue;
        } else {
            LOG_ERROR("Failed to do epoll_wait in worker " << options_.Name() << ", errno:" << errno);
            break;
        }
    }

    LOG_INFO("Worker " << options_.Name() << " progress thread exiting");
}
}  // namespace acc
}  // namespace ock