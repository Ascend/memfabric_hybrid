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
#ifndef MEMFABRIC_HYBRID_EMB_FLASH_HASHMAP_BUCK_H
#define MEMFABRIC_HYBRID_EMB_FLASH_HASHMAP_BUCK_H

#include "emb_flash_hashmap_types.h"
#include "emb_flash_bucket_allocator.h"

namespace ock {
namespace emb {
namespace hashmap {
/**
 * @brief Mutable hash bucket for flash persist and recover
 */
struct FlashHashBucket {
    /**
     * @brief Insert key and value into this bucket, with spinlock
     *
     * @param pKey         [in] key to be inserted
     * @param pValue       [in] value to be inserted
     * @return 0 if successfully inserted
     */
    Result Insert(uint64_t pKey, uint64_t pValue) noexcept;

    /**
     * @brief Get value by key, with spinlock
     *
     * @param pKey         [in] key to be found
     * @param pValue       [out] value that found
     * @return 0 if found
     */
    Result Get(uint64_t pKey, uint64_t &pValue) noexcept;

    /**
     * @brief Get value by key, if not found insert one and return the address of value
     *
     * @param pKey         [in] key to be found or inserted
     * @param pValue       [out] value of the key if found
     * @param pValueAddress[out] address of value of new inserted
     * @return 0 if found, EM_NOT_FOUND_BUT_INSERTED if not found but inserted
     */
    Result GetOrInsert(uint64_t pKey, uint64_t &pValue, uintptr_t &pValueAddress) noexcept;

    /**
     * @brief Get value by key without lock
     *
     * @param pKey         [in] key to be found
     * @param pValue       [out] value that found
     * @return 0 if found
     */
    Result GetWithoutLock(uint64_t pKey, uint64_t &pValue) noexcept;

    /**
     * @brief Get the last key and erase it, without spinlock
     *
     * @param pkey         [out] the key of last one
     * @param pValue       [out] the value of last one
     * @return 0 if there is last one
     */
    Result GetAndEraseLast(uint64_t &pKey, uint64_t &pValue) noexcept;

    /**
     * @brief Replace the key and value with new one if found, without spinlock
     *
     * @param originalKey  [in] the key to be replaced
     * @param pKey         [in] new key
     * @param pValue       [in] new value
     * @return 0 if successful
     */
    Result Replace(uint64_t originalKey, uint64_t pKey, uint64_t pValue) noexcept;

    /**
     * @brief Get va of next overflowed bucket
     * @return real va of next bucket
     */
    FlashHashBucket *Next() const noexcept;

    /**
     * @brief Lock in spin way
     */
    void Lock() noexcept;

    /**
     * @brief Unlock
     */
    void UnLock() noexcept;

    /**
     * @brief Check if has next bucket without lock
     *
     * @return true if has
     */
    bool HasNextWithoutLock() const noexcept;

    /**
     * @brief Link next lock without lock
     *
     * @param baseAddress  [in] base address
     * @param offset       [in] offset to base address
     * @return 0 if linked
     */
    Result LinkNextWithoutLock(uint64_t baseAddress, uint64_t offset) noexcept;

    /**
     * @brief operator <<
     */
    friend std::ostream &operator<<(std::ostream &os, const FlashHashBucket &o);

    /*
     * important:
     * 1 make sure size of this structure is 64 bytes, which same with one CPU cacheline,
     * 2 make sure init value of variables are 0 for fast bucket allocation when overflowed
     */
    uint64_t key[UN3]{INVALID_MAP_KEY, INVALID_MAP_KEY, INVALID_MAP_KEY};
    uint64_t value[UN3]{INVALID_MAP_VALUE, INVALID_MAP_VALUE, INVALID_MAP_VALUE};
    uint64_t bucketBaseAddress = 0;  /* a united based for all overflowed bucket in one hashmap */
    uint32_t offset2BaseAddress = 0; /* offset to base address, 4GB offset is big enough */
    uint16_t lock = 0;               /* spin lock data */
    uint16_t hasNext = 0;            /* if linked next bucket */
} __attribute__((aligned(8)));

EM_ALWAYS_INLINE Result FlashHashBucket::Insert(uint64_t pKey, uint64_t pValue) noexcept
{
    if (UNLIKELY(pKey == INVALID_MAP_KEY)) {
        return EM_HASHMAP_INVALID_KEY;
    }

    /* lock and insert, don't do loop here */
    Lock();
    if (key[UN0] == INVALID_MAP_KEY) {
        key[UN0] = pKey;
        value[UN0] = pValue;
        UnLock();
        return EM_OK;
    }

    if (key[UN1] == INVALID_MAP_KEY) {
        key[UN1] = pKey;
        value[UN1] = pValue;
        UnLock();
        return EM_OK;
    }

    if (key[UN2] == INVALID_MAP_KEY) {
        key[UN2] = pKey;
        value[UN2] = pValue;
        UnLock();
        return EM_OK;
    }

    UnLock();
    return EM_HASHMAP_BUCKET_FULL;
}

EM_ALWAYS_INLINE Result FlashHashBucket::Get(uint64_t pKey, uint64_t &pValue) noexcept
{
    if (UNLIKELY(pKey == INVALID_MAP_KEY)) {
        return EM_HASHMAP_INVALID_KEY;
    }

    /* lock and insert, don't do loop here */
    Lock();
    if (key[UN0] == pKey) {
        pValue = value[UN0];
        UnLock();
        return EM_OK;
    }

    if (key[UN1] == pKey) {
        pValue = value[UN1];
        UnLock();
        return EM_OK;
    }

    if (key[UN2] == pKey) {
        pValue = value[UN2];
        UnLock();
        return EM_OK;
    }

    UnLock();

    return EM_HASHMAP_KEY_NOT_FOUND;
}

EM_ALWAYS_INLINE Result FlashHashBucket::GetOrInsert(uint64_t pKey, uint64_t &pValue, uintptr_t &pValueAddress) noexcept
{
    if (UNLIKELY(pKey == INVALID_MAP_KEY)) {
        return EM_HASHMAP_INVALID_KEY;
    }

    pValue = INVALID_MAP_VALUE;
    pValueAddress = 0;

    /* lock and insert, don't do loop here */
    Lock();
    if (key[UN0] == pKey) {
        pValue = value[UN0];
        UnLock();
        return EM_OK;
    } else if (key[UN0] == INVALID_MAP_KEY) {
        key[UN0] = pKey;
        pValueAddress = reinterpret_cast<uintptr_t>(&value[UN0]);
        UnLock();
        return EM_HASHMAP_NOT_FOUND_BUT_INSERTED;
    }

    if (key[UN1] == pKey) {
        pValue = value[UN1];
        UnLock();
        return EM_OK;
    } else if (key[UN1] == INVALID_MAP_KEY) {
        key[UN1] = pKey;
        pValueAddress = reinterpret_cast<uintptr_t>(&value[UN1]);
        UnLock();
        return EM_HASHMAP_NOT_FOUND_BUT_INSERTED;
    }

    if (key[UN2] == pKey) {
        pValue = value[UN2];
        UnLock();
        return EM_OK;
    } else if (key[UN2] == INVALID_MAP_KEY) {
        key[UN2] = pKey;
        pValueAddress = reinterpret_cast<uintptr_t>(&value[UN2]);
        UnLock();
        return EM_HASHMAP_NOT_FOUND_BUT_INSERTED;
    }

    UnLock();
    return EM_HASHMAP_BUCKET_FULL;
}

EM_ALWAYS_INLINE Result FlashHashBucket::GetWithoutLock(uint64_t pKey, uint64_t &pValue) noexcept
{
    if (UNLIKELY(pKey == INVALID_MAP_KEY)) {
        return EM_HASHMAP_INVALID_KEY;
    }

    /* lock and insert, don't do loop here */
    if (key[UN0] == pKey) {
        pValue = value[UN0];
        return EM_OK;
    }

    if (key[UN1] == pKey) {
        pValue = value[UN1];
        return EM_OK;
    }

    if (key[UN2] == pKey) {
        pValue = value[UN2];
        return EM_OK;
    }

    return EM_HASHMAP_KEY_NOT_FOUND;
}

EM_ALWAYS_INLINE Result FlashHashBucket::GetAndEraseLast(uint64_t &pKey, uint64_t &pValue) noexcept
{
    /* don't lock here, we do lock outside */
    auto buck = this;
    while (buck != nullptr) {
        /* if the first one key invalid, means the bucket is empty and no linked bucketed */
        if (buck->key[UN0] == INVALID_MAP_KEY) {
            return EM_HASHMAP_KEY_NOT_FOUND;
        }

        /* if the second key is invalid, first one must be valid */
        if (buck->key[UN1] == INVALID_MAP_KEY) {
            pKey = buck->key[UN0];
            pValue = buck->value[UN0];
            buck->key[UN0] = INVALID_MAP_KEY;
            buck->value[UN0] = INVALID_MAP_VALUE;
            return EM_OK;
        }

        /* if the third key is invalid, second one must be valid */
        if (buck->key[UN2] == INVALID_MAP_KEY) {
            pKey = buck->key[UN1];
            pValue = buck->value[UN1];
            buck->key[UN1] = INVALID_MAP_KEY;
            buck->value[UN1] = INVALID_MAP_VALUE;
            return EM_OK;
        }

        /* if next is nullptr, third one must be valid */
        if (buck->Next() == nullptr) {
            pKey = buck->key[UN2];
            pValue = buck->value[UN2];
            buck->key[UN2] = INVALID_MAP_KEY;
            buck->value[UN2] = INVALID_MAP_VALUE;
            return EM_OK;
        }

        /* if next is not nullptr, but no key */
        if (buck->Next()->key[UN0] == INVALID_MAP_KEY) {
            pKey = buck->key[UN2];
            pValue = buck->value[UN2];
            buck->key[UN2] = INVALID_MAP_KEY;
            buck->value[UN2] = INVALID_MAP_VALUE;
            return EM_OK;
        }

        /* if the 2nd and 3rd are valid key, move to next */
        buck = buck->Next();
    }

    return EM_HASHMAP_KEY_NOT_FOUND;
}

EM_ALWAYS_INLINE Result FlashHashBucket::Replace(uint64_t originalKey, uint64_t pKey, uint64_t pValue) noexcept
{
    if (UNLIKELY(pKey == INVALID_MAP_KEY || originalKey == INVALID_MAP_KEY)) {
        return EM_HASHMAP_INVALID_KEY;
    }

    /* do this without lock, we have lock outside */
    if (key[UN0] == originalKey) {
        key[UN0] = pKey;
        value[UN0] = pValue;
        return EM_OK;
    }

    if (key[UN1] == originalKey) {
        key[UN1] = pKey;
        value[UN1] = pValue;
        return EM_OK;
    }

    if (key[UN2] == originalKey) {
        key[UN2] = pKey;
        value[UN2] = pValue;
        return EM_OK;
    }

    return EM_HASHMAP_KEY_NOT_FOUND;
}

EM_ALWAYS_INLINE FlashHashBucket *FlashHashBucket::Next() const noexcept
{
    /* if no base address set, means next is null */
    if (!HasNextWithoutLock()) {
        return nullptr;
    }

    /* offset + base address and convert */
    return reinterpret_cast<FlashHashBucket *>(bucketBaseAddress + offset2BaseAddress);
}

EM_ALWAYS_INLINE void FlashHashBucket::Lock() noexcept
{
    while (!__sync_bool_compare_and_swap(&lock, 0, 1)) {}
}

EM_ALWAYS_INLINE void FlashHashBucket::UnLock() noexcept
{
    __atomic_store_n(&lock, 0, __ATOMIC_SEQ_CST);
}

EM_ALWAYS_INLINE bool FlashHashBucket::HasNextWithoutLock() const noexcept
{
    return (hasNext == 1);
}

EM_ALWAYS_INLINE Result FlashHashBucket::LinkNextWithoutLock(uint64_t baseAddress, uint64_t offset) noexcept
{
    if (UNLIKELY(baseAddress == 0)) {
        return EM_INVALID_PARAM;
    }

    if (HasNextWithoutLock()) {
        return EM_HASHMAP_ALREADY_HAS_NEXT_BUCKET;
    }

    bucketBaseAddress = baseAddress;
    offset2BaseAddress = offset;
    hasNext = 1;
    return EM_OK;
}

inline std::ostream &operator<<(std::ostream &os, const FlashHashBucket &o)
{
    os << "FlashHashBucket keyValue: [[" << o.key[UN0] << "," << o.value[UN0] << "],[" << o.key[UN1] << ","
       << o.value[UN1] << "],[" << o.key[UN2] << "," << o.value[UN2] << "]], hasNext: " << o.hasNext
       << ", NextBuckBaseAddress: " << o.bucketBaseAddress << ", offset: " << o.offset2BaseAddress
       << ", locked: " << o.lock;
    return os;
}

} // namespace hashmap
} // namespace emb
} // namespace ock

#endif // MEMFABRIC_HYBRID_EMB_FLASH_HASHMAP_BUCK_H
