/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef MF_HYBM_RBTREE_RANGEALLOC_H
#define MF_HYBM_RBTREE_RANGEALLOC_H

#include <pthread.h>
#include <map>
#include <set>
#include "hybm_space_allocator.h"

namespace ock::mf {
struct SpaceRange {
    const uint64_t offset;
    const uint64_t size;

    SpaceRange(uint64_t o, uint64_t s) noexcept : offset{o}, size{s} {}
};

struct RangeSizeFirst {
    bool operator()(const SpaceRange &sr1, const SpaceRange &sr2) const noexcept;
};

class RbtreeRangePool : public SpaceAllocator {
public:
    RbtreeRangePool(uint8_t *address, uint64_t size) noexcept;
    ~RbtreeRangePool() noexcept override;

public:
    bool CanAllocate(uint64_t size) const noexcept override;
    AllocatedElement Allocate(uint64_t size) noexcept override;
    bool Release(const AllocatedElement &element) noexcept override;

private:
    static uint64_t AllocateSizeAlignUp(uint64_t inputSize) noexcept;

private:
    uint8_t *const baseAddress;
    const uint64_t totalSize;
    mutable pthread_spinlock_t lock{};
    std::map<uint64_t, uint64_t> addressTree;
    std::set<SpaceRange, RangeSizeFirst> sizeTree;
};
}

#endif  // MF_HYBM_RBTREE_RANGEALLOC_H
