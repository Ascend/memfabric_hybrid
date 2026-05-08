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
#ifndef ZBAL_SMA_DEVICE_ALLOC_H
#define ZBAL_SMA_DEVICE_ALLOC_H

#include "zbal_sma_common.h"
#include "zbal_sma_config.h"
#include "zbal_sma_device_pool.h"
#include "zbal_sma_mm_heap.h"
#include "zbal_sma_device_info.h"

using ZEvent = std::unique_ptr<c10_npu::NPUEvent, std::function<void(c10_npu::NPUEvent *)>>;
using CustomHeapPool = zbal::sma::heap::SplitMemoryHeap;
using TraceObserver = std::function<void(zbal::sma::device::TraceAction, int64_t, size_t, aclrtStream, int)>;
using SegmentObserver = std::function<void(const std::vector<const zbal::sma::device::DeviceBlock*>&, int)>;

using Stat = c10_npu::NPUCachingAllocator::Stat;
using StatType = c10_npu::NPUCachingAllocator::StatType;
using StatTypes = std::array<bool, static_cast<size_t>(StatType::NUM_TYPES)>;
using StatArray = c10_npu::NPUCachingAllocator::StatArray;
using DeviceStats = c10_npu::NPUCachingAllocator::DeviceStats;


namespace zbal {
namespace sma {
// To prevent the deadlock situation, temporarily release the lock.
//
// Deadlock Scenario Description:
//
// 1. Main Thread:
//    - Acquires the lock and performs sync to clear the taskqueue.
//    - taskqueue wait a empty signal from the sub-thread.
//
// 2. Sub-thread:
//    - Python function (tbe op compile) called in CANN may trigger GC that introduces a resource release operation.
//    - The release operation (`free`) cannot acquire the same lock holded in main thread.
//    - Unable to send a signal to the main thread.
class UnlockGuard {
public:
    explicit UnlockGuard(std::unique_lock<std::recursive_mutex>& lock) : lock_(lock) { lock_.unlock(); }

    ~UnlockGuard() { lock_.lock(); }

private:
    std::unique_lock<std::recursive_mutex>& lock_;
};

}  // namespace sma
}  // namespace zbal


namespace zbal {
namespace sma {
namespace device {

class EventController;

class DeviceSMACachingAllocator {
public:
    DeviceSMACachingAllocator() : default_pool_(false), graph_defers_()
    {}

    DeviceBlock *malloc(int device, size_t orig_size, aclrtStream stream, uint8_t allocator_type = 0);

    void free(DeviceBlock *block, uint8_t allocator_type = 0);

    void recordStream(DeviceBlock *block, c10_npu::NPUStream stream);

    void eraseStream(DeviceBlock *block, c10_npu::NPUStream stream);

    // change all activated block into unsafe
    void markAllBlockUnsafe();

    // get head block, along with cum-summed head to tail total segment blocks size
    void *getBaseAllocation(DeviceBlock *block, size_t *outSize);

    // set memory fraction to limit maximum allocated memory
    void setMemoryFraction(double fraction);

    // returns cached blocks to the system allocator
    void emptyCache(int device, bool check_error);

    // Retrieves info (total size + the largest block) of the memory cache
    void cacheInfo(size_t *total, size_t *largest);

    // free all event if count down to 0
    void releaseAndFreeEvents();

    // Called by NPUGraph::capture_begin
    void beginAllocateToPool(c10_npu::MempoolId_t mempool_id, std::function<bool(aclrtStream)> filter);

    // Called by NPUGraph::capture_end
    void endAllocateToPool(c10_npu::MempoolId_t mempool_id);

    // Called by NPUGraph::reset
    void releasePool(c10_npu::MempoolId_t mempool_id);

    // export device stats
    DeviceStats getStats();

    // Resets the historical accumulation stats for the device
    void resetAccumulatedStats();

    // Resets the historical peak stats for the device
    void resetPeakStats();

    // heap funcs
    inline void setMemHeapPool(void *base, uint64_t size)
    {
        mem_heap_pool_ = std::make_shared<CustomHeapPool>(base, size);
    };
    inline bool isHeapInited()
    {
        if (mem_heap_pool_ != nullptr)
            return mem_heap_pool_->isInitialized();
        else
            return false;
    };
    inline void *getHeapBase() { return mem_heap_pool_->getBaseAddr();};
    inline uint64_t getHeapTotalSize() { return mem_heap_pool_->getTotalSize();};
    inline uint64_t getHeapInUsedSize() { return mem_heap_pool_->getInUsedSize();};

    // attach observer functions
    void attachSnapShotObserver(TraceObserver trace_ob_func, SegmentObserver segment_ob_func);

    // manually take snapshot on current segmentInfos, remind this action use mutex
    void snapshot(int device);

private:
    // lock around all operations
    mutable std::recursive_mutex mutex_;

    // global unallocated cached blocks(except private_pool)
    DeviceBlockPool default_pool_;

    // allocated or in use by a stream
    ska::flat_hash_set<DeviceBlock *> active_blocks_;

    // mem heap
    std::shared_ptr<CustomHeapPool> mem_heap_pool_{nullptr};

    // addrs allocated by bootstrap
    ska::flat_hash_set<void *> symm_addrs_;

    // outstanding acl events
    friend class ::zbal::sma::device::EventController;
    // use get_event_internal to lazy init static EventController to avoid shutdown issues

    // record used memory.
    size_t total_allocated_memory_ = 0;

    // record maximum allowed memory.
    size_t allowed_memory_maximum_ = 0;

    bool set_fraction_ = false;

    // for cache&defer actions during and after graph capture
    friend class ::zbal::sma::device::GraphDeferPools;
    GraphDeferPools graph_defers_;

    // alloc/free/emptyCache trace observer func & segmentsInfo observer
    TraceObserver trace_observer_;
    SegmentObserver segments_observer_;

    // directly use c10_npu allocator device stats define
    DeviceStats stats_;

    // All following private methods do not acquire the allocator mutex
    // move a founded block from pool into active_list, may get new block which split from found one
    DeviceBlock *alloc_found_block(DeviceAllocParams params, size_t orig_size,
                                   std::shared_ptr<c10::GatheredContext> context,
                                   bool split_remainder, uint8_t allocator_type);

    // get all blocks(in default_pool, graph_pools, and active blocks)
    std::vector<const DeviceBlock *> get_all_blocks() const;

    // free a block from active blocks into the pool of cached free blocks
    void free_block(DeviceBlock *block,
                    const std::shared_ptr<c10::GatheredContext> &context,
                    uint8_t allocator_type = 0);

    // combine previously split blocks. returns the size of the subsumed block, or 0 on failure.
    size_t try_merge_blocks(DeviceBlock *dst, DeviceBlock *src, DeviceBlockPool &pool);

    // get running pool & its correspond block_type
    DeviceBlockPool &get_pool(size_t size, aclrtStream stream, DeviceBlockType &block_type);

    // whether split new block by actual size while doing alloc_found
    bool should_split(const DeviceBlock *block, size_t size);

    // search the closest free block in pool from DeviceAllocParams
    bool get_free_block(DeviceAllocParams &p);

    // free unused block until gc_threshold according to gc_count_
    void garbage_collect_cached_blocks(const std::shared_ptr<c10::GatheredContext>& ctx,
                                       std::unique_lock<std::recursive_mutex>& lock);

    // alloc a new block(or map a new one if using expand), need to mark private or not
    bool alloc_block(DeviceAllocParams &p, bool isRetry, const std::shared_ptr<c10::GatheredContext> &ctx,
                     std::unique_lock<std::recursive_mutex> &lock);

    // Free one or more blocks to the system allocator. But only enough to satisfy the target size
    // Start from over max_split_size to small size
    bool release_available_cached_blocks(const DeviceAllocParams& p, const std::shared_ptr<c10::GatheredContext>& ctx,
                                         std::unique_lock<std::recursive_mutex>& lock);

    // npuSynchronizeDevice must be executed before this function can be called
    bool release_cached_blocks(bool check_error, const std::shared_ptr<c10::GatheredContext> &context);

    // release a block from cached pool
    void release_block(DeviceBlock *block, const std::shared_ptr<c10::GatheredContext> &context);

    // remove a block (skip fall back into cache pool)
    void remove_block(DeviceBlock *block, const std::shared_ptr<c10::GatheredContext> &context);

    // release all block in pool, also free private pool if free_private
    void release_pool(DeviceBlockPool &pool, const std::shared_ptr<c10::GatheredContext> &context, bool free_private);

    // get static EventController(to avoid auto release issue)
    EventController* get_event_internal();

    // synchronize on all outstanding events and then free associated-blocks.
    void synchronize_and_free_events(bool check_error, const std::shared_ptr<c10::GatheredContext> &context);

    // insert events for all streams using this activated block
    void insert_events(DeviceBlock *block);

    // check each stream's events, process only one case if queried ended,
    // decrease block event_count_ and free when it down to 0
    void process_events(const std::shared_ptr<c10::GatheredContext> &context);

    // Accumulates sizes of all memory blocks for given device in given pool
    void cache_info_aux(DeviceBlockPool &block_pool, size_t *total, size_t *largest);

    static size_t round_size(size_t size);

    static size_t get_allocation_size(size_t size);

    void update_stat(Stat &stat, int64_t amount);

    void reset_accumulated_stat(Stat &stat);

    void reset_peak_stat(Stat &stat);

    template<typename Func>
    void for_each_selected_stat_type(const StatTypes &stat_types, Func f);

    void update_stat_array(StatArray &stat_array, int64_t amount, const StatTypes &stat_types);
};

}  // namespace device
}  // namespace sma
}  // namespace zbal


#endif  // ZBAL_SMA_DEVICE_ALLOC_H
