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
#include "dl_cann_api.h"

#include "zbal_sma_device.h"
#include "zbal_sma_device_pool.h"

namespace zbal {
namespace sma {
namespace device {

using namespace underapi;

using StreamSet = ska::flat_hash_set<c10_npu::NPUStream>;

// DeviceBlock
bool DeviceBlock::isSplit() const
{
    return (prev_ != nullptr) || (next_ != nullptr);
}

void DeviceBlock::splice(DeviceBlock *before, DeviceBlock *after)
{
    if (before) {
        ZBAL_CHECK_S(before->next_ == after, "block split check failed :", Z_INVALID_PTR);
        before->next_ = this;
    }
    prev_ = before;
    if (after) {
        ZBAL_ASSERT_S(after->prev_ == before, "block split check failed :", Z_INVALID_PTR);
        after->prev_ = this;
    }
    next_ = after;
}

// DeviceBlockPool
void DeviceBlockPool::eraseBlock(DeviceBlockType block_type, DeviceBlock *block)
{
    if (block_type == BT_SMALL)
        small_blocks_.erase(block);
    else
        large_blocks_.erase(block);
}

void DeviceBlockPool::insertBlock(DeviceBlockType block_type, DeviceBlock *block)
{
    if (block_type == BT_SMALL)
        small_blocks_.insert(block);
    else
        large_blocks_.insert(block);
}

ZEvent EventController::get(int device)
{
    ZBAL_ASSERT_S(0 <= device, "get device error:", Z_INVALID_VALUE);
    ZBAL_ASSERT_S(device < static_cast<int>(pools_.size()), "get device error:", Z_INVALID_VALUE);
    auto &pool = pools_[device];
    auto destructor = [&pool](c10_npu::NPUEvent *event) {
        std::lock_guard<std::mutex> g(pool.mutex_);
        pool.event_pool_.push_back(std::unique_ptr<c10_npu::NPUEvent>(event));
    };

    // Try to acquire an event from the per-device pool.
    {
        std::lock_guard<std::mutex> g(pool.mutex_);
        if (!pool.event_pool_.empty()) {
            auto *event = pool.event_pool_.back().release();
            pool.event_pool_.pop_back();
            return ZEvent(event, destructor);
        }
    }
    // otherwise, allocate a new event that will be returned to the pool on destruction.
    return ZEvent(std::make_unique<c10_npu::NPUEvent>(ACL_EVENT_CAPTURE_STREAM_PROGRESS).release(), destructor);
}

void EventController::emptyCache()
{
    for (auto &pool : pools_) {
        std::lock_guard<std::mutex> g(pool.mutex_);
        pool.event_pool_.clear();
    }
}

void EventController::synchronizeAndFreeEvents(DeviceSMACachingAllocator *allocator, bool check_error,
                                               const std::shared_ptr<c10::GatheredContext> &context)
{
    (void)check_error;
    // Synchronize on outstanding events and then free associated-blocks.
    for (auto &st : npu_events_) {
        for (auto &e : st.second) {
            ZEvent event = std::move(e.first);
            DeviceBlock *block = e.second;
            auto err = DlCannApi::AclrtSynchronizeEvent(*event);
            if (err != ACL_SUCCESS) {
                ZBAL_LOG_ERROR("Event: aclrtSynchronizeEvent failed, event = " << event.get());
            } else {
                ZBAL_LOG_INFO("Event: aclrtSynchronizeEvent is successfully executed, event = " << event.get());
            }

            block->event_count_--;
            if (block->event_count_ == 0) {
                allocator->free_block(block, context);
            }
        }
    }
    npu_events_.clear();
}

void EventController::insertEvents(DeviceSMACachingAllocator *allocator, DeviceBlock *block)
{
    (void)allocator;
    StreamSet streams(std::move(block->stream_uses_));
    ZBAL_ASSERT_S(block->stream_uses_.empty(), "check remain stream is empty failed:", Z_INVALID_VALUE);
    for (auto &stream : streams) {
        ZBAL_CHECK_S(c10_npu::SetDevice(stream.device_index()) == ACL_SUCCESS, "c10_npu func failed");

        ZEvent event = get(stream.device_index());
        event->record(stream);
        ZBAL_LOG_INFO("Event: record DeviceAllocator is successfully executed, event = " << event.get());

        block->event_count_++;
        npu_events_[stream].emplace_back(std::move(event), block);
    }
}

void EventController::processEvents(DeviceSMACachingAllocator *allocator,
                                    const std::shared_ptr<c10::GatheredContext> &context)
{
    // Process outstanding npuEvents. Events that are completed are removed
    // from the queue, and the 'event_count' for the corresponding allocation
    // is decremented. Stops at the first event which has not been completed.
    // Since events on different devices or streams may occur out of order,
    // the processing of some events may be delayed.
    // first: ZEvent; second: Block*
    for (auto it = npu_events_.begin(); it != npu_events_.end();) {
        while (!it->second.empty()) {
            auto &e = it->second.front();
            ZEvent event = std::move(e.first);
            DeviceBlock *block = e.second;

            if (!event->query()) {
                e.first = std::move(event);
                break;
            }

            block->event_count_--;
            if (block->event_count_ == 0) {
                allocator->free_block(block, context);
            }
            it->second.pop_front();
        }

        if (it->second.empty()) {
            it = npu_events_.erase(it);
        } else {
            it++;
        }
    }
}

void EventController::cleanEvents(DeviceSMACachingAllocator *allocator)
{
    for (auto &st : npu_events_) {
        for (auto &e : st.second) {
            ZEvent event_ = std::move(e.first);
            DeviceBlock *block = e.second;
            block->event_count_--;
            if (block->event_count_ == 0) {
                allocator->free_block(block, nullptr);
            }
        }
    }
    npu_events_.clear();
}

void EventController::cleanStream(DeviceSMACachingAllocator *allocator, DeviceBlock *block, c10_npu::NPUStream stream)
{
    // free block, lazy destroy block related events
    for (auto it = npu_events_[stream].begin(); it != npu_events_[stream].end();) {
        if (block != it->second) {
            it++;
            continue;
        }
        it = npu_events_[stream].erase(it);
        block->event_count_--;
        if (block->event_count_ == 0) {
            allocator->free_block(block, nullptr);
            break;
        }
    }
}

void GraphDeferPools::removeNpuGraphStreamUses(DeviceBlock *block)
{
    // remove stream uses added during npu-graph capture
    // (i.e., block->stream_uses - block->npu-graph_stream_uses)
    if (ZBAL_UNLIKELY(block_to_npugraph_stream_uses_.find(block) != block_to_npugraph_stream_uses_.end())) {
        StreamSet streams(std::move(block->stream_uses_));
        ZBAL_ASSERT_S(block->stream_uses_.empty(), "move stream action failed!");
        for (auto &stream : streams) {
            if (block_to_npugraph_stream_uses_[block].find(stream) == block_to_npugraph_stream_uses_[block].end()) {
                block->stream_uses_.insert(stream);
            }
        }
        block_to_npugraph_stream_uses_.erase(block);
    }
}

void GraphDeferPools::insertEventsDeferredUntilNoCapture(DeviceSMACachingAllocator *allocator,
                                                         const std::shared_ptr<c10::GatheredContext> &context)
{
    if (ZBAL_UNLIKELY(!needs_events_deferred_until_no_capture_.empty())) {
        for (auto *block : needs_events_deferred_until_no_capture_) {
            ZBAL_ASSERT(!block->stream_uses_.empty());
            // only streams recorded before npugraph will be used to insert events
            // since we know all streams recorded during npugraph must have completed
            // (refer to Section Cross-stream Dependencies and Events in CUDA Programming Guide).
            removeNpuGraphStreamUses(block);
            allocator->insert_events(block);
            if (block->event_count_ == 0) {
                allocator->free_block(block, context);
            }
        }
        needs_events_deferred_until_no_capture_.clear();
    }
}

void GraphDeferPools::appendEventsDeferredUntilNoCapture(DeviceBlock *block)
{
    needs_events_deferred_until_no_capture_.push_back(block);
}

void GraphDeferPools::insertBlockToNpuGraphStreamUses(DeviceBlock *block, c10_npu::NPUStream stream)
{
    block_to_npugraph_stream_uses_[block].insert(stream);
}

} // namespace device
} // namespace sma
} // namespace zbal
