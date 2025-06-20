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

template <typename Key, typename Value, uint32_t numBuckets>
class LookupMap {
    static_assert(numBuckets > 0, "numBuckets must be positive");

public:
    /**
     * @brief Insert a value into this concurrent hash map
     *
     * @param key          [in] key of value to be inserted
     * @param value        [in] value to be inserted
     * @return 0 if insert successfully
     */
    Result Insert(const Key &key, const Value &value)
    {
        Bucket &bucket = GetBucket(key);
        std::lock_guard<std::mutex> guard(bucket.mutex_);
        auto ret = bucket.data_.emplace(key, value);
        if (ret.second) {
            return MMC_OK;
        }
        return MMC_ERROR;
    }

    /**
     * @brief Find the value by key from the concurrent hash map
     *
     * @param key          [in] key to be found
     * @param value        [in/out] value found
     * @return 0 if found
     */
    Result Find(const Key &key, Value &value) const
    {
        Bucket &bucket = GetBucket(key);
        std::lock_guard<std::mutex> guard(bucket.mutex_);
        auto iter = bucket.data_.find(key);
        if (iter != bucket.data_.end()) {
            value = iter->second;
            return MMC_OK;
        }
        return MMC_ERROR;
    }

    /**
     * @brief Erase the value by key
     *
     * @param key          [in] the key of value to be erased
     * @return 0 if erase successfully
     */
    Result Erase(const Key &key)
    {
        Bucket &bucket = GetBucket(key);
        std::lock_guard<std::mutex> guard(bucket.mutex_);
        if (bucket.data_.erase(key) > 0) {
            return MMC_OK;
        }
        return MMC_ERROR;
    }

private:
    struct Bucket {
        std::unordered_map<Key, Value> data_;
        std::mutex mutex_;
    };

    std::hash<Key> keyHasher;

    Bucket &GetBucket(Key key) const
    {
        return buckets_[keyHasher(key) % numBuckets];
    }

private:
    Bucket buckets_[numBuckets];
};
}  // namespace mmc
}  // namespace ock

#endif  // MEMFABRIC_HYBRID_MMC_LOOKUP_MAP_H