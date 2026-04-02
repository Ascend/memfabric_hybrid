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
#ifndef MEMFABRIC_HYBRID_HOST_HCOM_RECONNECTOR_H
#define MEMFABRIC_HYBRID_HOST_HCOM_RECONNECTOR_H

#include <cstdint>
#include <mutex>
#include <chrono>
#include <map>
#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include <functional>
#include <unordered_set>
#include <condition_variable>
#include "hybm_types.h"

namespace ock {
namespace mf {
namespace transport {
namespace host {

constexpr int64_t DEFAULT_MIN_WAIT_MS = 50;
constexpr int64_t DEFAULT_MAX_WAIT_MS = 1000;
using ReconnFunc = std::function<Result(uint32_t rankId, const std::string &nic)>;

struct ReconnectTask {
    uint32_t rankId;
    std::string nic;
    int64_t nextConnectTime;
    int64_t failedTimes;
    ReconnectTask() noexcept : ReconnectTask(0, "", 0) {}
    ReconnectTask(uint32_t r, std::string net, int64_t ts) noexcept
        : rankId{r}, nic{std::move(net)}, nextConnectTime{ts}, failedTimes{0}
    {}
};

struct ReconnectTaskKey {
    int64_t timestamp;
    uint32_t rankId;
    ReconnectTaskKey(int64_t ts, uint32_t rk) noexcept : timestamp{ts}, rankId{rk} {}
};

struct TaskKeyComparator {
    bool operator()(const ReconnectTaskKey &key1, const ReconnectTaskKey &key2) const noexcept
    {
        if (key1.timestamp != key2.timestamp) {
            return key1.timestamp < key2.timestamp;
        }
        return key1.rankId < key2.rankId;
    }
};

class HcomReconnector {
public:
    explicit HcomReconnector(int64_t minMs = DEFAULT_MIN_WAIT_MS, int64_t maxMs = DEFAULT_MAX_WAIT_MS) noexcept;
    Result Start(ReconnFunc reconnFunc) noexcept;
    void Stop() noexcept;
    void AddRank(uint32_t rankId) noexcept;
    void AddRanks(const std::vector<uint32_t> &ranks) noexcept;
    void RemoveRank(uint32_t rankId) noexcept;
    void RemoveRanks(const std::vector<uint32_t> &ranks) noexcept;
    Result AddReconnectTask(uint32_t rankId, const std::string &nic) noexcept;

private:
    void ReconnectLoop() noexcept;
    void ReconnectTimeoutTasks() noexcept;

private:
    const std::chrono::steady_clock::time_point baseTimePoint_;
    const int64_t minWaitMs_;
    const int64_t maxWaitMs_;
    std::atomic<bool> started_;
    std::mutex loopWaitMutex_;
    std::condition_variable loopWaitCond_;
    ReconnFunc reconnFunc_;
    std::thread reconnectThread_;
    std::mutex mapMutex_;
    std::map<ReconnectTaskKey, ReconnectTask, TaskKeyComparator> taskMap_;
    std::unordered_set<uint32_t> ranks_;
};
} // namespace host
} // namespace transport
} // namespace mf
} // namespace ock

#endif // MEMFABRIC_HYBRID_HOST_HCOM_RECONNECTOR_H
