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

template <typename Key, typename Value, uint32_t numBuckets> class MmcLookupMap {
    static_assert(numBuckets > 0, "numBuckets must be positive");
    using BucketPtr = std::unordered_map<Key, Value> *;
    using MapIterator = typename std::unordered_map<Key, Value>::iterator;

public:
    class Iterator {
    public:
        Iterator(BucketPtr begin, BucketPtr end) : curBucket_(begin), endBucket_(end)
        {
            if (curBucket_ != endBucket_) {
                mapIter_ = curBucket_->begin();
                SkipEmptyBuckets();
            }
        }

        std::pair<const Key, Value> &operator*() const
        {
            return *mapIter_;
        }

        Iterator &operator++()
        {
            ++mapIter_;
            SkipEmptyBuckets();
            return *this;
        }

        bool operator==(const Iterator &other) const
        {
            if (curBucket_ != other.curBucket_) {
                return false;
            }
            if (curBucket_ == endBucket_) {
                return true;
            }
            return mapIter_ == other.mapIter_;
        }

        bool operator!=(const Iterator &other) const
        {
            return curBucket_ != other.curBucket_;
        }

    private:
        BucketPtr curBucket_;
        BucketPtr endBucket_;
        MapIterator mapIter_;

        // use to jump between buckets
        void SkipEmptyBuckets()
        {
            // while current bucket is not the last bucket AND mapIter is at the end of current bucket
            while (curBucket_ != endBucket_ && mapIter_ == curBucket_->end()) {
                ++curBucket_;  // go to next bucket
                if (curBucket_ != endBucket_) {
                    mapIter_ = curBucket_->begin();
                }
            }
        }
    };

    Iterator begin()
    {
        return Iterator(buckets_, buckets_ + numBuckets);
    }

    Iterator end()
    {
        return Iterator(buckets_ + numBuckets, buckets_ + numBuckets);
    }

    /**
     * @brief Insert a value into this concurrent hash map
     *
     * @param key          [in] key of value to be inserted
     * @param value        [in] value to be inserted
     * @return 0 if insert successfully
     */
    Result Insert(const Key &key, const Value &value)
    {
        std::size_t index = GetIndex(key);
        std::lock_guard<std::mutex> guard(locks_[index]);
        auto ret = buckets_[index].emplace(key, value);
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
    Result Find(const Key &key, Value &value)
    {
        std::size_t index = GetIndex(key);
        std::lock_guard<std::mutex> guard(locks_[index]);
        auto iter = buckets_[index].find(key);
        if (iter != buckets_[index].end()) {
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
        std::size_t index = GetIndex(key);
        std::lock_guard<std::mutex> guard(locks_[index]);
        if (buckets_[index].erase(key) > 0) {
            return MMC_OK;
        }
        return MMC_ERROR;
    }

private:
    std::hash<Key> keyHasher_;

    std::size_t GetIndex(const Key &key) const
    {
        return keyHasher_(key) % numBuckets;
    }

private:
    std::unordered_map<Key, Value> buckets_[numBuckets];
    std::mutex locks_[numBuckets];
};
}  // namespace mmc
}  // namespace ock

#endif  // MEMFABRIC_HYBRID_MMC_LOOKUP_MAP_H