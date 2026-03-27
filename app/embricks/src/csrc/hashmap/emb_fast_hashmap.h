/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Embricks is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MEMFABRIC_HYBRID_EMB_FAST_HASHTABLE_H
#define MEMFABRIC_HYBRID_EMB_FAST_HASHTABLE_H

#include "emb_common_includes.h"

namespace ock {
namespace emb {

struct HashSpinLock {
public:
    void Lock() noexcept;
    void UnLock() noexcept;

private:
    uint64_t lock_ = 0;
} __attribute__((packed));

EM_ALWAYS_INLINE void HashSpinLock::Lock() noexcept
{
    while (!__sync_bool_compare_and_swap(&lock_, 0, 1)) {}
}

EM_ALWAYS_INLINE void HashSpinLock::UnLock() noexcept
{
    __atomic_store_n(&lock_, 0, __ATOMIC_SEQ_CST);
}

/**
* Major concept:
* 1 each bucket occupied 64bytes, which is equal to size of cacheline of typical modern CPU
* 2 each bucket store 3 key/value pairs, types of both key and value are uint64
* 3 the placement of bucket is following
*   | key1 | key2 | key3 | value1 | value2 | value3 | spinlock | ptrOfNextBucket
* 4 for read only bucket, no cas and no spinlock logic
* 5 for update cas for used for bucket
* 6 for append next bucket, spin lock is acquired
*/
struct HashBucketWithoutLock {
public:
    bool Put(uint64_t key, uint64_t value) noexcept;
    bool Get(uint64_t key, uint64_t &value) noexcept;
    bool Remove(uint64_t key, uint64_t &value) noexcept;

private:
    uint64_t key_[3L]{};
    uint64_t value_[3L]{};
    HashBucketWithoutLock *next_ = nullptr;
    HashSpinLock spinLock_{};
};

EM_ALWAYS_INLINE bool HashBucketWithoutLock::Put(uint64_t key, uint64_t value) noexcept
{
    return false;
}

EM_ALWAYS_INLINE bool HashBucketWithoutLock::Get(uint64_t key, uint64_t &value) noexcept
{
    return false;
}

EM_ALWAYS_INLINE bool HashBucketWithoutLock::Remove(uint64_t key, uint64_t &value) noexcept
{
    return false;
}

} // namespace emb
} // namespace ock

#endif // MEMFABRIC_HYBRID_EMB_FAST_HASHTABLE_H
