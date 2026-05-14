#ifndef TORCH_NPU_CSRC_CORE_NPU_NPU_CACHING_ALLOCATOR_H_
#define TORCH_NPU_CSRC_CORE_NPU_NPU_CACHING_ALLOCATOR_H_

#include <cstdint>
#include <array>

namespace c10_npu {
namespace NPUCachingAllocator {

enum class StatType : size_t {
    AGGREGATED_ALLOCATED_BYTES = 0,
    AGGREGATED_RESERVED_BYTES,
    AGGREGATED_ACTIVE_BYTES,
    AGGREGATED_INACTIVE_BYTES,
    AGGREGATED_EVENTS_COUNT,
    AGGREGATED_EVENTS_FREED,
    AGGREGATED_EVENTS_REUSED,
    NUM_TYPES
};

struct Stat {
    int64_t current = 0;
    int64_t peak = 0;
    int64_t allocated = 0;
    int64_t freed = 0;
};

using StatArray = std::array<Stat, static_cast<size_t>(StatType::NUM_TYPES)>;

struct DeviceStats {
    StatArray array{};
};

} // namespace NPUCachingAllocator
} // namespace c10_npu

#endif // TORCH_NPU_CSRC_CORE_NPU_NPU_CACHING_ALLOCATOR_H_
