/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#pragma once

#define EXPORT_API __attribute__((visibility("default")))

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <c10/core/Allocator.h>
#include <c10/util/Registry.h>
#include <c10/util/SmallVector.h>
#include "torch_npu/csrc/core/npu/NPUGraphsUtils.h"
#include "torch_npu/csrc/core/npu/NPUMacros.h"
#include "torch_npu/csrc/core/npu/NPUStream.h"
// #include "torch_npu/csrc/core/npu/register/OptionsManager.h"
#include "torch_npu/csrc/core/npu/NPUWorkspaceAllocator.h"
#include "torch_npu/csrc/distributed/HCCLUtils.hpp"

#include "zbal_pytorch_util.h"
#include "zbal_pytorch_mm_heap.h"

#include <mutex>
#include <atomic>

std::string format_size(uint64_t size);

namespace c10_npu {
namespace dma {

C10_NPU_API std::mutex* getFreeMutex();

// Caching allocator will execute every registered callback if it unable to find
// block inside of already allocated area.
class FreeMemoryCallback {
public:
    virtual ~FreeMemoryCallback(){};
    virtual bool Execute() = 0;
};

C10_DECLARE_REGISTRY(FreeNPUMemoryCallbacksRegistry, FreeMemoryCallback);
#define REGISTER_FREE_MEMORY_CALLBACK(name, ...) \
  C10_REGISTER_CLASS(FreeNPUMemoryCallbacksRegistry, name, __VA_ARGS__);


// (Ascend): Turn this into an honest to goodness class. I briefly attempted to do
// this, but it was a bit irritating to figure out how to also correctly
// apply pimpl pattern so I didn't have to leak any internal implementation
// details in the header (NPUShmemAllocator could be made a pimpl, but
// you also need to appropriately define a class which is a subclass
// of Allocator. Not impossible, but required a bit more surgery than
// I wanted to do at the time.)
//
// Why is this using a namespace rather than old-style THNCachingAllocator_
// prefix?  Mostly because it made the HIPify rules easier to write; _ is
// not counted as a word boundary, so you would otherwise have to list each
// of these functions.

// Struct containing memory allocator summary statistics for a device.
using Stat = c10_npu::NPUCachingAllocator::Stat;
using StatType = c10_npu::NPUCachingAllocator::StatType;
using StatArray = c10_npu::NPUCachingAllocator::StatArray;
using DeviceStats = c10_npu::NPUCachingAllocator::DeviceStats;
/*
struct Stat {
    int64_t current = 0;
    int64_t peak = 0;
    int64_t allocated = 0;
    int64_t freed = 0;
};

enum struct StatType : uint64_t {
    AGGREGATE = 0,
    SMALL_POOL = 1,
    LARGE_POOL = 2,
    NUM_TYPES = 3  // remember to update this whenever a new stat type is added
};

typedef std::array<Stat, static_cast<size_t>(StatType::NUM_TYPES)> StatArray;

struct DeviceStats {
    // COUNT: allocations requested by client code
    StatArray allocation;
    // COUNT: number of allocated segments from npuMalloc().
    StatArray segment;
    // COUNT: number of active memory blocks (allocated or used by stream)
    StatArray active;
    // COUNT: number of inactive, split memory blocks (unallocated but can't be released via npuFree)
    StatArray inactive_split;

    // SUM: bytes requested by client code
    StatArray allocated_bytes;
    // SUM: bytes reserved by this memory allocator (both free and used)
    StatArray reserved_bytes;
    // SUM: bytes within active memory blocks
    StatArray active_bytes;
    // SUM: bytes within inactive, split memory blocks
    StatArray inactive_split_bytes;
    // SUM: bytes requested by client code
    StatArray requested_bytes;

    // COUNT: total number of failed calls to NPU malloc necessitating cache flushes.
    int64_t num_alloc_retries = 0;

    // COUNT: total number of OOMs (i.e. failed calls to NPU after cache flush)
    int64_t num_ooms = 0;

    // COUNT: total number of oversize blocks allocated from pool
    Stat oversize_allocations;

    // COUNT: total number of oversize blocks requiring malloc
    Stat oversize_segments;

    // SIZE: maximum block size that is allowed to be split.
    int64_t max_split_size = 0;
};*/

typedef std::shared_ptr<c10::GatheredContext> (*CreateContextFn)(void);

// Struct containing info of an allocation block (i.e. a fractional part of a cudaMalloc)..
struct BlockInfo {
    int64_t size = 0;
    int64_t requested_size = 0;
    int32_t gc_counter = 0;
    bool allocated = false;
    bool active = false;
    std::shared_ptr<c10::GatheredContext> context_when_allocated;
};

// Struct containing info of a memory segment (i.e. one contiguous cudaMalloc).
struct SegmentInfo {
    int64_t device = 0;
    int64_t  address = 0;
    aclrtStream stream = nullptr;
    int64_t total_size = 0;
    int64_t requested_size = 0;
    int64_t allocated_size = 0;
    int64_t active_size = 0;
    bool is_large = false;
    bool is_expandable = false;
    MempoolId_t owner_private_pool_id = {0, 0};
    std::vector<BlockInfo> blocks;
    std::shared_ptr<c10::GatheredContext> context_when_allocated;
};

struct AllocatorState {
    virtual ~AllocatorState() = default;
};

struct TraceEntry {
    enum Action {
        ALLOC,          // API made to the caching allocator for new memory
        FREE_REQUESTED, // API call made to the caching allocator to free memory
        FREE_COMPLETED, // The allocator might have to delay a free because
                        // it is still in use on another stream via
                        // record_stream This event is generated when a free
                        // actually completes.
        SEGMENT_ALLOC, // a call to AclrtMalloc to get more memory from the OS
        SEGMENT_FREE, // a call to aclrtFree to return memory to the OS (e.g. to
                      // defragment or empty_caches)
        SEGMENT_MAP,  // a call to AclrtMapMem (used with expandable_segments)
        SEGMENT_UNMAP, // unmap part of a segment (used with expandable
                       // segments)
        SNAPSHOT, // a call to snapshot, used to correlate memory snapshots to
                  // trace events
        OOM, // the allocator threw an OutOfMemoryError (addr_ is the amount of
            // free bytes reported by cuda)
        WORKSPACE_SNAPSHOT,
        EMPTY_CACHE
    };
    TraceEntry(Action action, int device, int64_t addr, size_t size,
               aclrtStream stream,
               std::shared_ptr<c10::GatheredContext> context = nullptr)
        : action_(action), device_(device), addr_(addr),
          context_(std::move(context)), stream_(stream), size_(size)
    {
    }
    Action action_;
    int device_;
    int64_t addr_; // for OOM, this is the amount of free bytes reported by cuda
    std::shared_ptr<c10::GatheredContext> context_;
    aclrtStream stream_;
    int64_t size_;
};

struct SnapshotInfo {
    std::vector<SegmentInfo> segments;
    std::vector<std::vector<TraceEntry> > device_traces;
};

// returns the pointers freed in the pool
// and the pointers allocated. Note: a pointer
// may appear in both freed and allocated
struct CheckpointDelta {
    std::vector<void*> ptrs_freed;
    std::vector<at::DataPtr> dataptrs_allocd;
};

enum struct RecordContext {
    NEVER = 0,
    STATE = 1, // only keep stacks for active allocations
    ALLOC = 2, // additionally keep stacks for allocations in the trace history
    ALL = 3,   // additionally record stacks for when something is freed
};

using OutOfMemoryObserver =
    std::function<void(int64_t device, int64_t allocated, int64_t device_total,
                       int64_t device_free)>;

struct ShareableHandle {
    ptrdiff_t offset;
    std::string handle;
};

class NPUAllocator : public c10::Allocator {
public:
    virtual c10::DataPtr allocate_with_aligned(size_t size, size_t aligned) const = 0;
    virtual void* raw_alloc(size_t nbytes) = 0;
    virtual void* raw_alloc_with_stream(size_t nbytes, aclrtStream stream) = 0;
    virtual void raw_delete(void* ptr) = 0;
    virtual void init(int device_count) = 0;
    virtual bool initialized() = 0;
    virtual void setMemoryFraction(double fraction, int device) = 0;
    virtual void emptyCacheImpl(bool check_error, bool free_physical) = 0;
    virtual void emptyCache(bool check_error) = 0;
    virtual void emptyVirtAddrCache(bool check_error) = 0;
    virtual void cacheInfo(int dev_id, size_t* cachedAndFree, size_t* largestBlock) = 0;
    virtual void* getBaseAllocation(void* ptr, size_t* size) = 0;
    virtual void recordStream(void *ptr, c10_npu::NPUStream stream) = 0;
    virtual void eraseStream(void *ptr, c10_npu::NPUStream stream) = 0;
    virtual DeviceStats getDeviceStats(int device) = 0;
    virtual void resetAccumulatedStats(int device) = 0;
    virtual void resetPeakStats(int device) = 0;
    virtual SnapshotInfo snapshot() = 0;

    // CUDAGraph interactions
    virtual void beginAllocateToPool(
        c10::DeviceIndex device,
        MempoolId_t mempool_id,
        std::function<bool(aclrtStream)> filter) = 0;
    virtual void endAllocateToPool(
        c10::DeviceIndex device,
        MempoolId_t mempool_id) = 0;
    virtual void releasePool(c10::DeviceIndex device, MempoolId_t mempool_id) = 0;
    virtual void FreeDeviceCachedMemory(int device) = 0;
    virtual std::string name() = 0;
    virtual bool checkPoolLiveAllocations(
        c10::DeviceIndex device,
        MempoolId_t mempool_id,
        const std::unordered_set<void*>& expected_live_allocations)
    {
        (void)device;
        (void)mempool_id;
        (void)expected_live_allocations;
        TORCH_CHECK(false, name(),
            " does not yet support checkPoolLiveAllocations. "
            "If you need it, please file an issue describing your use case.", PTA_ERROR_MOCK(ErrCode::NOT_SUPPORT));
    }
    virtual ShareableHandle shareIpcHandle(void* ptr) = 0;
    virtual std::shared_ptr<void> getIpcDevPtr(std::string handle) = 0;
    virtual bool isHistoryEnabled()
    {
        TORCH_CHECK(
            false, name(),
            " does not yet support recordHistory. "
            "If you need it, please file an issue describing your use case.", PTA_ERROR_MOCK(ErrCode::NOT_SUPPORT));
    }
    virtual void recordHistory(bool enabled, CreateContextFn context_recorder,
                               size_t alloc_trace_max_entries,
                               RecordContext when) = 0;
    virtual void attachOutOfMemoryObserver(OutOfMemoryObserver observer) = 0;
    virtual bool checkUceInMemPool(int device) = 0;
    virtual bool checkBlockIsSafe(const c10::DataPtr& ptr) = 0;
    virtual void markAllBlockUnsafe(int device) = 0;
    virtual void updateBlockToSafe(const c10::DataPtr &ptr) = 0;
    virtual void cleanEvent() = 0;
    virtual void buildServerMemMapForHccl(int device, std::shared_ptr<c10d_npu::HCCLComm> hcclComm) {
        (void)device;
        (void)hcclComm;
    }
    virtual std::shared_ptr<AllocatorState> getCheckpointState(
        c10::DeviceIndex device,
        MempoolId_t id) = 0;
    virtual CheckpointDelta setCheckpointPoolState(
        c10::DeviceIndex device,
        std::shared_ptr<AllocatorState> pps) = 0;
};

bool checkConfigExpandableSegments();

bool isConfig1GPageSizeEnable();

} // namespace dma
} // namespace c10_npu


void finalize();

void pybind11_allocator(pybind11::module_ &m);

extern "C" {
EXPORT_API void *dma_malloc(size_t size, int device, aclrtStream stream);

EXPORT_API void dma_init(int device_count);

EXPORT_API void dma_empty_cache(bool check_error);

EXPORT_API void dma_free(void *ptr, size_t size, int device, aclrtStream stream);

EXPORT_API void dma_record_stream(void *ptr, c10_npu::NPUStream stream);

EXPORT_API void dma_erase_stream(void *ptr, c10_npu::NPUStream stream);

EXPORT_API void dma_begin_allocate_to_pool(int device, c10_npu::MempoolId_t mempool_id, std::function<bool(aclrtStream)> filter);

EXPORT_API void dma_end_allocate_to_pool(int device, c10_npu::MempoolId_t mempool_id);

EXPORT_API void dma_release_pool(int device, c10_npu::MempoolId_t mempool_id);

EXPORT_API c10_npu::NPUCachingAllocator::DeviceStats dma_get_device_stats(int device);

EXPORT_API void *dma_get_base_addr(int device = -1);

EXPORT_API void dma_init_heap(void *base_ptr, uint64_t local_mem_size, bool is_simulation = false);

EXPORT_API void dma_get_heap_stats(size_t &in_used_size, size_t &total_size, int device = -1);
}

void dma_record_memory_history(std::optional<std::string> enabled, int64_t max_entries);

pybind11::dict dma_dump_snapshot();
