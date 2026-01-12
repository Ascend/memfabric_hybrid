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

#ifndef MEMFABRIC_HYBRID_SPINLOCK_H
#define MEMFABRIC_HYBRID_SPINLOCK_H

#include <atomic>
#include <thread>

namespace ock {
namespace mf {

class SpinLock {
public:
    SpinLock() : flag_(false) {}

    void lock()
    {
        while (flag_.exchange(true, std::memory_order_acquire)) {
            while (flag_.load(std::memory_order_relaxed)) {
                std::this_thread::yield();  // 让出CPU时间片
            }
        }
    }

    void unlock()
    {
        flag_.store(false, std::memory_order_release);
    }

    bool try_lock()
    {
        return !flag_.exchange(true, std::memory_order_acquire);
    }

private:
    std::atomic<bool> flag_;
};

} // namespace mf
} // namespace ock

#endif // MEMFABRIC_HYBRID_SPINLOCK_H