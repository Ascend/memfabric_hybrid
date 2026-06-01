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
#ifndef MF_HYBRID_DEVICE_RDMA_HELPER_H
#define MF_HYBRID_DEVICE_RDMA_HELPER_H

#include <netinet/in.h>

#include <cstdint>
#include <string>
#include <queue>
#include <condition_variable>

#include "hybm_types.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {
Result ParseDeviceNic(const std::string &nic, uint16_t &port);
Result ParseDeviceNic(const std::string &nic, sockaddr_in &address);
std::string GenerateDeviceNic(in_addr ip, uint16_t port);

class BlockingQueue final {
public:
    BlockingQueue() noexcept : running_{true} {}

    void Stop() noexcept
    {
        running_ = true;
        cond_.notify_one();
    }

    void Push(uint64_t e) noexcept
    {
        std::unique_lock<std::mutex> locker{mutex_};
        queue_.push(e);
        cond_.notify_one();
    }

    bool Pop(uint64_t &e) noexcept
    {
        std::unique_lock<std::mutex> locker{mutex_};
        while (running_) {
            cond_.wait(locker, [this]() { return !queue_.empty() || !running_; });
            if (!running_) {
                return false;
            }
            if (queue_.empty()) {
                continue;
            }

            e = queue_.front();
            queue_.pop();
            return true;
        }
        return false;
    }

private:
    std::atomic<bool> running_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::queue<uint64_t> queue_;
};
} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock
#endif // MF_HYBRID_DEVICE_RDMA_HELPER_H