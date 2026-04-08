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

#include "emb_flash_hashmap_types.h"

namespace ock {
namespace emb {
namespace hashmap {
/**
 * Updatable hashmap bucket
 */
struct HashBucket {
    /**
     * @brief Insert key and value into this bucket
     *
     * @param key          [in] key to be inserted
     * @param value        [in] value to be inserted
     * @return 0 if successfully inserted
     */
    Result Insert(uint64_t key, uint64_t value) noexcept;

    /**
     * @brief Get value by key
     *
     * @param key          [in] key to be found
     * @param value        [out] value that found
     * @return 0 if found
     */
    Result Get(uint64_t key, uint64_t &value) noexcept;

    /**
     * @brief Get value by key without lock
     *
     * @param key          [in] key to be found
     * @param value        [out] value that found
     * @return 0 if found
     */
    Result GetWithoutLock(uint64_t key, uint64_t &value) noexcept;

    /**
     * @brief Get the last key and
     *
     * @param key          [out] the key of last one
     * @param value        [out] the value of last one
     * @return 0 if there is last one
     */
    Result GetAndEraseLast(uint64_t &key, uint64_t &value) noexcept;

    /**
     * @brief Replace the key and value with new one if found
     *
     * @param originalKey  [in] the key to be replaced
     * @param key          [in] new key
     * @param value        [in] new value
     * @return 0 if successful
     */
    Result Replace(uint64_t originalKey, uint64_t key, uint64_t value) noexcept;

    /**
     * @brief To string
     * @return string representation of this object
     */
    std::string ToString() const noexcept;

    uint64_t key_[UN3]{kInvalidMapKey, kInvalidMapKey, kInvalidMapKey};         /* initialize with invalid key */
    uint64_t value_[UN3]{kInvalidMapValue, kInvalidMapValue, kInvalidMapValue}; /* initialize with invalid value */
    HashBucket *next_ = nullptr;                                                /* pointer to next bucket */
    BucketSpinLock spinLock_{};                                                 /* spin lock */
} __attribute__((aligned(8)));

EM_ALWAYS_INLINE Result HashBucket::Insert(uint64_t key, uint64_t value) noexcept
{
    /* lock and insert, don't do loop here */
    spinLock_.Lock();
    if (key_[UN0] == kInvalidMapKey) {
        key_[UN0] = key;
        value_[UN0] = value;
        spinLock_.UnLock();
        return EM_OK;
    }

    if (key_[UN1] == kInvalidMapKey) {
        key_[UN1] = key;
        value_[UN1] = value;
        spinLock_.UnLock();
        return EM_OK;
    }

    if (key_[UN2] == kInvalidMapKey) {
        key_[UN2] = key;
        value_[UN2] = value;
        spinLock_.UnLock();
        return EM_OK;
    }

    spinLock_.UnLock();
    return EM_HASHMAP_BUCKET_FULL;
}

EM_ALWAYS_INLINE Result HashBucket::Get(uint64_t key, uint64_t &value) noexcept
{
    /* lock and insert, don't do loop here */
    spinLock_.Lock();
    if (key_[UN0] == key) {
        value = value_[UN0];
        spinLock_.UnLock();
        return EM_OK;
    }

    if (key_[UN1] == key) {
        value = value_[UN1];
        spinLock_.UnLock();
        return EM_OK;
    }

    if (key_[UN2] == key) {
        value = value_[UN2];
        spinLock_.UnLock();
        return EM_OK;
    }

    spinLock_.UnLock();

    return EM_HASHMAP_KEY_NOT_FOUND;
}

EM_ALWAYS_INLINE Result HashBucket::GetWithoutLock(uint64_t key, uint64_t &value) noexcept
{
    /* lock and insert, don't do loop here */
    if (key_[UN0] == key) {
        value = value_[UN0];
        return EM_OK;
    }

    if (key_[UN1] == key) {
        value = value_[UN1];
        return EM_OK;
    }

    if (key_[UN2] == key) {
        value = value_[UN2];
        return EM_OK;
    }

    return EM_HASHMAP_KEY_NOT_FOUND;
}

EM_ALWAYS_INLINE Result HashBucket::GetAndEraseLast(uint64_t &key, uint64_t &value) noexcept
{
    /* don't lock here, we do lock outside */
    auto buck = this;
    while (buck != nullptr) {
        /* if the first one key invalid, means the bucket is empty and no linked bucketed */
        if (buck->key_[UN0] == kInvalidMapKey) {
            return EM_HASHMAP_KEY_NOT_FOUND;
        }

        /* if the second key is invalid, first one must be valid */
        if (buck->key_[UN1] == kInvalidMapKey) {
            key = buck->key_[UN0];
            value = buck->value_[UN0];
            buck->key_[UN0] = kInvalidMapKey;
            buck->value_[UN0] = kInvalidMapValue;
            return EM_OK;
        }

        /* if the third key is invalid, second one must be valid */
        if (buck->key_[UN2] == kInvalidMapKey) {
            key = buck->key_[UN1];
            value = buck->value_[UN1];
            buck->key_[UN1] = kInvalidMapKey;
            buck->value_[UN1] = kInvalidMapValue;
            return EM_OK;
        }

        /* if next is nullptr, third one must be valid */
        if (buck->next_ == nullptr) {
            key = buck->key_[UN2];
            value = buck->value_[UN2];
            buck->key_[UN2] = kInvalidMapKey;
            buck->value_[UN2] = kInvalidMapValue;
            return EM_OK;
        }

        /* if next is not nullptr, but no key */
        if (buck->next_->key_[UN0] == kInvalidMapKey) {
            key = buck->key_[UN2];
            value = buck->value_[UN2];
            buck->key_[UN2] = kInvalidMapKey;
            buck->value_[UN2] = kInvalidMapValue;
            return EM_OK;
        }

        /* if the 2nd and 3rd are valid key, move to next */
        buck = buck->next_;
    }

    return EM_HASHMAP_KEY_NOT_FOUND;
}

EM_ALWAYS_INLINE Result HashBucket::Replace(uint64_t originalKey, uint64_t key, uint64_t value) noexcept
{
    /* do this without lock, we have lock outside */
    if (key_[UN0] == originalKey) {
        key_[UN0] = key;
        value_[UN0] = value;
        return EM_OK;
    }

    if (key_[UN1] == originalKey) {
        key_[UN1] = key;
        value_[UN1] = value;
        return EM_OK;
    }

    if (key_[UN2] == originalKey) {
        key_[UN2] = key;
        value_[UN2] = value;
        return EM_OK;
    }

    return EM_HASHMAP_KEY_NOT_FOUND;
}

inline std::string HashBucket::ToString() const noexcept
{
    std::ostringstream oss;
    oss << "HashBucket [[" << key_[UN0] << "," << value_[UN0] << "], [" << key_[UN1] << "," << value_[UN1] << "], ["
        << key_[UN2] << "," << value_[UN2] << "], next: " << std::hex << next_ << "]";
    return oss.str();
}

/**
 * Allocator of overflowed bucket
 */
class NaiveBucketAllocator {
public:
    void *Allocate(uint64_t size) noexcept;

    void Free(void *p) noexcept;
};

EM_ALWAYS_INLINE void *NaiveBucketAllocator::Allocate(uint64_t size) noexcept
{
    return calloc(1, size);
}

EM_ALWAYS_INLINE void NaiveBucketAllocator::Free(void *p) noexcept
{
    if (LIKELY(p != nullptr)) {
        free(p);
        p = nullptr;
    }
}

/**
 * Flash hashmap which supports update and query
 */
template<typename OverflowAllocator = NaiveBucketAllocator>
class FlashHashmap {
public:
    FlashHashmap() = default;
    ~FlashHashmap()
    {
        UnInitialize();
    }

    /**
     * @brief Initialize a hash map from scratch
     *
     * @param capacity     [in] reserved capacity, the bucket count will be determined by this
     *
     * @return 0 if successful
     */
    Result Initialize(uint64_t capacity) noexcept;

    /**
     * @brief Initialize from persist file
     *
     * @param options      [in] options for recover
     *
     * @return 0 if successful
     */
    Result Initialize(const FlashHashmapRecoverOptions &options) noexcept;

    /**
     * @brief UnInitialize the hashmap, all resources will be released
     */
    void UnInitialize() noexcept;

    /**
     * @brief Try to find in the hashmap by key, if found return 0 and its value,
     * otherwise insert the key/value
     *
     * @param key          [in] key to be found
     * @param value        [in/out] value that found or to be inserted
     * @return 0 if found
     */
    Result FindOrInsert(uint64_t key, uint64_t &value) noexcept;

    /**
     * @brief Try to find in the hash map by key
     *
     * @param key          [in] key to be found
     * @param value        [out] value of the key if found
     * @return 0 if found
     */
    Result Find(uint64_t key, uint64_t &value) noexcept;

    /**
     * @brief Remove the value by key  in the hashmap
     *
     * @param key          [in] key to be removed
     * @param value        [out] the value if key is found
     * @return 0 if found
     */
    Result Remove(uint64_t key, uint64_t &value) noexcept;

    /**
     * @brief Get bucket count per sub map
     *
     * @return bucket count of each sub map
     */
    uint32_t BucketCount() const noexcept;

    /**
     * @brief Get the size of hash map
     * @return size
     */
    uint64_t Size() const noexcept;

    /**
     * @brief Check if the hashmap is initialized successfully or not
     *
     * @return true if initialized
     */
    bool Initialized() const noexcept;

    /**
     * @brief stream output
     */
    friend std::ostream &operator<<(std::ostream &os, const FlashHashmap<OverflowAllocator> &o)
    {
        uint64_t tmpBucketCount = o.bucketCount_;
        os << "FlashHashmap [" << "size_: " << o.size_.load() << ", bucketCount_: " << o.bucketCount_
           << ", kSubMapCount: " << kSubMapCount << ", allocator_: " << o.allocator_
           << ", sizeOfBucket: " << sizeof(HashBucket) << ", totalBucketCount: " << (tmpBucketCount * kSubMapCount)
           << ", memorySizeOfAllBuckets: " << (tmpBucketCount * kSubMapCount * sizeof(HashBucket)) << "bytes ]";

        return os;
    }

    void IncreaseRef() noexcept;
    void DecreaseRef() noexcept;

private:
    Result CreateSubMaps() noexcept;
    void DestroySubMaps() noexcept;
    void DestroyOverflowedBuckets() noexcept;
    Result NewBucketAndPut(uint64_t key, uint64_t value, HashBucket *lastBucket) noexcept;

private:
    /* make sure the size of this class is 64bytes which fit into one CPU cacheline */
    int32_t refCount_ = 0;                   /* ref count*/
    uint32_t bucketCount_ = 0;               /* bucket count of each sub map */
    std::atomic<uint64_t> size_{0};          /* size of items */
    HashBucket *subMaps_[kSubMapCount]{};    /* sub map, 5 sub map */
    OverflowAllocator *allocator_ = nullptr; /* allocator of overflowed bucket */
    /* make sure the size of this class is 64bytes which fit into one CPU cacheline */
};

using FlashHashmapPtr = EmRef<FlashHashmap<NaiveBucketAllocator>>;

template<typename OverflowAllocator>
EM_ALWAYS_INLINE void FlashHashmap<OverflowAllocator>::IncreaseRef() noexcept
{
    __atomic_add_fetch(&refCount_, 1, __ATOMIC_RELAXED);
}

template<typename OverflowAllocator>
EM_ALWAYS_INLINE void FlashHashmap<OverflowAllocator>::DecreaseRef() noexcept
{
    if (__atomic_sub_fetch(&refCount_, 1, __ATOMIC_ACQ_REL) == 0) {
        delete this;
    }
}

template<typename OverflowAllocator>
inline Result FlashHashmap<OverflowAllocator>::Initialize(uint64_t capacity) noexcept
{
    if (allocator_ != nullptr) {
        return EM_OK;
    }

    /* get proper bucketCount for easy subMap */
    auto bucketCountPerSubMap = capacity / kSubMapCount;
    bucketCountPerSubMap = (bucketCountPerSubMap > UN5) ? bucketCountPerSubMap : UN5;

    /* loop primes array to get a proper prime which is just less than bucketCountPerSubMap */
    uint64_t i = 0;
    while (i < kMapPrimesCount - 1 && kMapPrimes[i] < bucketCountPerSubMap) {
        i++;
    }
    bucketCount_ = kMapPrimes[i];
    EM_LOG_DEBUG("bucket count is set to " << bucketCount_);

    /* create sub map */
    auto result = CreateSubMaps();
    EM_ASSERT_RETURN(result == EM_OK, result);

    /* create allocator  */
    allocator_ = new (std::nothrow) OverflowAllocator();
    if (allocator_ == nullptr) {
        DestroySubMaps();
        bucketCount_ = 0;
        EM_LOG_ERROR("New overflow bucket allocator failed, probably out of memory");
        return EM_NEW_OBJ_FAILED;
    }

    EM_LOG_DEBUG("FlashHashmap is initialized, dump " << (*this));
    return EM_OK;
}

template<typename OverflowAllocator>
inline Result FlashHashmap<OverflowAllocator>::Initialize(const FlashHashmapRecoverOptions &options) noexcept
{
    return EM_OK;
}

template<typename OverflowAllocator>
inline void FlashHashmap<OverflowAllocator>::UnInitialize() noexcept
{
    if (allocator_ == nullptr) {
        EM_LOG_DEBUG("Not initialized");
        return;
    }

    /* free all overflow buckets */
    DestroyOverflowedBuckets();

    /* destroy sub maps */
    DestroySubMaps();

    bucketCount_ = 0;
    size_ = 0;

    delete allocator_;
    allocator_ = nullptr;
}

template<typename OverflowAllocator>
inline uint64_t FlashHashmap<OverflowAllocator>::Size() const noexcept
{
    return size_.load();
}

template<typename OverflowAllocator>
inline Result FlashHashmap<OverflowAllocator>::FindOrInsert(uint64_t key, uint64_t &value) noexcept
{
    if (UNLIKELY(key == kInvalidMapKey)) {
        return EM_HASHMAP_INVALID_KEY;
    }

    /* get bucket */
    auto buck = &(subMaps_[key % kSubMapCount][key % bucketCount_]);

    /* loop all buckets linked */
    while (buck != nullptr) {
        if (buck->Get(key, value) == EM_OK) {
            return EM_OK;
        }

        /* assign spin lock to tmp one as we are moving forward bucket ptr */
        auto &tmpBuckLock = buck->spinLock_;
        tmpBuckLock.Lock();
        if (buck->next_ != nullptr) {
            buck = buck->next_;
            tmpBuckLock.UnLock();
        } else {
            tmpBuckLock.UnLock();
            break;
        }
    }

    /* create new bucket and insert */
    return NewBucketAndPut(key, value, buck);
}

template<typename OverflowAllocator>
inline Result FlashHashmap<OverflowAllocator>::NewBucketAndPut(uint64_t key, uint64_t value,
                                                               HashBucket *lastBucket) noexcept
{
    /*
     * here we need to handle the case of multiple threads,
     * one thread is doing allocation,
     * other threads are waiting by busy loop, so here we do loop here
     */
    auto buck = lastBucket;
    for (auto i = 0; i < UN4096; i++) {
        /* if the bucket is not null, i.e. new bucket is created, probably created by another thread */
        while (buck != nullptr) {
            if (buck->Insert(key, value) == EM_OK) {
                ++size_;
                return EM_OK;
            }

            /* since there are only 3 key/value can be stored in one bucket, there is a case
             * that more than 3 threads doing insert at the same time, so we need to create more bucket
             *
             * NOTE: assign spin lock to tmp one as we are moving forward bucket ptr
             */
            auto &tmpBuckLock = buck->spinLock_;
            tmpBuckLock.Lock();
            if (buck->next_ != nullptr) {
                buck = buck->next_;
                tmpBuckLock.UnLock();
            } else {
                tmpBuckLock.UnLock();
                break;
            }
        }

        /*
         * if not put successfully in existing bucket, allocate a new one, i.e. the new created bucket is
         * full again, need to create new one again, when the threads count is larger than 3 and position
         * on the same bucket, we need to handle this event possibility is low
         */
        auto &lock = buck->spinLock_;
        lock.Lock();
        /* if another thread allocated new buck already, unlock and continue, do the insert operation */
        if (buck->next_ != nullptr) {
            buck = buck->next_;
            lock.UnLock();
            continue;
        }

        /*
         * firstly entered thread allocate new bucket */
        auto newBuckRaw = allocator_->Allocate(sizeof(HashBucket));
        if (UNLIKELY(newBuckRaw == nullptr)) {
            lock.UnLock();
            EM_LOG_ERROR("Alloc new overflowed bucket from allocator failed, probably out of memory");
            return EM_NEW_OBJ_FAILED;
        }

        /*
         * placement new, this maybe trigger page fault, that result in holding the spin lock for a while,
         * to improve this, a better cachable bucket allocator need to be implemented
         */
        auto newBuck = new (newBuckRaw) HashBucket();

        /* secondly link to current buck, set buck to new buck, don't do any memset there */
        buck->next_ = newBuck;
        buck = newBuck;

        /* unlock */
        lock.UnLock();
    }

    EM_LOG_DEBUG("create new overflowed bucket for key: " << key << ", value: " << value);
    return EM_HASHMAP_NEW_BUCKET_FAILED;
}

template<typename OverflowAllocator>
inline Result FlashHashmap<OverflowAllocator>::Find(uint64_t key, uint64_t &value) noexcept
{
    if (UNLIKELY(key == kInvalidMapKey)) {
        return EM_HASHMAP_INVALID_KEY;
    }

    /* get bucket */
    auto buck = &(subMaps_[key % kSubMapCount][key % bucketCount_]);

    /* loop all buckets linked */
    while (buck != nullptr) {
        if (buck->Get(key, value) == EM_OK) {
            return EM_OK;
        }

        /* assign spin lock to tmp one as we are moving forward bucket ptr */
        auto &tmpBuckLock = buck->spinLock_;
        tmpBuckLock.Lock();
        if (buck->next_ != nullptr) {
            buck = buck->next_;
            tmpBuckLock.UnLock();
        } else {
            tmpBuckLock.UnLock();
            break;
        }
    }

    EM_LOG_DEBUG("key: " << key << " not found");
    return EM_HASHMAP_KEY_NOT_FOUND;
}

template<typename OverflowAllocator>
inline Result FlashHashmap<OverflowAllocator>::Remove(uint64_t key, uint64_t &value) noexcept
{
    if (UNLIKELY(key == kInvalidMapKey)) {
        return EM_HASHMAP_INVALID_KEY;
    }

    /* remove key and value is heavy operation, in this operation:
     * 1 hold the spin lock of the first bucket
     * 2 loop all linked buckets
     * 3 get the last key/value
     * 4 replace the key if found with last key/value
     */
    /* get bucket */
    auto buck = &(subMaps_[key % kSubMapCount][key % bucketCount_]);

    /* do lock */
    auto &lock = buck->spinLock_;
    lock.Lock();

    /* loop all linked buckets */
    while (buck != nullptr) {
        /* not found the key, go to next */
        if (buck->GetWithoutLock(key, value) != EM_OK) {
            buck = buck->next_;
            continue;
        }

        /*
         * if found, get and erase the fast one on current and contiguous buckets,
         * and for sure, we can find the last valid key and value,
         * if not get last key/value, then don't know what happened, so here we just log an error message
         */
        uint64_t lastKey = kInvalidMapKey;
        uint64_t lastValue = kInvalidMapValue;
        auto result = buck->GetAndEraseLast(lastKey, lastValue);
        if (result != EM_OK) {
            lock.UnLock();
            EM_LOG_ERROR("Un-reachable path");
            return EM_ERROR;
        }

        /* if the last key is same with key to be removed, just return as it already erased */
        if (key == lastKey) {
            --size_;
            lock.UnLock();
            return EM_OK;
        }

        /* replace the key and value with the last one */
        result = buck->Replace(key, lastKey, lastValue);
        if (result != EM_OK) {
            lock.UnLock();
            EM_LOG_ERROR("Un-reachable path");
            return EM_ERROR;
        }

        --size_;
        lock.UnLock();
        return EM_OK;
    }

    /* unlock */
    lock.UnLock();

    return EM_HASHMAP_KEY_NOT_FOUND;
}

template<typename OverflowAllocator>
inline uint32_t FlashHashmap<OverflowAllocator>::BucketCount() const noexcept
{
    return bucketCount_;
}

template<typename OverflowAllocator>
inline bool FlashHashmap<OverflowAllocator>::Initialized() const noexcept
{
    return allocator_ != nullptr;
}

template<typename OverflowAllocator>
inline Result FlashHashmap<OverflowAllocator>::CreateSubMaps() noexcept
{
    /* set all sub maps to nullptr */
    for (auto i = 0; i < kSubMapCount; i++) {
        subMaps_[i] = nullptr;
    }

    /* allocate buckets for sub-maps */
    for (auto i = 0; i < kSubMapCount; i++) {
        auto tmp = new (std::nothrow) HashBucket[bucketCount_];
        if (tmp == nullptr) {
            DestroySubMaps();
            EM_LOG_ERROR("Create sub map buckets failed, probably out of memory");
            return EM_NEW_OBJ_FAILED;
        }

        subMaps_[i] = tmp;
    }

    EM_LOG_DEBUG("sub maps are created");
    return EM_OK;
}

template<typename OverflowAllocator>
inline void FlashHashmap<OverflowAllocator>::DestroySubMaps() noexcept
{
    /* free all sub maps */
    for (auto i = 0; i < kSubMapCount; i++) {
        auto &tmp = subMaps_[i];
        if (tmp != nullptr) {
            delete[] tmp;
            subMaps_[i] = nullptr;
        }
    }

    EM_LOG_DEBUG("sub maps are destroyed");
}

template<typename OverflowAllocator>
inline void FlashHashmap<OverflowAllocator>::DestroyOverflowedBuckets() noexcept
{
    for (auto i = 0; i < kSubMapCount; i++) {
        auto &tmpSubMap = subMaps_[i];
        if (tmpSubMap == nullptr) {
            continue;
        }

        /* free overflow entries in one sub map */
        for (uint32_t buckIndex = 0; buckIndex < bucketCount_; ++buckIndex) {
            auto curBuck = tmpSubMap[buckIndex].next_;
            HashBucket *nextOverflowBuck = nullptr;

            /* exit loop when curBuck is null */
            while (curBuck != nullptr) {
                /* assign next overflow buck to tmp variable */
                nextOverflowBuck = curBuck->next_;

                /* free this overflow bucket */
                allocator_->Free(curBuck);

                /* assign next to current */
                curBuck = nextOverflowBuck;
            }
        }
    }

    EM_LOG_DEBUG("destroy overflowed buckets");
}

} // namespace hashmap
} // namespace emb
} // namespace ock

#endif // MEMFABRIC_HYBRID_EMB_FAST_HASHTABLE_H
