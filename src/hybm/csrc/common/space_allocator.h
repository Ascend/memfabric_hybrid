/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MEMORYFABRIC_SPACE_ALLOCATOR_H
#define MEMORYFABRIC_SPACE_ALLOCATOR_H

#include <cstdint>
#include <memory>

namespace ock::mf {
class AllocatedElement {
public:
    AllocatedElement() noexcept : startAddress{nullptr}, size{0} {}

    explicit AllocatedElement(uint8_t *p, uint64_t s) noexcept : startAddress{p}, size{s} {}

    uint8_t *Address() noexcept
    {
        return startAddress;
    }

    const uint8_t *Address() const noexcept
    {
        return startAddress;
    }

    uint64_t Size() const noexcept
    {
        return size;
    }

    void Reset() noexcept
    {
        startAddress = nullptr;
        size = 0;
    }

private:
    uint8_t *startAddress;
    uint64_t size;
};

class SpaceAllocator {
public:
    virtual ~SpaceAllocator() = default;

public:
    virtual bool CanAllocate(uint64_t size) const noexcept = 0;
    virtual AllocatedElement Allocate(uint64_t size) noexcept = 0;
    virtual bool Release(const AllocatedElement &element) noexcept = 0;
};

using SpaceAllocatorPtr = std::shared_ptr<SpaceAllocator>;
}

#endif  // MEMORYFABRIC_SPACE_ALLOCATOR_H
