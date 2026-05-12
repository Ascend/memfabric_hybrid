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
#include "zbal_sma_device.h"

namespace zbal {
namespace sma {

std::string format_size(uint64_t size)
{
    std::ostringstream os;
    os.precision(2);
    os << std::fixed;
    if (size <= kKB) {
        os << size << " bytes";
    } else if (size <= kMB) {
        os << (size / float(kKB));
        os << " KiB";
    } else if (size <= kGB) {
        os << (size / float(kMB));
        os << " MiB";
    } else {
        os << (size / float(kGB));
        os << " GiB";
    }
    return os.str();
}

} // namespace sma
} // namespace zbal

namespace zbal {
namespace sma {
namespace device {

using Blocks = std::set<DeviceBlock *, Comparison>;
using StreamSet = ska::flat_hash_set<c10_npu::NPUStream>;

// private funcs
DeviceBlock *DeviceSMACachingAllocator::alloc_found_block(DeviceAllocParams params, size_t orig_size,
                                                          std::shared_ptr<c10::GatheredContext> context,
                                                          bool split_remainder, uint8_t allocator_type)
{
    (void)allocator_type;
    auto size = params.size();
    auto device = params.device();
    auto pool = params.pool_;
    auto stream = params.stream();

    ZBAL_ASSERT_S(params.result_ == Z_OK && params.block_ != nullptr && params.block_->ptr_ != nullptr, Z_INVALID_PTR);
    DeviceBlock *block = params.block_;
    DeviceBlock *remaining = nullptr;

    if (split_remainder) {
        remaining = block;

        block = new DeviceBlock(device, stream, size, pool, block->ptr_, block->block_type_);
        block->prev_ = remaining->prev_;
        if (block->prev_) {
            block->prev_->next_ = block;
        }
        block->next_ = remaining;

        // consider that size & begin & end is already aligned, no need to do re-aligned for new block
        remaining->prev_ = block;
        remaining->ptr_ = static_cast<char *>(remaining->ptr_) + size;
        remaining->size_ -= size;

        pool->insertBlock(remaining->block_type_, remaining);
    }

    block->allocated_ = true;
    block->requested_size_ = orig_size;
    if (!block->is_safe_) {
        ZBAL_LOG_WARN("Unsafe memory block is passively refreshed by releasing and allocating memory again");
    }
    block->is_safe_ = true;
    trace_observer_(TraceAction::ALLOC, int64_t(block->ptr_), orig_size, block->stream_, block->deviceId_);

    block->context_when_allocated_ = std::move(context);

    active_blocks_.insert(block);

    ZBAL_LOG_DEBUG("SMA CachingAllocator malloc: malloc = " << block->size_);

    return block;
}

std::vector<const DeviceBlock *> DeviceSMACachingAllocator::get_all_blocks() const
{
    std::vector<const DeviceBlock *> blocks;
    blocks.insert(blocks.end(), default_pool_.small_blocks_.begin(), default_pool_.small_blocks_.end());
    blocks.insert(blocks.end(), default_pool_.large_blocks_.begin(), default_pool_.large_blocks_.end());
    for (const auto &gp : graph_defers_.graph_pools_) {
        blocks.insert(blocks.end(), gp.second->small_blocks_.begin(), gp.second->small_blocks_.end());
        blocks.insert(blocks.end(), gp.second->large_blocks_.begin(), gp.second->large_blocks_.end());
    }
    blocks.insert(blocks.end(), active_blocks_.begin(), active_blocks_.end());
    return blocks;
}

void DeviceSMACachingAllocator::free_block(DeviceBlock *block, const std::shared_ptr<c10::GatheredContext> &context,
                                           uint8_t allocator_type)
{
    ZBAL_ASSERT_S(!block->allocated_ && block->event_count_ == 0, Z_INVALID_VALUE);
    (void)context;
    (void)allocator_type;
    trace_observer_(TraceAction::FREE_COMPLETED, int64_t(block->ptr_), block->requested_size_, block->stream_,
                    block->deviceId_);

    block->context_when_allocated_ = nullptr;

    auto &pool = *block->pool_;

    const std::array<DeviceBlock *, 2> merge_candidates = {block->prev_, block->next_};
    for (DeviceBlock *merge_candidate : merge_candidates) {
        const int64_t subsumed_size = static_cast<int64_t>(try_merge_blocks(block, merge_candidate, pool));
        (void)subsumed_size;
    }

    if (block->block_type_ == BT_SMALL) {
        active_blocks_.erase(block);
        pool.insertBlock(block->block_type_, block);
    } else {
        active_blocks_.erase(block);
        if (block->stream_uses_.empty() && !block->isSplit() && (block->size_ > SMAConfig::segment_size_mb()) &&
            !block->pool_->is_private_) {
            // directly remove block instead leave to cache pool in processing large block
            remove_block(block, nullptr);
        } else {
            pool.insertBlock(block->block_type_, block);
        }
    }
}

size_t DeviceSMACachingAllocator::try_merge_blocks(DeviceBlock *dst, DeviceBlock *src, DeviceBlockPool &pool)
{
    if (!src || src->allocated_ || src->event_count_ > 0 || !src->stream_uses_.empty()) {
        return 0;
    }

    ZBAL_ASSERT_S(dst->isSplit() && src->isSplit(), "assert is_split error:", Z_INVALID_VALUE);

    if (dst->prev_ == src) {
        dst->ptr_ = src->ptr_;
        dst->prev_ = src->prev_;
        if (dst->prev_) {
            dst->prev_->next_ = dst;
        }
    } else {
        dst->next_ = src->next_;
        if (dst->next_) {
            dst->next_->prev_ = dst;
        }
    }

    const size_t subsumed_size = src->size_;
    dst->size_ += subsumed_size;

    pool.eraseBlock(src->block_type_, src);

    delete src;
    src = nullptr;

    return subsumed_size;
}

DeviceBlockPool &DeviceSMACachingAllocator::get_pool(size_t size, aclrtStream stream, DeviceBlockType &block_type)
{
    // captures_underway is a conservative guess that the current stream may be
    // capturing. It's only non-empty if some thread has begun and not yet ended
    // a capture, so it's usually 0, and we can short-circuit
    // npuStreamCaptureStatus (which does a TLS lookup).
    if (ZBAL_UNLIKELY(!graph_defers_.captures_underway_.empty())) {
        for (auto &entry : graph_defers_.captures_underway_) {
            if (entry.second(stream)) {
                auto it1 = graph_defers_.graph_pools_.find(entry.first);
                ZBAL_ASSERT(it1 != graph_defers_.graph_pools_.end());
                if (size <= kSmallSize) {
                    block_type = BT_SMALL;
                } else {
                    block_type = BT_BIG;
                }
                return *(it1->second);
            }
        }
    }

    if (size <= kSmallSize) {
        block_type = BT_SMALL;
    } else {
        block_type = BT_BIG;
    }
    return default_pool_;
}

bool DeviceSMACachingAllocator::should_split(const DeviceBlock *block, size_t size)
{
    size_t remaining = block->size_ - size;
    if (block->block_type_ == BT_SMALL) {
        return remaining >= kMinBlockSize;
    } else {
        return (size < SMAConfig::max_split_size()) && (remaining > kSmallSize);
    }
}

bool DeviceSMACachingAllocator::get_free_block(DeviceAllocParams &p)
{
    DeviceBlockPool &pool = *p.pool_;

    Blocks block_slot = (p.block_type_ == BT_SMALL) ? pool.small_blocks_ : pool.large_blocks_;
    if (ZBAL_UNLIKELY(set_fraction_ && SMAConfig::garbage_collection_threshold() > 0.0)) {
        // Track block reuse interval only when garbage collection is enabled.
        for (auto &b : block_slot) {
            ++b->gc_count_;
        }
    }
    auto it = block_slot.lower_bound(&p.search_key_);
    // stream diff indicate that block in this stream have already no appropriate block
    if (it == block_slot.end() || (*it)->stream_ != p.stream()) {
        return false;
    }

    // Do not return an over-sized block for a large request
    if ((p.size() < SMAConfig::max_split_size()) && ((*it)->size_ >= SMAConfig::max_split_size())) {
        return false;
    }
    // Allow over-sized block size to be rounded up but within a limit
    if ((p.size() >= SMAConfig::max_split_size()) && ((*it)->size_ >= p.size() + kLargeBuffer)) {
        return false;
    }
    p.block_ = *it;
    p.block_type_ = (*it)->block_type_;
    (*it)->gc_count_ = 0; // Denote this block has been used
    pool.eraseBlock(p.block_type_, *it);
    return true;
}

void DeviceSMACachingAllocator::garbage_collect_cached_blocks(const std::shared_ptr<c10::GatheredContext> &ctx,
                                                              std::unique_lock<std::recursive_mutex> &lock)
{
    // Free unused cached blocks to reclaim NPU memory.
    // Unlike release_cached_blocks(), this does not enforce synchronization and
    // therefore should be of less overheads.

    size_t gc_threshold = static_cast<size_t>(SMAConfig::garbage_collection_threshold() * allowed_memory_maximum_);
    // No need to trigger GC yet
    if (total_allocated_memory_ <= gc_threshold) {
        return;
    }
    const auto target_size = total_allocated_memory_ - gc_threshold;
    size_t gc_reclaimed = 0;

    // Calculate the total age of the free-able blocks. We'll use it later to get "avg age" threshold.
    double total_age = 0.0;
    int freeable_block_count = 0;
    for (auto &b : default_pool_.large_blocks_) {
        if (!b->isSplit()) {
            total_age += b->gc_count_;
            ++freeable_block_count;
        }
    }
    // No free-able blocks?
    if (freeable_block_count == 0) {
        return;
    }

    {
        UnlockGuard guard(lock);
        c10_npu::npuSynchronizeDevice(true);
    }

    // Repeat GC until we reach reclaim > target size.
    bool block_freed = true;
    while (gc_reclaimed < target_size && block_freed && freeable_block_count > 0) {
        // Free blocks exceeding this age threshold first.
        double age_threshold = total_age / freeable_block_count;
        // Stop iteration if we can no longer free a block.
        block_freed = false;

        // Free blocks of > avg age. Don't stop upon reaching the target_size,
        // we don't want this GC to be triggered frequently.
        auto it = default_pool_.large_blocks_.begin();
        while (it != default_pool_.large_blocks_.end()) {
            DeviceBlock *block = *it;
            ++it;
            if (!block->isSplit() && block->gc_count_ >= age_threshold) {
                block_freed = true;
                gc_reclaimed += block->size_;
                total_age -= block->gc_count_; // Decrement the age
                freeable_block_count--;        // One less block that can be freed
                release_block(block, ctx);

                ZBAL_LOG_DEBUG("SMACachingAllocator gc: free = " << block->size_ <<
                               " allocated = " << total_allocated_memory_);
            }
        }
    }
}

bool DeviceSMACachingAllocator::alloc_block(DeviceAllocParams &p, bool isRetry,
                                            const std::shared_ptr<c10::GatheredContext> &ctx,
                                            std::unique_lock<std::recursive_mutex> &lock)
{
    (void)isRetry;
    (void)lock;
    size_t size = p.alloc_size_;
    void *ptr = nullptr;

    if (set_fraction_ && total_allocated_memory_ + size > allowed_memory_maximum_) {
        p.result_ = Z_ERROR_ALLOC;
        return false;
    } else {
        // add active_pool is_private check before this?
        if (mem_heap_pool_ && mem_heap_pool_->isInitialized()) {
            p.result_ = zbal::sma::CustomHeapAlignedAllocate(&ptr, size, mem_heap_pool_);
            if (p.result_ == Z_OK) {
                symm_addrs_.insert(ptr);
            }
        } else {
            ZBAL_LOG_ERROR("sma heap not inited, using aclRT instead(this may be a undefined behavior)");
        }

        if (p.result_ != Z_OK) {
            return false;
        }
    }

    if (p.pool_->is_private_) {
        // The block is for a NPU graph's PrivatePool.
        p.pool_->npuMalloc_count_++;
    }

    total_allocated_memory_ += size;
    p.block_ = new DeviceBlock(p.device(), p.stream(), size, p.pool_, (char *)ptr, p.block_type_);
    ZBAL_LOG_DEBUG("DeviceSMACachingAllocator: malloc = " << size << " ret = " << p.result_);

    // p.block_ came from new, not npuMalloc. It should not be nullptr here.
    ZBAL_ASSERT_S(p.block_ != nullptr && p.block_->ptr_ != nullptr, "block invalid!");

    trace_observer_(TraceAction::SEGMENT_ALLOC, int64_t(p.block_->ptr_), p.block_->size_, p.stream(), p.device());
    p.block_->context_when_allocated_ = ctx;
    return true;
}

bool DeviceSMACachingAllocator::release_available_cached_blocks(const DeviceAllocParams &p,
                                                                const std::shared_ptr<c10::GatheredContext> &ctx,
                                                                std::unique_lock<std::recursive_mutex> &lock)
{
    // meaning no split over max_split_size, just skip
    if (SMAConfig::max_split_size() == std::numeric_limits<size_t>::max()) {
        return false;
    }
    DeviceBlockPool &pool = *p.pool_;
    DeviceBlock key = p.search_key_;
    key.size_ = (key.size_ < SMAConfig::max_split_size()) ? SMAConfig::max_split_size() : key.size_;
    // maybe this max_split_size is only appropriate for small pool?
    auto &block_slot = (p.block_type_ == BT_SMALL) ? pool.small_blocks_ : pool.large_blocks_;
    auto it = block_slot.lower_bound(&key);

    {
        UnlockGuard guard(lock);
        c10_npu::npuSynchronizeDevice(true);
    }

    if (it == block_slot.end() || (*it)->stream_ != p.stream()) {
        // No single block is large enough; free multiple oversize blocks, starting with the largest
        if (it == block_slot.begin()) {
            return false;
        }
        size_t totalReleased = 0;
        // Back up one item.  Now on the largest block for the correct stream
        --it;
        while ((totalReleased < key.size_) && ((*it)->size_ >= SMAConfig::max_split_size()) &&
               ((*it)->stream_ == p.stream())) {
            auto cur = it;
            totalReleased += (*it)->size_;
            if (it != block_slot.begin()) {
                --it;
                release_block(*cur, ctx);
                if (totalReleased >= key.size_) {
                    break;
                }
            } else {
                release_block(*cur, ctx);
                break;
            }
        }
        if (totalReleased < key.size_) {
            return false;
        }
    } else {
        // free a single block large enough and return
        release_block(*it, ctx);
    }
    return true;
}

void DeviceSMACachingAllocator::release_block(DeviceBlock *block, const std::shared_ptr<c10::GatheredContext> &context)
{
    trace_observer_(TraceAction::SEGMENT_FREE, int64_t(block->ptr_), block->size_, block->stream_, block->deviceId_);
    (void)context;
    if (symm_addrs_.count((void *)block->ptr_)) {
        symm_addrs_.erase((void *)block->ptr_);
        ZBAL_CHECK_S(zbal::sma::CustomHeapRelease((void *)block->ptr_, mem_heap_pool_) == ACL_SUCCESS,
                     "symm heap free failed");
    } else {
        ZBAL_LOG_ERROR("sma miss this ptr, using aclRT instead(this may be a undefined behavior)");
    }
    total_allocated_memory_ -= block->size_;

    auto *pool = block->pool_;

    if (pool->is_private_) {
        // The npuFreed block belonged to a NPU graph's PrivatePool.
        ZBAL_ASSERT(pool->npuMalloc_count_ > 0);
        pool->npuMalloc_count_--;
    }
    ZBAL_LOG_DEBUG("DeviceSMACachingAllocator free by: size= " << block->size_);

    pool->eraseBlock(block->block_type_, block);
    delete block;
    block = nullptr;
}

void DeviceSMACachingAllocator::remove_block(DeviceBlock *block, const std::shared_ptr<c10::GatheredContext> &context)
{
    ZBAL_ASSERT_S(!block->pool_->is_private_, "remove block can not be used in graph pool, need sync before");

    trace_observer_(TraceAction::SEGMENT_FREE, int64_t(block->ptr_), block->size_, block->stream_, block->deviceId_);
    (void)context;
    if (symm_addrs_.count((void *)block->ptr_)) {
        symm_addrs_.erase((void *)block->ptr_);
        ZBAL_CHECK_S(zbal::sma::CustomHeapRelease((void *)block->ptr_, mem_heap_pool_) == ACL_SUCCESS,
                     "symm heap free failed");
    } else {
        ZBAL_LOG_ERROR("sma miss this ptr, using aclRT instead(this may be a undefined behavior)");
    }
    total_allocated_memory_ -= block->size_;

    auto *pool = block->pool_;
    pool->eraseBlock(block->block_type_, block);
    delete block;
    block = nullptr;
}

void DeviceSMACachingAllocator::release_pool(DeviceBlockPool &pool,
                                             const std::shared_ptr<c10::GatheredContext> &context, bool free_private)
{
    // Frees all non-split blocks
    // skip private pool if not free_private
    if (!free_private && pool.is_private_) {
        return;
    }

    auto it = pool.large_blocks_.begin();
    while (it != pool.large_blocks_.end()) {
        DeviceBlock *block = *it;
        ++it;
        if (!block->prev_ && !block->next_) {
            release_block(block, context);
        }
    }

    it = pool.small_blocks_.begin();
    while (it != pool.small_blocks_.end()) {
        DeviceBlock *block = *it;
        ++it;
        if (!block->prev_ && !block->next_) {
            release_block(block, context);
        }
    }
}

EventController *DeviceSMACachingAllocator::get_event_internal()
{
    // Leak the event pool to avoid shutdown issues.
    static auto *event_pool_ = new EventController();
    return event_pool_;
}

bool DeviceSMACachingAllocator::release_cached_blocks(bool check_error,
                                                      const std::shared_ptr<c10::GatheredContext> &context)
{
    // First ensure that all blocks that can't currently be allocated due to
    // outstanding events are returned to the pool.
    synchronize_and_free_events(check_error, context);

    // Free all non-split cached blocks, including graph pools which use_count is down to 0
    release_pool(default_pool_, context, false);
    // Free all free-able graph pools
    for (auto it = graph_defers_.graph_pools_freeable_.begin(); it != graph_defers_.graph_pools_freeable_.end();) {
        // See notifyCaptureDestroy for the strategy here.
        ZBAL_ASSERT(it->second->use_count_ == 0);
        release_pool(*(it->second), context, true);
        if (it->second->npuMalloc_count_ == 0) {
            auto erase_count = graph_defers_.graph_pools_.erase(it->first);
            ZBAL_ASSERT(erase_count == 1);
            it = graph_defers_.graph_pools_freeable_.erase(it);
        } else {
            ++it;
        }
    }

    return true;
}

void DeviceSMACachingAllocator::synchronize_and_free_events(bool check_error,
                                                            const std::shared_ptr<c10::GatheredContext> &context)
{
    // This function syncs, so capture should not be underway. Might as well
    // make sure capture-deferred end of life events get processed too.
    ZBAL_ASSERT(graph_defers_.captures_underway_.empty());
    graph_defers_.insertEventsDeferredUntilNoCapture(this, context);

    auto event_pool = get_event_internal();
    event_pool->synchronizeAndFreeEvents(this, check_error, context);
}

void DeviceSMACachingAllocator::insert_events(DeviceBlock *block)
{
    // insert events ctx guard
    int pre_device = -1;
    c10_npu::GetDevice(&pre_device);
    aclrtContext compiler_ctx = aclrtContext();
    aclError ret_ctx = aclrtGetCurrentContext(&compiler_ctx);

    auto event_pool = get_event_internal();
    event_pool->insertEvents(this, block);

    if (ret_ctx == ACL_SUCCESS) {
        ZBAL_CHECK_S(aclrtSetCurrentContext(compiler_ctx) == ACL_SUCCESS, "c10_npu func failed");
        // Setting context will exchange device implicitly,
        // so we need to reset the cached device here to ensure consistency.
        ZBAL_CHECK_S(c10_npu::SetDevice(pre_device) == ACL_SUCCESS, "c10_npu func failed");
    }
}

void DeviceSMACachingAllocator::process_events(const std::shared_ptr<c10::GatheredContext> &context)
{
    graph_defers_.insertEventsDeferredUntilNoCapture(this, context);

    auto event_pool = get_event_internal();
    event_pool->processEvents(this, context);
}

void DeviceSMACachingAllocator::cache_info_aux(DeviceBlockPool &block_pool, size_t *total, size_t *largest)
{
    for (auto it = block_pool.small_blocks_.begin(); it != block_pool.small_blocks_.end(); ++it) {
        size_t blocksize = (*it)->size_;
        *total += blocksize;
        if (blocksize > *largest) {
            *largest = blocksize;
        }
    }
    for (auto it = block_pool.large_blocks_.begin(); it != block_pool.large_blocks_.end(); ++it) {
        size_t blocksize = (*it)->size_;
        *total += blocksize;
        if (blocksize > *largest) {
            *largest = blocksize;
        }
    }
}

size_t DeviceSMACachingAllocator::round_size(size_t size)
{
    size = size + ALIGN_32;
    if (size < kMinBlockSize) {
        return kMinBlockSize;
    } else {
        return kMinBlockSize * ((size + kMinBlockSize - 1) / kMinBlockSize);
    }
}

size_t DeviceSMACachingAllocator::get_allocation_size(size_t size)
{
    if (size <= kSmallSize) {
        return kSmallBuffer;
    } else if (size <= kLargeBuffer) {
        return kLargeBuffer;
    } else {
        return kRoundLarge * ((size + kRoundLarge - 1) / kRoundLarge);
    }
}

void DeviceSMACachingAllocator::reset_accumulated_stat(Stat &stat)
{
    stat.allocated = 0;
    stat.freed = 0;
}

void DeviceSMACachingAllocator::reset_peak_stat(Stat &stat)
{
    stat.peak = stat.current;
}

template<typename Func>
void DeviceSMACachingAllocator::for_each_selected_stat_type(const StatTypes &stat_types, Func f)
{
    for (const auto stat_type : c10::irange(stat_types.size())) {
        if (stat_types[stat_type]) {
            f(stat_type);
        }
    }
}

void DeviceSMACachingAllocator::update_stat(Stat &stat, int64_t amount)
{
    stat.current += amount;
    stat.peak = std::max(stat.current, stat.peak);
    if (amount > 0) {
        stat.allocated += amount;
    }
    if (amount < 0) {
        stat.freed += -amount;
    }
}

void DeviceSMACachingAllocator::update_stat_array(StatArray &stat_array, int64_t amount, const StatTypes &stat_types)
{
    for_each_selected_stat_type(
        stat_types, [&stat_array, amount, this](size_t stat_type) { update_stat(stat_array[stat_type], amount); });
}

// public funcs
void DeviceSMACachingAllocator::releaseAndFreeEvents()
{
    std::unique_lock<std::recursive_mutex> lock(mutex_);

    auto event_pool = get_event_internal();
    event_pool->cleanEvents(this);
}

void DeviceSMACachingAllocator::markAllBlockUnsafe()
{
    for (auto &active_block : active_blocks_) {
        active_block->is_safe_ = false;
    }
    return;
}

void *DeviceSMACachingAllocator::getBaseAllocation(DeviceBlock *block, size_t *outSize)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    while (block->prev_) {
        block = block->prev_;
    }
    void *basePtr = block->ptr_;
    if (outSize) {
        size_t size = 0;
        while (block) {
            size += block->size_;
            block = block->next_;
        }
        *outSize = size;
    }
    return basePtr;
}

DeviceBlock *DeviceSMACachingAllocator::malloc(int device, size_t orig_size, aclrtStream stream, uint8_t allocator_type)
{
    // done outside the lock because we don't know what locks the recorder needs to have...
    auto context = nullptr;

    std::unique_lock<std::recursive_mutex> lock(mutex_);

    if (device == -1) {
        ZBAL_CHECK_S(c10_npu::GetDevice(&device) == ACL_SUCCESS, "c10_npu func check failed!");
    }

    if (ZBAL_LIKELY(graph_defers_.captures_underway_.empty())) {
        // Processes end-of-life events for outstanding allocations used on
        // multiple streams (checks if their NPU-side uses are complete and
        // recycles their memory if so)
        //
        // Q. Why skip process_events if a capture might be underway?
        // A. process_events involves npuEventQueries, illegal during NPU graph
        //    capture.
        //    Dumb simple solution: defer reclaiming these allocations until after
        //    capture. Cross-stream memory use is uncommon, so the deferral's
        //    effect on memory use during capture should be small.
        process_events(context);
    }
    auto size = round_size(orig_size);
    DeviceBlockType block_type;
    auto &pool = get_pool(size, stream, block_type);
    const size_t alloc_size = get_allocation_size(size);

    DeviceAllocParams params(device, size, stream, &pool, alloc_size, block_type);

    // First, try to get a block from the existing pool.
    bool block_found = get_free_block(params);
    // Can't reuse an existing block; try to get a new one.
    if (!block_found) {
        // Do garbage collection if the flag is set.
        if (ZBAL_UNLIKELY(set_fraction_ && SMAConfig::garbage_collection_threshold() > 0.0)) {
            garbage_collect_cached_blocks(context, lock);
        }
        // Attempt allocate
        block_found =
            alloc_block(params, false, context, lock) ||
            // Free enough available cached blocks to satisfy alloc and retry alloc.
            (release_available_cached_blocks(params, context, lock) && alloc_block(params, false, context, lock));
    }

    if (!block_found && ZBAL_UNLIKELY(graph_defers_.captures_underway_.empty())) {
        ZBAL_LOG_WARN("Get a block from the existing pool failed. "
                      "Try to free cached blocks and reallocate. This warning log can be ignored.");
        // Free all non-split cached blocks and retry alloc.
        {
            UnlockGuard guard(lock);
            // Make sure taskqueue is empty, then execute release_cached_blocks
            c10_npu::npuSynchronizeDevice(true);
        }
        block_found = (release_cached_blocks(true, nullptr) && alloc_block(params, true, context, lock));
    }

    if (!block_found) {
        if (params.result_ == Z_ERROR_ALLOC) {
            trace_observer_(TraceAction::OOM, total_allocated_memory_, params.size(), params.stream(), params.device());
            snapshot(params.device());
            device::DeviceInfoObserver::getInstance().dumpSnapshotJson(params.device(), "oom_log.");
            size_t device_total;
            zbal::sma::CustomGetTotalSize(device_total, mem_heap_pool_);
            AT_ERROR("NPU out of memory. Tried to allocate ", format_size(alloc_size), " (NPU:", device, "); with ",
                     format_size(total_allocated_memory_), " total allocated and ", format_size(device_total),
                     " total reserved.");
        }
        ZBAL_CHECK_S(params.result_ == Z_OK, "check alloc result failed");
    }

    bool split_remainder = should_split(params.block_, params.size());
    return alloc_found_block(std::move(params), orig_size, std::move(context), split_remainder, allocator_type);
}

void DeviceSMACachingAllocator::free(DeviceBlock *block, uint8_t allocator_type)
{
    std::shared_ptr<c10::GatheredContext> context = nullptr;
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    block->allocated_ = false;

    // following logic might modify underlying Block, causing the size changed. We store ahead for reporting
    auto orig_block_size = block->size_;

    trace_observer_(TraceAction::FREE_REQUESTED, int64_t(block->ptr_), block->requested_size_, block->stream_,
                    block->deviceId_);

    if (!block->stream_uses_.empty() && c10_npu::NpuSysCtrl::GetInstance().GetInitFlag()) {
        if (ZBAL_UNLIKELY(!graph_defers_.captures_underway_.empty())) {
            // It's forbidden to npuEventQuery an event recorded during NPU graph
            // capture. We conservatively defer recording end-of-life events until
            // the next call to process_events() (which won't happen until no
            // captures are underway)
            graph_defers_.appendEventsDeferredUntilNoCapture(block);
        } else {
            insert_events(block);
        }
    } else {
        free_block(block, context, allocator_type);
    }

    ZBAL_LOG_INFO("SMA CachingAllocator free: free = " << orig_block_size <<
                  ", allocated = " << total_allocated_memory_);
}

void DeviceSMACachingAllocator::recordStream(DeviceBlock *block, c10_npu::NPUStream stream)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    block->stream_uses_.insert(stream);

    if (C10_UNLIKELY(!graph_defers_.captures_underway_.empty())) {
        graph_defers_.insertBlockToNpuGraphStreamUses(block, stream);
    }
}

// this func is a non-standard func since Pytorch do not have this API, and erase without query is a wrong action
void DeviceSMACachingAllocator::eraseStream(DeviceBlock *block, c10_npu::NPUStream stream)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    block->stream_uses_.erase(stream);

    auto event_pool = get_event_internal();
    event_pool->cleanStream(this, block, stream);
}

void DeviceSMACachingAllocator::setMemoryFraction(double fraction)
{
    size_t device_total;
    zbal::sma::CustomGetTotalSize(device_total, mem_heap_pool_);
    allowed_memory_maximum_ = static_cast<size_t>(fraction * device_total);
    set_fraction_ = true;
}

void DeviceSMACachingAllocator::emptyCache(int device, bool check_error)
{
    std::shared_ptr<c10::GatheredContext> context = nullptr;
    trace_observer_(TraceAction::EMPTY_CACHE, 0, 0, 0, device);
    // Make sure event deque from taskqueue, then synchronize Event
    c10_npu::npuSynchronizeDevice(check_error);
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    release_cached_blocks(check_error, context);
}

void DeviceSMACachingAllocator::cacheInfo(size_t *total, size_t *largest)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    cache_info_aux(default_pool_, total, largest);

    for (const auto &gp : graph_defers_.graph_pools_) {
        cache_info_aux(*(gp.second), total, largest);
    }
}

void DeviceSMACachingAllocator::beginAllocateToPool(c10_npu::MempoolId_t mempool_id,
                                                    std::function<bool(aclrtStream)> filter)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = graph_defers_.graph_pools_.find(mempool_id);
    if (it == graph_defers_.graph_pools_.end()) {
        // mempool_id does not reference an existing pool. Make a new private pool for this capture.
        graph_defers_.graph_pools_.emplace(mempool_id, std::make_unique<DeviceBlockPool>(true));
    } else {
        // mempool_id references an existing pool, which the current capture will
        // share. Check this pool is live (at least one other capture already
        // references it).
        ZBAL_ASSERT(it->second->use_count_ > 0);
        it->second->use_count_++;
    }
    for (auto it2 = graph_defers_.captures_underway_.begin(); it2 != graph_defers_.captures_underway_.end(); ++it2) {
        ZBAL_CHECK_S(it2->first != mempool_id, "beginAllocateToPool: already recording to mempool_id");
    }
    graph_defers_.captures_underway_.emplace_back(mempool_id, std::move(filter));
}

void DeviceSMACachingAllocator::endAllocateToPool(c10_npu::MempoolId_t mempool_id)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    for (auto it = graph_defers_.captures_underway_.begin(); it != graph_defers_.captures_underway_.end(); ++it) {
        if (it->first == mempool_id) {
            graph_defers_.captures_underway_.erase(it);
            return;
        }
    }
    ZBAL_CHECK_S(false, "endAllocatePool: not currently recording to mempool_id");
}

void DeviceSMACachingAllocator::releasePool(c10_npu::MempoolId_t mempool_id)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    // The instantiated npugraphExec_t has been destroyed. We can't blindly
    // delete and npuFree the mempool its capture used, because
    //  1. other graph(s) might share the same pool
    //  2. the user might still hold references to output tensors allocated
    //  during capture.
    // To handle 1 and 2, we track the number of graphs using this particular
    // mempool. When the count reaches 0, we tell free_cached_blocks it may now
    // npuFree blocks from this graph's pool when it discovers they're unused
    // (unsplit).
    auto it = graph_defers_.graph_pools_.find(mempool_id);
    ZBAL_ASSERT(it != graph_defers_.graph_pools_.end());
    auto uc = --(it->second->use_count_);
    ZBAL_ASSERT(uc >= 0);
    if (uc == 0) {
        // Allows free_cached_blocks to begin npuFreeing this pool's memory,
        // and makes sure this pool wasn't somehow made freeable already.
        // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
        bool inserted = graph_defers_.graph_pools_freeable_.insert({mempool_id, it->second.get()}).second;
        ZBAL_ASSERT(inserted);
    }
}

DeviceStats DeviceSMACachingAllocator::getStats()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return stats_;
}

void DeviceSMACachingAllocator::resetAccumulatedStats()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    for (size_t statType = 0; statType < static_cast<size_t>(StatType::NUM_TYPES); ++statType) {
        reset_accumulated_stat(stats_.allocation[statType]);
        reset_accumulated_stat(stats_.segment[statType]);
        reset_accumulated_stat(stats_.active[statType]);
        reset_accumulated_stat(stats_.inactive_split[statType]);
        reset_accumulated_stat(stats_.allocated_bytes[statType]);
        reset_accumulated_stat(stats_.reserved_bytes[statType]);
        reset_accumulated_stat(stats_.active_bytes[statType]);
        reset_accumulated_stat(stats_.inactive_split_bytes[statType]);
        reset_accumulated_stat(stats_.requested_bytes[statType]);
    }

    stats_.num_alloc_retries = 0;
    stats_.num_ooms = 0;
    reset_accumulated_stat(stats_.oversize_allocations);
    reset_accumulated_stat(stats_.oversize_segments);
}

void DeviceSMACachingAllocator::resetPeakStats()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    for (size_t statType = 0; statType < static_cast<size_t>(StatType::NUM_TYPES); ++statType) {
        reset_peak_stat(stats_.allocation[statType]);
        reset_peak_stat(stats_.segment[statType]);
        reset_peak_stat(stats_.active[statType]);
        reset_peak_stat(stats_.inactive_split[statType]);
        reset_peak_stat(stats_.allocated_bytes[statType]);
        reset_peak_stat(stats_.reserved_bytes[statType]);
        reset_peak_stat(stats_.active_bytes[statType]);
        reset_peak_stat(stats_.inactive_split_bytes[statType]);
        reset_peak_stat(stats_.requested_bytes[statType]);
    }

    reset_peak_stat(stats_.oversize_allocations);
    reset_peak_stat(stats_.oversize_segments);
}

void DeviceSMACachingAllocator::attachSnapShotObserver(TraceObserver trace_ob_func, SegmentObserver segment_ob_func)
{
    trace_observer_ = trace_ob_func;
    segments_observer_ = segment_ob_func;
}

void DeviceSMACachingAllocator::snapshot(int device)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const auto all_blocks = get_all_blocks();
    device::DeviceInfoObserver::getInstance().takeSnapshot(all_blocks, device);
}

} // namespace device
} // namespace sma
} // namespace zbal
