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
#ifndef ZBAL_SMA_DEVICE_POOL_H
#define ZBAL_SMA_DEVICE_POOL_H

#include "zbal_sma_common.h"
#include "zbal_sma_config.h"

using StreamSet = class ska::flat_hash_set<c10_npu::NPUStream>;
using ZEvent = std::unique_ptr<c10_npu::NPUEvent, std::function<void(c10_npu::NPUEvent *)>>;

namespace zbal {
namespace sma {
namespace device {

struct DeviceBlockPool;

/**
 * @brief DeviceBlock
 */
struct DeviceBlock {
    int32_t deviceId_{-1};                 // device Id
    aclrtStream stream_{nullptr};          // allocation stream
    StreamSet stream_uses_;                // streams on which the block was used
    size_t size_{0};                       // block size in bytes
    size_t requested_size_{0};             // memory originally requested
    DeviceBlockPool *pool_{nullptr};       // owning block pool
    DeviceBlockType block_type_{BT_SMALL}; // block type in pool
    void *ptr_{nullptr};                   // memory address
    DeviceBlock *prev_{nullptr};           // prev block if split from a larger allocation
    DeviceBlock *next_{nullptr};           // next block if split from a larger allocation
    int32_t event_count_{0};               // number of outstanding events
    bool allocated_{false};                // in-use flag
    int64_t gc_count_{0};                  // get_free_blocks_call_count when DeviceBlock is inserted

    bool is_safe_{true}; // whether this block have corresponded dataPtr
    std::shared_ptr<c10::GatheredContext> context_when_allocated_;

    DeviceBlock(int32_t device, aclrtStream stream, size_t size, DeviceBlockPool *pool, void *ptr,
                DeviceBlockType block_type)
        : deviceId_(device), stream_(stream), size_(size), requested_size_(0), pool_(pool), block_type_(block_type),
          ptr_(ptr)
    {}
    // constructor for search key
    DeviceBlock(int32_t device, aclrtStream stream, size_t size) : deviceId_(device), stream_(stream), size_(size) {}

    ~DeviceBlock() = default;

    bool isSplit() const;
    void splice(DeviceBlock *before, DeviceBlock *after);
};

/**
 * @brief Comparator function of device block in blockSets
 */
using Comparison = bool (*)(const DeviceBlock *, const DeviceBlock *);
inline bool DeviceBlockCompareBySize(const DeviceBlock *a, const DeviceBlock *b)
{
    if (a->stream_ != b->stream_) {
        return (uintptr_t)a->stream_ < (uintptr_t)b->stream_;
    }

    if (a->size_ != b->size_) {
        return a->size_ < b->size_;
    }

    return a->ptr_ < b->ptr_;
}

/**
 * @brief DeviceBlockPool
 */
class DeviceBlockPool {
public:
    std::set<DeviceBlock *, Comparison> small_blocks_;
    std::set<DeviceBlock *, Comparison> large_blocks_;

    // following params applied for privatePool(or graphPool) cases
    bool is_private_{false};
    // Number of live graphs using this pool
    int use_count_{1};
    // Number of unfreed npuMallocs made for this pool. When use_count and
    // npuMalloc_count drop to zero, we can delete this PrivatePool from
    // graph_pools.
    int npuMalloc_count_{0};

    DeviceBlockPool(bool is_private = false)
        : small_blocks_(DeviceBlockCompareBySize), large_blocks_(DeviceBlockCompareBySize), is_private_(is_private)
    {}

    void eraseBlock(DeviceBlockType block_type, DeviceBlock *block);
    void insertBlock(DeviceBlockType block_type, DeviceBlock *block);
};

/**
 * @brief DeviceAllocParams
 */
struct DeviceAllocParams {
    DeviceAllocParams(int32_t device, size_t size, aclrtStream stream, DeviceBlockPool *pool, size_t alloc_size,
                      DeviceBlockType block_type)
        : search_key_(device, stream, size), pool_(pool), alloc_size_(alloc_size), block_type_(block_type)
    {}

    inline int32_t device() const
    {
        return search_key_.deviceId_;
    };
    inline aclrtStream stream() const
    {
        return search_key_.stream_;
    };
    inline size_t size() const
    {
        return search_key_.size_;
    };

    DeviceBlock search_key_;
    // since all value pass of DeviceBlockPool using ptr, we direct use pool_ instead of p_pool_
    DeviceBlockPool *pool_;
    size_t alloc_size_;
    DeviceBlock *block_{nullptr};
    DeviceBlockType block_type_;
    ZResult result_{Z_OK};
};

class DeviceSMACachingAllocator;

/**
 * @brief EventController
 * friend of DeviceSMACachingAllocator to call free_block in allocator
 */
class EventController {
public:
    // Explicit device count
    EventController() : pools_(c10_npu::device_count()) {}

    // get a zevent on target device(cached or new)
    ZEvent get(int device);

    // sync events and free block if its events cnt down to 0
    void synchronizeAndFreeEvents(DeviceSMACachingAllocator *allocator, bool check_error,
                                  const std::shared_ptr<c10::GatheredContext> &context);

    // insert events according to blocks stream_uses_
    void insertEvents(DeviceSMACachingAllocator *allocator, DeviceBlock *block);

    // query events and free block if its events cnt down to 0, break after query one success block(if have)
    void processEvents(DeviceSMACachingAllocator *allocator, const std::shared_ptr<c10::GatheredContext> &context);

    // [US]force to clean all Events
    void cleanEvents(DeviceSMACachingAllocator *allocator);

    // [US]force to free block and its event on target stream
    void cleanStream(DeviceSMACachingAllocator *allocator, DeviceBlock *block, c10_npu::NPUStream stream);

    void emptyCache();

private:
    // {stream: [(event -- block*), (event -- block*)]}
    ska::flat_hash_map<c10_npu::NPUStream, std::deque<std::pair<ZEvent, DeviceBlock *>>> npu_events_;

    // this struct is too simple to drop in device_pool file
    struct PerDevicePool {
        alignas(ALIGN_64) std::mutex mutex_;
        std::vector<std::unique_ptr<c10_npu::NPUEvent>> event_pool_;
    };
    std::vector<PerDevicePool> pools_;
};

/**
 * @brief MempoolIdHash
 */
struct MempoolIdHash {
    std::size_t operator()(const c10_npu::MempoolId_t &mempool_id) const noexcept
    {
        return mempool_id.first != 0 ? mempool_id.first : mempool_id.second;
    }
};

/**
 * @brief GraphDeferPools
 * mainly defer the event and stream process during capture then replay after it
 * also use new blockPool to cache tensor for each capture stream
 * friend of DeviceSMACachingAllocator to call free_block in allocator
 */
class GraphDeferPools {
public:
    // following needed by top level graph API in allocator, we used public for convenience

    // captures_underway tracks if we are diverting some
    // allocations to a specific pool.
    // Most of the time it's empty, in which case malloc can avoid calling
    // aclrtStreamGetCaptureInfo in the hot path.
    std::vector<std::pair<c10_npu::MempoolId_t, std::function<bool(aclrtStream)>>> captures_underway_;

    // Private pools for NPU graphs
    ska::flat_hash_map<c10_npu::MempoolId_t, std::unique_ptr<DeviceBlockPool>, MempoolIdHash> graph_pools_;

    // Pools no longer referenced by any graph. Their BlockPools are eligible for
    // free_blocks. Can't be a vector or deque because we might erase entries in
    // any order. Could be an std::list, but we don't care much, access and
    // insert/erase are rare.
    ska::flat_hash_map<c10_npu::MempoolId_t, DeviceBlockPool *, MempoolIdHash> graph_pools_freeable_;

    // public functions
    GraphDeferPools() {}

    // remove stream uses added during npu-graph capture(considered down with capture end)
    void removeNpuGraphStreamUses(DeviceBlock *block);

    // defer insert events(created by record stream during capture) until no capture
    void insertEventsDeferredUntilNoCapture(DeviceSMACachingAllocator *allocator,
                                            const std::shared_ptr<c10::GatheredContext> &context);

    void appendEventsDeferredUntilNoCapture(DeviceBlock *block);

    void insertBlockToNpuGraphStreamUses(DeviceBlock *block, c10_npu::NPUStream stream);

private:
    // cache events when graph capture and deferred to insert after no capture
    std::vector<DeviceBlock *> needs_events_deferred_until_no_capture_;

    // mapping from block to a stream_set, containing streams on which the block
    // was used while npugraph capturing
    std::unordered_map<DeviceBlock *, StreamSet> block_to_npugraph_stream_uses_;
};

} // namespace device
} // namespace sma
} // namespace zbal

#endif // ZBAL_SMA_DEVICE_POOL_H
