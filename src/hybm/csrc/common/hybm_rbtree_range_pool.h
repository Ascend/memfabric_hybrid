/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/

#ifndef MF_HYBM_RBTREE_RANGEALLOC_H
#define MF_HYBM_RBTREE_RANGEALLOC_H

#include <pthread.h>
#include <map>
#include <set>
#include "hybm_space_allocator.h"

namespace ock {
namespace mf {
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
} // namespace mf
} // namespace ock

#endif // MF_HYBM_RBTREE_RANGEALLOC_H
