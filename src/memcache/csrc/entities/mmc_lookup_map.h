/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEMFABRIC_HYBRID_MMC_LOOKUP_MAP_H
#define MEMFABRIC_HYBRID_MMC_LOOKUP_MAP_H

#include "mmc_spinlock.h"
#include "mmc_types.h"
#include <unordered_map>

namespace ock {
namespace mmc {

template <typename Key, typename Value, uint32_t numBuckets> class LookupMap {
    static_assert(numBuckets > 0, "numBuckets must be positive");

public:
    Result Insert(const Key &key, const Value &value)
    {
        Bucket &bucket = GetBucket(key);
        std::lock_guard<Spinlock> guard(bucket.spinlock_);
        auto ret = bucket.data_.emplace(key, value);
        if (ret.second) {
            return MMC_OK;
        }
        return MMC_ERROR;
    };

    Result Find(const Key &key, Value &value) const
    {
        Bucket &bucket = GetBucket(key);
        std::lock_guard<Spinlock> guard(bucket.spinlock_);
        auto iter = bucket.data_.find(key);
        if (iter != bucket.data_.end()) {
            value = iter->second;
            return MMC_OK;
        }
        return MMC_ERROR;
    };

    Result Erase(const Key &key)
    {
        Bucket &bucket = GetBucket(key);
        std::lock_guard<Spinlock> guard(bucket.spinlock_);
        if (bucket.data_.erase(key) > 0) {
            return MMC_OK;
        }
        return MMC_ERROR;
    };

private:
    struct Bucket {
        std::unordered_map<Key, Value> data_;
        Spinlock spinlock_;
    };

    Bucket buckets_[numBuckets];

    std::hash<Key> keyHasher;

    Bucket &GetBucket(Key key) const
    {
        uint32_t index = keyHasher(key) % numBuckets;
        return buckets_[index];
    };
};
} // namespace mmc
} // namespace ock

#endif // MEMFABRIC_HYBRID_MMC_LOOKUP_MAP_H