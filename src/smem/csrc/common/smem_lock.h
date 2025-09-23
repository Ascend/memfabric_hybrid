/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef MEMFABRIC_HYBRID_SMEM_LOCK_H
#define MEMFABRIC_HYBRID_SMEM_LOCK_H

#include <pthread.h>

namespace ock {
namespace smem {
class ReadWriteLock {
public:
    ReadWriteLock()
    {
        pthread_rwlock_init(&mLock, nullptr);
    }

    ~ReadWriteLock()
    {
        pthread_rwlock_destroy(&mLock);
    }

    ReadWriteLock(const ReadWriteLock &) = delete;
    ReadWriteLock &operator=(const ReadWriteLock &) = delete;
    ReadWriteLock(ReadWriteLock &&) = delete;
    ReadWriteLock &operator=(ReadWriteLock &&) = delete;

    inline void LockRead()
    {
        pthread_rwlock_rdlock(&mLock);
    }

    inline void LockWrite()
    {
        pthread_rwlock_wrlock(&mLock);
    }

    inline void UnLock()
    {
        pthread_rwlock_unlock(&mLock);
    }

private:
    pthread_rwlock_t mLock{};
};

class WriteGuard {
public:
    WriteGuard(ReadWriteLock &lock) : lock_(lock)
    {
        lock_.LockWrite();
    }

    ~WriteGuard()
    {
        lock_.UnLock();
    }

    WriteGuard(const WriteGuard &) = delete;
    WriteGuard &operator=(const WriteGuard &) = delete;
    WriteGuard(WriteGuard &&) = delete;
    WriteGuard &operator=(WriteGuard &&) = delete;

private:
    ReadWriteLock &lock_;
};

class ReadGuard {
public:
    ReadGuard(ReadWriteLock &lock) : lock_(lock)
    {
        lock_.LockRead();
    }

    ~ReadGuard()
    {
        lock_.UnLock();
    }

    ReadGuard(const ReadGuard &) = delete;
    ReadGuard &operator=(const ReadGuard &) = delete;
    ReadGuard(ReadGuard &&) = delete;
    ReadGuard &operator=(ReadGuard &&) = delete;

private:
    ReadWriteLock &lock_;
};
}
}

#endif  // MEMFABRIC_HYBRID_SMEM_LOCK_H
