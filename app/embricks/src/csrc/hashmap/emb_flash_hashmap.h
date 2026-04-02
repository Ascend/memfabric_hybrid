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

struct BucketSpinLock {
public:
    void Lock() noexcept;
    void UnLock() noexcept;

private:
    uint64_t lock_ = 0;
} __attribute__((aligned(8)));

EM_ALWAYS_INLINE void BucketSpinLock::Lock() noexcept
{
    while (!__sync_bool_compare_and_swap(&lock_, 0, 1)) {}
}

EM_ALWAYS_INLINE void BucketSpinLock::UnLock() noexcept
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
constexpr uint64_t kInvalidKey = UINT64_MAX;
constexpr uint64_t kInvalidValue = UINT64_MAX;

struct HashBucketReadonly {
public:
    /**
     * @brief Readonly bucket doesn't support put key/value
     */
    Result Put(uint64_t key, uint64_t value) noexcept;

    /**
     * @brief Get value by key in the bucket
     *
     * @param key          [in] key to be found
     * @param value        [out] value that found
     * @return 0 if found, EM_HASHMAP_NO_KEY_FOUND if not found
     */
    Result Get(uint64_t key, uint64_t &value) noexcept;

    /**
     * @brief Readonly bucket doesn't support remove
     */
    Result Remove(uint64_t key, uint64_t &value) noexcept;

private:
    uint64_t key_[UN3]{kInvalidKey, kInvalidKey, kInvalidKey};         /* initialize with invalid key */
    uint64_t value_[UN3]{kInvalidValue, kInvalidValue, kInvalidValue}; /* initialize with invalid value */
    HashBucketReadonly *next_ = nullptr;                               /* pointer to next bucket */
    BucketSpinLock spinLock_{};                                        /* spin lock of next bucket */
} __attribute__((aligned(8)));

EM_ALWAYS_INLINE Result HashBucketReadonly::Put(uint64_t key, uint64_t value) noexcept
{
    return EM_NOT_IMPLEMENTED;
}

EM_ALWAYS_INLINE Result HashBucketReadonly::Get(uint64_t key, uint64_t &value) noexcept
{
    /* don't use loop here for performance consideration */
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

    return EM_HASHMAP_NO_KEY_FOUND;
}

EM_ALWAYS_INLINE Result HashBucketReadonly::Remove(uint64_t key, uint64_t &value) noexcept
{
    return EM_NOT_IMPLEMENTED;
}

/**
 * Updatable hashmap bucket
 */
struct HashBucket {
public:
    Result Put(uint64_t key, uint64_t value) noexcept;
    Result Get(uint64_t key, uint64_t &value) noexcept;
    Result Remove(uint64_t key, uint64_t &value) noexcept;

private:
    uint64_t key_[UN3]{kInvalidKey, kInvalidKey, kInvalidKey};         /* initialize with invalid key */
    uint64_t value_[UN3]{kInvalidValue, kInvalidValue, kInvalidValue}; /* initialize with invalid value */
    HashBucket *next_ = nullptr;                                       /* pointer to next bucket */
    BucketSpinLock spinLock_{};                                        /* spin lock of next bucket */
} __attribute__((aligned(8)));

EM_ALWAYS_INLINE Result HashBucket::Put(uint64_t key, uint64_t value) noexcept
{
    return EM_NOT_IMPLEMENTED;
}

EM_ALWAYS_INLINE Result HashBucket::Get(uint64_t key, uint64_t &value) noexcept
{
    return EM_NOT_IMPLEMENTED;
}

EM_ALWAYS_INLINE Result HashBucket::Remove(uint64_t key, uint64_t &value) noexcept
{
    return EM_NOT_IMPLEMENTED;
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
 * Flash hash map, which un-ordered
 */
template<typename Bucket, typename BucketAllocator = NaiveBucketAllocator>
class FlashHashmap : public EmReferable {
public:
    FlashHashmap() = default;
    ~FlashHashmap() override;

    /**
     * @brief Initialize the flash hashmap
     *
     * @return 0 if successful
     */
    Result Initialize() noexcept;

    /**
     * @brief Un-initialize the map
     */
    void UnInitialize() noexcept;

    /**
     * @brief Get current item size of this map
     *
     * @return size of items
     */
    uint64_t Size() const noexcept;

    /**
     * @brief Update | insert if not found
     *
     * @param key          [in] key to be updated or inserted
     * @param value        [in/out] value
     * @return 0 if successful
     */
    Result FindOrUpsert(uint64_t key, uint64_t &value) noexcept;

    /**
     * @brief Find the value by key
     *
     * @param key          [in] key to be found
     * @param value        [out] value of the key
     * @return 0 if found
     */
    Result Find(const uint64_t key, uint64_t &value) noexcept;

    /**
     * @brief Remove the key
     *
     * @param key          [in] key to be removed
     * @return 0 if found and removed
     */
    Result Remove(uint64_t key) noexcept;

protected:
    static constexpr uint32_t kSubMapCount = 5;
    static constexpr uint32_t kPrimesCount = 256;
    const uint64_t kPrimes[kPrimesCount] = {
        2,          3,          5,          7,          11,         13,         17,         19,         23,
        29,         31,         37,         41,         43,         47,         53,         59,         61,
        67,         71,         73,         79,         83,         89,         97,         103,        109,
        113,        127,        137,        139,        149,        157,        167,        179,        193,
        199,        211,        227,        241,        257,        277,        293,        313,        337,
        359,        383,        409,        439,        467,        503,        541,        577,        619,
        661,        709,        761,        823,        887,        953,        1031,       1109,       1193,
        1289,       1381,       1493,       1613,       1741,       1879,       2029,       2179,       2357,
        2549,       2753,       2971,       3209,       3469,       3739,       4027,       4349,       4703,
        5087,       5503,       5953,       6427,       6949,       7517,       8123,       8783,       9497,
        10273,      11113,      12011,      12983,      14033,      15173,      16411,      17749,      19183,
        20753,      22447,      24281,      26267,      28411,      30727,      33223,      35933,      38873,
        42043,      45481,      49201,      53201,      57557,      62233,      67307,      72817,      78779,
        85229,      92203,      99733,      107897,     116731,     126271,     136607,     147793,     159871,
        172933,     187091,     202409,     218971,     236897,     256279,     277261,     299951,     324503,
        351061,     379787,     410857,     444487,     480881,     520241,     562841,     608903,     658753,
        712697,     771049,     834181,     902483,     976369,     1056323,    1142821,    1236397,    1337629,
        1447153,    1565659,    1693859,    1832561,    1982627,    2144977,    2320627,    2510653,    2716249,
        2938679,    3179303,    3439651,    3721303,    4026031,    4355707,    4712381,    5098259,    5515729,
        5967347,    6456007,    6984629,    7556579,    8175383,    8844859,    9569143,    10352717,   11200489,
        12117689,   13109983,   14183539,   15345007,   16601593,   17961079,   19431899,   21023161,   22744717,
        24607243,   26622317,   28802401,   31160981,   33712729,   36473443,   39460231,   42691603,   46187573,
        49969847,   54061849,   58488943,   63278561,   68460391,   74066549,   80131819,   86693767,   93793069,
        101473717,  109783337,  118773397,  128499677,  139022417,  150406843,  162723577,  176048909,  190465427,
        206062531,  222936881,  241193053,  260944219,  282312799,  305431229,  330442829,  357502601,  386778277,
        418451333,  452718089,  489790921,  529899637,  573292817,  620239453,  671030513,  725980837,  785430967,
        849749479,  919334987,  994618837,  1076067617, 1164186217, 1259520799, 1362662261, 1474249943, 1594975441,
        1725587117, 1866894511, 2019773507, 2185171673, 2364114217, 2557710269, 2767159799, 2993761039, 3238918481,
        3504151727, 3791104843, 4101556399, 4294967291};

protected:
    std::atomic<int64_t> size_{0};
    BucketAllocator *allocator_ = nullptr; /* allocate overflowed bucket */
    Bucket *subMaps_[kSubMapCount]{};      /* sub map */
    uint64_t bucketCount_ = 0;             /* bucket count of each sub map */
    uint64_t baseSize_ = 4096L;            /* base size */

    friend class FlashHashmapPersist;
};

template<typename Bucket, typename BucketAllocator>
inline FlashHashmap<Bucket, BucketAllocator>::~FlashHashmap()
{
    UnInitialize();
}

template<typename Bucket, typename BucketAllocator>
inline Result FlashHashmap<Bucket, BucketAllocator>::Initialize() noexcept
{
    return EM_OK;
}

template<typename Bucket, typename BucketAllocator>
inline void FlashHashmap<Bucket, BucketAllocator>::UnInitialize() noexcept
{}

template<typename Bucket, typename BucketAllocator>
inline uint64_t FlashHashmap<Bucket, BucketAllocator>::Size() const noexcept
{
    return size_.load();
}

template<typename Bucket, typename BucketAllocator>
inline Result FlashHashmap<Bucket, BucketAllocator>::FindOrUpsert(uint64_t key, uint64_t &value) noexcept
{
    return EM_OK;
}

template<typename Bucket, typename BucketAllocator>
inline Result FlashHashmap<Bucket, BucketAllocator>::Find(const uint64_t key, uint64_t &value) noexcept
{
    return EM_OK;
}

template<typename Bucket, typename BucketAllocator>
inline Result FlashHashmap<Bucket, BucketAllocator>::Remove(uint64_t key) noexcept
{
    return EM_OK;
}

using ReadonlyFlashHashmapPtr = EmRef<FlashHashmap<HashBucketReadonly>>;
using FlashHashmapPtr = EmRef<FlashHashmap<HashBucket>>;

} // namespace emb
} // namespace ock

#endif // MEMFABRIC_HYBRID_EMB_FAST_HASHTABLE_H
