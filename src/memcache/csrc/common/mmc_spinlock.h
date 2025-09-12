/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_MMC_SPINLOCK_H
#define MEMFABRIC_HYBRID_MMC_SPINLOCK_H

#include <atomic>
#include <mutex>

namespace ock {
namespace mmc {

class Spinlock {
public:
    /**
     * Acquire the lock
     * Note: Use lowercase name here, then we can use std::lock_guard with SpinLock
     */
    void lock();

    /**
     * Release the lock
     * Note: Use lowercase name here, then we can use std::lock_guard with SpinLock
     */
    void unlock();

private:
    std::atomic<uint32_t> lock_{0};  // 4 bytes atomic variable
};

inline void Spinlock::lock()
{
    while (lock_.exchange(1, std::memory_order_acquire)) {
    }
}

inline void Spinlock::unlock()
{
    lock_.store(0, std::memory_order_release);
}

}  // namespace mmc
}  // namespace ock

#endif  // MEMFABRIC_HYBRID_MMC_SPINLOCK_H