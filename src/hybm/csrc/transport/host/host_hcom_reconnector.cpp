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
#include <cmath>
#include "hybm_logger.h"
#include "host_hcom_reconnector.h"

namespace ock {
namespace mf {
namespace transport {
namespace host {
HcomReconnector::HcomReconnector(int64_t minMs, int64_t maxMs) noexcept
    : baseTimePoint_{std::chrono::steady_clock::now()}, minWaitMs_{minMs}, maxWaitMs_{maxMs}, started_{false}
{}

Result HcomReconnector::Start(ReconnFunc reconnFunc) noexcept
{
    if (reconnFunc == nullptr) {
        BM_LOG_ERROR("input reconnect function is null.");
        return BM_INVALID_PARAM;
    }

    if (started_) {
        BM_LOG_WARN("HcomReconnector already started");
        return BM_OK;
    }

    reconnFunc_ = reconnFunc;
    started_ = true;
    reconnectThread_ = std::thread([this]() { ReconnectLoop(); });
    return BM_OK;
}

void HcomReconnector::Stop() noexcept
{
    if (!started_) {
        return;
    }
    started_ = false;
    loopWaitCond_.notify_one();
    if (reconnectThread_.joinable()) {
        reconnectThread_.join();
    }
}

void HcomReconnector::AddRank(uint32_t rankId) noexcept
{
    std::unique_lock<std::mutex> locker{mapMutex_};
    ranks_.emplace(rankId);
}

void HcomReconnector::AddRanks(const std::vector<uint32_t> &ranks) noexcept
{
    std::unique_lock<std::mutex> locker{mapMutex_};
    ranks_.insert(ranks.begin(), ranks.end());
}

void HcomReconnector::RemoveRank(uint32_t rankId) noexcept
{
    std::unique_lock<std::mutex> locker{mapMutex_};
    ranks_.erase(rankId);
}

void HcomReconnector::RemoveRanks(const std::vector<uint32_t> &ranks) noexcept
{
    std::unique_lock<std::mutex> locker{mapMutex_};
    for (auto rank : ranks) {
        ranks_.erase(rank);
    }
}

Result HcomReconnector::AddReconnectTask(uint32_t rankId, const std::string &nic) noexcept
{
    auto nextTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(minWaitMs_);
    auto timeDifMs = std::chrono::duration_cast<std::chrono::milliseconds>(nextTime - baseTimePoint_).count();
    ReconnectTask task{rankId, nic, timeDifMs};

    if (!started_) {
        BM_LOG_ERROR("HcomReconnector not started.");
        return BM_NOT_INITIALIZED;
    }

    std::unique_lock<std::mutex> locker{mapMutex_};
    if (ranks_.find(rankId) == ranks_.end()) {
        locker.unlock();
        BM_LOG_INFO("rank : " << rankId << " removed, no need reconnect.");
        return BM_INVALID_PARAM;
    }

    taskMap_.emplace(ReconnectTaskKey{timeDifMs, rankId}, std::move(task));
    locker.unlock();
    BM_LOG_INFO("rank : " << rankId << " reconnect task added.");
    return BM_OK;
}

void HcomReconnector::ReconnectLoop() noexcept
{
    pthread_setname_np(pthread_self(), "host_re_conn");
    BM_LOG_INFO("start thread for host_re_conn");
    while (started_) {
        std::unique_lock<std::mutex> locker{loopWaitMutex_};
        loopWaitCond_.wait_for(locker, std::chrono::milliseconds(minWaitMs_), [this]() { return !started_.load(); });
        locker.unlock();

        if (!started_) {
            break;
        }
        ReconnectTimeoutTasks();
    }
    BM_LOG_INFO("end thread for host_re_conn");
}

void HcomReconnector::ReconnectTimeoutTasks() noexcept
{
    auto now = std::chrono::steady_clock::now();
    auto nowDifMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - baseTimePoint_).count();
    ReconnectTaskKey boundKey{nowDifMs, 0};

    std::vector<ReconnectTask> timeoutTasks;
    {
        std::unique_lock<std::mutex> locker{mapMutex_};
        timeoutTasks.reserve(ranks_.size());
        auto endIt = taskMap_.upper_bound(boundKey);
        for (auto it = taskMap_.begin(); it != endIt; ++it) {
            auto &&task = std::move(it->second);
            if (ranks_.find(task.rankId) != ranks_.end()) {
                timeoutTasks.emplace_back(std::move(task));
            }
        }
        taskMap_.erase(taskMap_.begin(), endIt);
    }

    std::vector<ReconnectTask> failedTasks;
    failedTasks.reserve(timeoutTasks.size());
    for (auto &task : timeoutTasks) {
        auto res = reconnFunc_(task.rankId, task.nic);
        if (res == BM_OK) {
            BM_LOG_INFO("reconnect for rank id: " << task.rankId << " success.");
        } else {
            BM_LOG_DEBUG("reconnect for rank id: " << task.rankId << " failed:" << res);
            failedTasks.emplace_back(std::move(task));
        }
    }

    if (!failedTasks.empty()) {
        now = std::chrono::steady_clock::now();
        nowDifMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - baseTimePoint_).count();
        for (auto &task : failedTasks) {
            task.failedTimes++;
            auto powWait = static_cast<int64_t>(std::pow(minWaitMs_, task.failedTimes + 1));
            task.nextConnectTime = std::min(powWait, maxWaitMs_) + nowDifMs;
        }

        std::unique_lock<std::mutex> locker{mapMutex_};
        for (auto &task : failedTasks) {
            ReconnectTaskKey key{task.nextConnectTime, task.rankId};
            taskMap_.emplace(key, std::move(task));
            BM_LOG_DEBUG("add back for task(rank: " << task.rankId << ", time:" << task.nextConnectTime << ")");
        }
    }
}
} // namespace host
} // namespace transport
} // namespace mf
} // namespace ock