/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#pragma once

#include <mutex>

namespace ock {
namespace mmc {
class Lock {
public:
    Lock() = default;
    ~Lock() = default;

    Lock(const Lock &) = delete;
    Lock &operator = (const Lock &) = delete;
    Lock(Lock &&) = delete;
    Lock &operator = (Lock &&) = delete;

    inline void DoLock()
    {
        mLock.lock();
    }

    inline void Unlock()
    {
        mLock.unlock();
    }

private:
    std::mutex mLock;
};

template <class T> class Locker {
public:
    explicit Locker(T *lock) : mLock(lock)
    {
        if (mLock != nullptr) {
            mLock->DoLock();
        }
    }

    ~Locker()
    {
        if (mLock != nullptr) {
            mLock->Unlock();
        }
    }

    Locker(const Locker &) = delete;
    Locker &operator = (const Locker &) = delete;
    Locker(Locker &&) = delete;
    Locker &operator = (Locker &&) = delete;

private:
    T *mLock;
};

#define GUARD(lLock, alias) Locker<Lock> __l##alias(lLock)
} // namespace mmc
} // namespace ock

