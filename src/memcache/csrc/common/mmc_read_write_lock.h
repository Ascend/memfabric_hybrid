/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_MMC_READWRITELOCK_H
#define MEMFABRIC_HYBRID_MMC_READWRITELOCK_H

#include <condition_variable>
#include <memory>
#include <mutex>

namespace ock {
namespace mmc {
class ReadWriteLock {
public:
    ReadWriteLock() = default;

    void LockRead()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (isWriting_) {
            cv_.wait(lock);
        }
        numReaders_++;
    }

    void UnlockRead()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        numReaders_--;
        if (numReaders_ == 0) {
            cv_.notify_one();  // wake up the write thread
        }
    }

    void LockWrite()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (isWriting_ || numReaders_ > 0) {
            cv_.wait(lock);
        }
        isWriting_ = true;
    }

    void UnlockWrite()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        isWriting_ = false;
        cv_.notify_all();  // wake up all the read threads
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    uint16_t numReaders_{0};  // number of current reade threads
    bool isWriting_{false};   // whether currently have write threads
};

class ReadLock {
public:
    explicit ReadLock(ReadWriteLock &rwLock) : rwLock_(rwLock)
    {
        rwLock_.LockRead();
    }
    ~ReadLock()
    {
        rwLock_.UnlockRead();
    }

    // forbid copy like the standard library
    ReadLock(const ReadLock &) = delete;
    ReadLock &operator=(const ReadLock &) = delete;

private:
    ReadWriteLock &rwLock_;
};

class WriteLock {
public:
    explicit WriteLock(ReadWriteLock &rwLock) : rwLock_(rwLock)
    {
        rwLock_.LockWrite();
    }
    ~WriteLock()
    {
        rwLock_.UnlockWrite();
    }

    // forbid copy like the standard library
    WriteLock(const ReadLock &) = delete;
    WriteLock &operator=(const ReadLock &) = delete;

private:
    ReadWriteLock &rwLock_;
};

}  // namespace mmc
}  // namespace ock

#endif  // MEMFABRIC_HYBRID_MMC_READWRITELOCK_H