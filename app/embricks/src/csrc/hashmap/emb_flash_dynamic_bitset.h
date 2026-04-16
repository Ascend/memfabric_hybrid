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
#ifndef MEMFABRIC_HYBRID_EMB_FLASH_DYNAMIC_BITSET_H
#define MEMFABRIC_HYBRID_EMB_FLASH_DYNAMIC_BITSET_H

#include "emb_common_includes.h"

namespace ock {
namespace emb {
namespace hashmap {

constexpr int32_t DBS_CHUCK_SHIFT = 6;            // right shift bits
constexpr int32_t DBS_CHUCK_MASK = 63;            // mask for bit ops
#define DBS_CHUCK_POS(x) ((x) >> DBS_CHUCK_SHIFT) // chuck pos
#define DBS_BIT_POS(x)   ((x) & DBS_CHUCK_MASK)   // bit pos

/**
 * A bitset which dynamic bit count
 */
class FlashDynamicBitSet {
public:
    FlashDynamicBitSet() = default;
    ~FlashDynamicBitSet();

    FlashDynamicBitSet(const FlashDynamicBitSet &) = delete;
    FlashDynamicBitSet(FlashDynamicBitSet &&) = delete;
    FlashDynamicBitSet &operator=(const FlashDynamicBitSet &) = delete;
    FlashDynamicBitSet &operator=(FlashDynamicBitSet &&) = delete;

    /**
     * @brief Initialize the bitset with certain capacity
     *
     * @param capacity     [in] capacity
     * @return 0 if successful
     */
    Result Initialize(uint32_t capacity) noexcept;

    /**
     * @brief Un-initialize the dynamic bit set, with release memory
     */
    void UnInitialize() noexcept;

    /*
     * @brief SetWrite the pos to true
     *
     * @param pos          [in] position of bit
     */
    void Set(uint32_t pos) noexcept;

    /*
     * @brief SetWrite the post to false
     *
     * @param pos          [in] position of bit
     */
    bool Clear(uint32_t pos) noexcept;

    /*
     * @brief Check the bit at position is true or not
     *
     * @param pos          [in] position of bit
     */
    bool Test(uint32_t pos) const noexcept;

    /*
     * @brief Count the number of bits of true
     */
    uint32_t Count() const noexcept;

    /*
     * @brief Check if all bits are true
     */
    bool Full() const noexcept;

    /**
     * @brief Capacity
     *
     * @return capacity of this bitset
     */
    uint32_t Capacity() const noexcept;

    /*
     * @brief Find the next bit pos which is not true
     *
     * @param startPos     [in] the start position to find
     * @param resultPos    [out] the position of result
     *
     * @return true if found
     */
    bool FindAndSet(uint32_t startPos, uint32_t &resultPos) noexcept;

private:
    bool TestInner(uint32_t pos) const noexcept;

private:
    uint64_t *bitChucks_ = nullptr;
    uint32_t chuckCount_ = 0;
    uint32_t capacity_ = 0;
    uint32_t trueCount_ = 0;
};

inline Result FlashDynamicBitSet::Initialize(uint32_t capacity) noexcept
{
    if (capacity == 0) {
        EM_LOG_ERROR("Invalid capacity " << capacity << ", should be not 0");
        return EM_INVALID_PARAM;
    }

    /* use bitChucks_ as the flag for initialization */
    if (bitChucks_ != nullptr) {
        EM_LOG_DEBUG("DynamicBitSet already initialized");
        return EM_OK;
    }

    /* assign capacity */
    capacity_ = capacity;

    /* calculate the size of chucks to store bits */
    chuckCount_ = (capacity_ + DBS_CHUCK_MASK) >> DBS_CHUCK_SHIFT;
    EM_LOG_DEBUG("chuckCount: " << chuckCount_ << ", capacity: " << capacity_);

    /* allocate memory for chucks */
    bitChucks_ = new (std::nothrow) uint64_t[chuckCount_];
    if (bitChucks_ == nullptr) {
        EM_LOG_ERROR("Initialize DynamicBitSet failed, probably out of memory");
        return EM_NEW_OBJ_FAILED;
    }

    /* allocate physical memory and set to 0 */
    bzero(reinterpret_cast<void *>(bitChucks_), 8L * chuckCount_);

    trueCount_ = 0;

    EM_LOG_DEBUG("DynamicBitset initialized, this: " << std::hex << this << ", bitChucks: " << bitChucks_ << std::dec
                                                     << ", capacity: " << capacity_ << ", chuckCount: " << chuckCount_
                                                     << ", trueCount: " << trueCount_);

    return EM_OK;
}

inline void FlashDynamicBitSet::UnInitialize() noexcept
{
    if (bitChucks_ == nullptr) {
        return;
    }

    capacity_ = 0;
    trueCount_ = 0;
    chuckCount_ = 0;

    delete[] bitChucks_;
    bitChucks_ = nullptr;

    EM_LOG_DEBUG("DynamicBitset uninitialized");
}

inline FlashDynamicBitSet::~FlashDynamicBitSet()
{
    UnInitialize();
}

inline void FlashDynamicBitSet::Set(uint32_t pos) noexcept
{
    if (UNLIKELY(pos >= capacity_)) {
        EM_LOG_WARN("Invalid pos " << pos << " exceed capacity " << capacity_);
        return;
    }

    /* if already set */
    if (TestInner(pos)) {
        return;
    }

    /* set the bit in chucks to true */
    bitChucks_[DBS_CHUCK_POS(pos)] |= (1uL) << DBS_BIT_POS(pos);
    ++trueCount_;
}

inline bool FlashDynamicBitSet::Clear(uint32_t pos) noexcept
{
    if (UNLIKELY(pos >= capacity_)) {
        return false;
    }

    if (!TestInner(pos)) {
        return true;
    }

    /* set the bit in chucks to false i.e. 0 */
    bitChucks_[DBS_CHUCK_POS(pos)] &= ~((1uL) << DBS_BIT_POS(pos));
    --trueCount_;

    return true;
}

inline bool FlashDynamicBitSet::TestInner(uint32_t pos) const noexcept
{
    /* test bit */
    return (bitChucks_[DBS_CHUCK_POS(pos)] & ((1uL) << DBS_BIT_POS(pos))) != 0;
}

inline bool FlashDynamicBitSet::Test(uint32_t pos) const noexcept
{
    if (UNLIKELY(pos >= capacity_)) {
        return false;
    }

    return TestInner(pos);
}

inline uint32_t FlashDynamicBitSet::Count() const noexcept
{
    return trueCount_;
}

inline bool FlashDynamicBitSet::Full() const noexcept
{
    return trueCount_ == capacity_;
}

inline uint32_t FlashDynamicBitSet::Capacity() const noexcept
{
    return capacity_;
}

inline bool FlashDynamicBitSet::FindAndSet(uint32_t startPos, uint32_t &resultPos) noexcept
{
    if (UNLIKELY(startPos >= capacity_ || capacity_ == trueCount_)) {
        EM_LOG_ERROR("Invalid params, may be start pos or true count exceed capacity.");
        return false;
    }

    for (uint32_t i = DBS_CHUCK_POS(startPos); i < chuckCount_; ++i) {
        /* if the whole uint64_t is all true bits */
        if (bitChucks_[i] == UINT64_MAX) {
            continue;
        }

        /* if not all true bits, find available bit */
        auto &wholeValue = bitChucks_[i];
        uint32_t bitIdx = static_cast<uint32_t>(__builtin_ffsll(~wholeValue));
        if (UNLIKELY(bitIdx == 0)) {
            continue;
        }

        /* calc result pos */
        resultPos = (i << DBS_CHUCK_SHIFT) + --bitIdx;
        if (UNLIKELY(resultPos >= capacity_)) {
            resultPos = 0;
            return false;
        }

        // set bit to true
        wholeValue |= ((1uL) << DBS_BIT_POS(bitIdx));
        ++trueCount_;

        return true;
    }

    return false;
}

} // namespace hashmap
} // namespace emb
} // namespace ock

#endif // MEMFABRIC_HYBRID_EMB_FLASH_DYNAMIC_BITSET_H
