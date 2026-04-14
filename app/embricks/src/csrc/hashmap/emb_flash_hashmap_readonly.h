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
#ifndef MEMFABRIC_HYBRID_EMB_READONLY_FLASH_HASHMAP_H
#define MEMFABRIC_HYBRID_EMB_READONLY_FLASH_HASHMAP_H

#include "emb_flash_hashmap_types.h"

namespace ock {
namespace emb {
namespace hashmap {

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
struct ReadonlyHashBucket {
    /**
     * @brief Get value by key in the bucket
     *
     * @param key          [in] key to be found
     * @param value        [out] value that found
     * @return 0 if found, EM_HASHMAP_KEY_NOT_FOUND if not found
     */
    Result Get(uint64_t key, uint64_t &value) noexcept;

    uint64_t key_[UN3]{kInvalidMapKey, kInvalidMapKey, kInvalidMapKey};         /* initialize with invalid key */
    uint64_t value_[UN3]{kInvalidMapValue, kInvalidMapValue, kInvalidMapValue}; /* initialize with invalid value */
    ReadonlyHashBucket *next_ = nullptr;                                        /* pointer to next bucket */
    BucketSpinLock spinLock_{};                                                 /* spin lock of next bucket */
} __attribute__((aligned(8)));

EM_ALWAYS_INLINE Result ReadonlyHashBucket::Get(uint64_t key, uint64_t &value) noexcept
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

    return EM_HASHMAP_KEY_NOT_FOUND;
}

/**
 * Flash hash map, which un-ordered
 */
class ReadonlyFlashHashmap : public EmReferable {
public:
    ReadonlyFlashHashmap() = default;
    ~ReadonlyFlashHashmap() override;

    /**
     * @brief Recover from file
     *
     * @param options      [in] options of recover
     * @return 0 if successful
     */
    Result Initialize(const FlashHashmapRecoverOptions &options) noexcept;

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
     * @brief Find the value by key
     *
     * @param key          [in] key to be found
     * @param value        [out] value of the key
     * @return 0 if found
     */
    Result Find(const uint64_t key, uint64_t &value) noexcept;

    /**
     * @brief Get bucket count per sub map
     *
     * @return bucket count of each sub map
     */
    uint32_t BucketCount() const noexcept;

    /**
     * @brief Check if the hashmap is initialized successfully or not
     *
     * @return true if initialized
     */
    bool Initialized() const noexcept;

    /**
     * @brief stream output
     */
    friend std::ostream &operator<<(std::ostream &os, const ReadonlyFlashHashmap &o)
    {
        os << "ReadonlyFlashHashmap [inited: " << o.inited_ << ", size_: " << o.size_
           << ", bucketCount_: " << o.bucketCount_ << ", kSubMapCount: " << kSubMapCount << "]";

        return os;
    }

protected:
    uint64_t size_{0};                            /* item size of the map*/
    bool inited_ = false;                         /* flag for initialization */
    uint32_t bucketCount_ = 0;                    /* bucket count of each sub map */
    ReadonlyHashBucket *subMaps_[kSubMapCount]{}; /* sub maps */

    friend class ReadonlyFlashHashmapPersist;
};

using ReadonlyFlashHashmapPtr = EmRef<ReadonlyFlashHashmap>;

inline ReadonlyFlashHashmap::~ReadonlyFlashHashmap()
{
    UnInitialize();
}

inline Result ReadonlyFlashHashmap::Initialize(const FlashHashmapRecoverOptions &options) noexcept
{
    return EM_OK;
}

inline void ReadonlyFlashHashmap::UnInitialize() noexcept {}

EM_ALWAYS_INLINE uint64_t ReadonlyFlashHashmap::Size() const noexcept
{
    return size_;
}

EM_ALWAYS_INLINE Result ReadonlyFlashHashmap::Find(const uint64_t key, uint64_t &value) noexcept
{
    if (UNLIKELY(key == kInvalidMapKey)) {
        return EM_HASHMAP_INVALID_KEY;
    }

    return EM_OK;
}

inline uint32_t ReadonlyFlashHashmap::BucketCount() const noexcept
{
    return bucketCount_;
}

inline bool ReadonlyFlashHashmap::Initialized() const noexcept
{
    return inited_;
}

} // namespace hashmap
} // namespace emb
} // namespace ock

#endif // MEMFABRIC_HYBRID_EMB_READONLY_FLASH_HASHMAP_H
