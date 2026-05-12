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
#include "zbal_sma.h"
#include "zbal_sma_device_info.h"

namespace zbal {
namespace sma {

SecondaryMemoryAllocator::SecondaryMemoryAllocator() {}

void SecondaryMemoryAllocator::add_allocated_block(device::DeviceBlock *block)
{
    std::lock_guard<std::mutex> lock(mutex_);
    allocated_blocks_[block->ptr_] = block;
}

device::DeviceBlock *SecondaryMemoryAllocator::get_allocated_block(void *ptr, bool remove)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = allocated_blocks_.find(ptr);
    if (it == allocated_blocks_.end()) {
        return nullptr;
    }
    device::DeviceBlock *block = it->second;
    if (remove) {
        allocated_blocks_.erase(it);
    }
    return block;
}

bool SecondaryMemoryAllocator::initialized()
{
    return !device_allocator_.empty();
}

void SecondaryMemoryAllocator::cleanEvent()
{
    int count = static_cast<int>(device_allocator_.size());
    for (int i = 0; i < count; i++) {
        device_allocator_[i]->releaseAndFreeEvents();
    }
}

bool SecondaryMemoryAllocator::checkBlockIsSafe(const c10::DataPtr &ptr)
{
    if (!ptr.get()) {
        return true;
    }

    device::DeviceBlock *block = get_allocated_block(ptr.get());
    ZBAL_ASSERT_S(block != nullptr, "No allocated block can be found", Z_INVALID_PTR);
    return block->is_safe_;
}

void SecondaryMemoryAllocator::markAllBlockUnsafe(int device)
{
    return device_allocator_[device]->markAllBlockUnsafe();
}

void SecondaryMemoryAllocator::updateBlockToSafe(const c10::DataPtr &ptr)
{
    if (!ptr.get()) {
        return;
    }

    device::DeviceBlock *block = get_allocated_block(ptr.get());
    ZBAL_ASSERT_S(block != nullptr, "No allocated block can be found", Z_INVALID_PTR);
    if (!block->is_safe_) {
        ZBAL_LOG_INFO("Triggers to refresh the data of the unsafe memory block and remove the unsafe flag");
    }
    block->is_safe_ = true;
}

void SecondaryMemoryAllocator::assertValidDevice(int device)
{
    const auto device_num = device_allocator_.size();
    ZBAL_CHECK_S(0 <= device && device < static_cast<int64_t>(device_num), "Invalid device argument ", device,
                 ": did you call init?");
}

ZResult SecondaryMemoryAllocator::Initialize(zbal_allocator_options_t *options, int32_t device_count) noexcept
{
    (void)options;
    int size = static_cast<int>(device_allocator_.size());
    if (size < device_count) {
        device_allocator_.resize(device_count);
        for (auto i = size; i < device_count; ++i) {
            device_allocator_[i] = std::make_unique<device::DeviceSMACachingAllocator>();
            // support outside callback later
            auto &observer = device::DeviceInfoObserver::getInstance();
            auto trace_cb = [&observer](device::TraceAction action, int64_t addr, size_t size, aclrtStream stream,
                                        int device) { observer.recordTrace(action, addr, size, stream, device); };

            auto snapshot_cb = [&observer](const std::vector<const device::DeviceBlock *> &blocks, int dev) {
                observer.takeSnapshot(blocks, dev);
            };

            device_allocator_[i]->attachSnapShotObserver(trace_cb, snapshot_cb);
        }
    }
    return Z_OK;
}

ZResult SecondaryMemoryAllocator::Allocate(void **devPtr, int device, size_t size, aclrtStream stream) noexcept
{
    assertValidDevice(device);
    device::DeviceBlock *block = device_allocator_[device]->malloc(device, size, stream);

    add_allocated_block(block);
    *devPtr = static_cast<void *>(block->ptr_);

    return Z_OK;
}

ZResult SecondaryMemoryAllocator::Free(void *ptr) noexcept
{
    if (!ptr) {
        return Z_INVALID_PTR;
    }
    device::DeviceBlock *block = get_allocated_block(ptr, true);
    if (!block) {
        ZBAL_LOG_WARN("invalid device pointer: " << ptr);
    }
    device_allocator_[block->deviceId_]->free(block);

    return Z_OK;
}

ZResult SecondaryMemoryAllocator::EmptyCache(bool check_error)
{
    ZBAL_LOG_DEBUG("Begin empty cache with check_error = " << check_error);
    int32_t current_device = 0;
    if (check_error) {
        ZBAL_CHECK_S(c10_npu::GetDevice(&current_device) == ACL_SUCCESS, Z_RT_ERROR);
    } else {
        c10_npu::GetDevice(&current_device);
    }

    int device_count = static_cast<int>(device_allocator_.size());
    for (int device_idx = 0; device_idx < device_count; device_idx++) {
        // use getUsedDevice to tell which device is used, otherwise will cause one rank take all device use case?
        if (device_allocator_[device_idx]->isHeapInited()) {
            if (check_error) {
                ZBAL_CHECK_S(c10_npu::SetDevice(device_idx) == ACL_SUCCESS, Z_RT_ERROR);
            } else {
                c10_npu::SetDevice(device_idx);
            }
            device_allocator_[device_idx]->emptyCache(device_idx, check_error);
        }
    }
    if (check_error) {
        ZBAL_CHECK_S(c10_npu::MaybeSetDevice(current_device) == ACL_SUCCESS, Z_RT_ERROR);
    } else {
        c10_npu::MaybeSetDevice(current_device);
    }
    ZBAL_LOG_DEBUG("End empty cache with check_error = " << check_error);

    return Z_OK;
}

ZResult SecondaryMemoryAllocator::RecordStream(void *ptr, c10_npu::NPUStream stream)
{
    // Empty tensor's storage().data() might be a null ptr. As there is no
    // blocks associated with those tensors, it is fine to do nothing here.
    if (!ptr) {
        return Z_ERROR;
    }

    device::DeviceBlock *block = get_allocated_block(ptr);
    // block must not be null reaching here
    ZBAL_ASSERT_S(block != nullptr, "No allocated block can be found");
    device_allocator_[block->deviceId_]->recordStream(block, stream);

    return Z_OK;
}

ZResult SecondaryMemoryAllocator::EraseStream(void *ptr, c10_npu::NPUStream stream)
{
    if (!ptr) {
        return Z_ERROR;
    }

    device::DeviceBlock *block = get_allocated_block(ptr);
    if (!block) {
        ZBAL_LOG_ERROR("invalid device pointer: " << ptr);
    }

    if (block->stream_ != c10_npu::getCurrentNPUStream(block->deviceId_).stream(false)) {
        // If the Stream applying for tensor block different from
        // the stream of submiting event wait task in HCCL synchronize()
        // method, the recordSteam can not be erased.
        // New tensor creation may use the block before HCCL op is complete.
        return Z_ERROR;
    }

    device_allocator_[block->deviceId_]->eraseStream(block, stream);

    return Z_OK;
}

ZResult SecondaryMemoryAllocator::BeginAllocateToPool(int device, c10_npu::MempoolId_t mempool_id,
                                                      std::function<bool(aclrtStream)> filter)
{
    assertValidDevice(device);
    device_allocator_[device]->beginAllocateToPool(mempool_id, filter);
    return Z_OK;
}

ZResult SecondaryMemoryAllocator::EndAllocateToPool(int device, c10_npu::MempoolId_t mempool_id)
{
    assertValidDevice(device);
    device_allocator_[device]->endAllocateToPool(mempool_id);
    return Z_OK;
}

ZResult SecondaryMemoryAllocator::ReleasePool(int device, c10_npu::MempoolId_t mempool_id)
{
    assertValidDevice(device);
    device_allocator_[device]->releasePool(mempool_id);
    return Z_OK;
}

DeviceStats SecondaryMemoryAllocator::GetDeviceStats(int device)
{
    assertValidDevice(device);
    return device_allocator_[device]->getStats();
}

ZResult SecondaryMemoryAllocator::ResetAccumulatedStats(int device)
{
    assertValidDevice(device);
    device_allocator_[device]->resetAccumulatedStats();
    return Z_OK;
}

ZResult SecondaryMemoryAllocator::ResetPeakStats(int device)
{
    assertValidDevice(device);
    device_allocator_[device]->resetPeakStats();
    return Z_OK;
}

ZResult SecondaryMemoryAllocator::GetHeapState(size_t &in_used_size, size_t &total_size, int device)
{
    assertValidDevice(device);
    in_used_size = zbal::sma::SecondaryMemoryAllocator::GetInstance()->device_allocator_[device]->getHeapInUsedSize();
    total_size = zbal::sma::SecondaryMemoryAllocator::GetInstance()->device_allocator_[device]->getHeapTotalSize();
    return Z_OK;
}

ZResult SecondaryMemoryAllocator::SnapShot(zbal::sma::device::SnapshotDeviceInfo &device_info, int device)
{
    assertValidDevice(device);

    // take snapshot
    device_allocator_[device]->snapshot(device);
    // export snapshot + history
    auto record_info = zbal::sma::device::DeviceInfoObserver::getInstance().dumpSnapshot(device);

    device_info.seg_infos_.insert(device_info.seg_infos_.end(), record_info.seg_infos_.begin(),
                                  record_info.seg_infos_.end());
    device_info.trace_infos_.insert(device_info.trace_infos_.end(), record_info.trace_infos_.begin(),
                                    record_info.trace_infos_.end());

    return Z_OK;
}

} // namespace sma
} // namespace zbal

extern "C" {
ZBAL_API void *sma_malloc(size_t size, int device, aclrtStream stream)
{
    void *ptr = nullptr;
    if (size == 0) {
        return ptr;
    }
    zbal::sma::SecondaryMemoryAllocator::GetInstance()->Allocate(&ptr, device, size, stream);
    return ptr;
}

ZBAL_API void sma_free(void *ptr, size_t size, int device, aclrtStream stream)
{
    (void)size;
    (void)device;
    (void)stream;
    zbal::sma::SecondaryMemoryAllocator::GetInstance()->Free(ptr);
}

ZBAL_API void sma_init(int device_count)
{
    zbal::sma::SecondaryMemoryAllocator::GetInstance()->Initialize(nullptr, device_count);
}

ZBAL_API void sma_empty_cache(bool check_error)
{
    zbal::sma::SecondaryMemoryAllocator::GetInstance()->EmptyCache(check_error);
}

ZBAL_API void sma_record_stream(void *ptr, c10_npu::NPUStream stream)
{
    zbal::sma::SecondaryMemoryAllocator::GetInstance()->RecordStream(ptr, stream);
}

ZBAL_API void sma_erase_stream(void *ptr, c10_npu::NPUStream stream)
{
    zbal::sma::SecondaryMemoryAllocator::GetInstance()->EraseStream(ptr, stream);
}

ZBAL_API void sma_begin_allocate_to_pool(int device, c10_npu::MempoolId_t mempool_id,
                                         std::function<bool(aclrtStream)> filter)
{
    zbal::sma::SecondaryMemoryAllocator::GetInstance()->BeginAllocateToPool(device, mempool_id, filter);
}

ZBAL_API void sma_end_allocate_to_pool(int device, c10_npu::MempoolId_t mempool_id)
{
    zbal::sma::SecondaryMemoryAllocator::GetInstance()->EndAllocateToPool(device, mempool_id);
}

ZBAL_API void sma_release_pool(int device, c10_npu::MempoolId_t mempool_id)
{
    zbal::sma::SecondaryMemoryAllocator::GetInstance()->ReleasePool(device, mempool_id);
}

ZBAL_API DeviceStats sma_get_device_stats(int device)
{
    return zbal::sma::SecondaryMemoryAllocator::GetInstance()->GetDeviceStats(device);
}

ZBAL_API void *sma_get_base_addr(int device)
{
    int device_i = 0;
    if (device < 0)
        c10_npu::GetDevice(&device_i);
    else
        device_i = device;
    return zbal::sma::SecondaryMemoryAllocator::GetInstance()->device_allocator_[device_i]->getHeapBase();
}

ZBAL_API void sma_init_heap(void *base_ptr, uint64_t local_mem_size)
{
    int device = 0;
    c10_npu::GetDevice(&device);

    if (static_cast<size_t>(device) >= zbal::sma::SecondaryMemoryAllocator::GetInstance()->device_allocator_.size()) {
        ZBAL_LOG_ERROR("try to init mem heap but allocator is not inited, switch allocator first!");
        return;
    }

    if (!zbal::sma::SecondaryMemoryAllocator::GetInstance()->device_allocator_[device]->isHeapInited()) {
        void *symm_base_addr_ = base_ptr;
        zbal::sma::SecondaryMemoryAllocator::GetInstance()->device_allocator_[device]->setMemHeapPool(symm_base_addr_,
                                                                                                      local_mem_size);
    } else {
        ZBAL_LOG_WARN("re-entrance into sma init, skip this time init");
    }
}

ZBAL_API void sma_get_heap_stats(size_t &in_used_size, size_t &total_size, int device)
{
    int device_i = 0;
    if (device < 0)
        c10_npu::GetDevice(&device_i);
    else
        device_i = device;

    if (static_cast<size_t>(device_i) < zbal::sma::SecondaryMemoryAllocator::GetInstance()->device_allocator_.size() &&
        zbal::sma::SecondaryMemoryAllocator::GetInstance()->device_allocator_[device_i]->isHeapInited()) {
        zbal::sma::SecondaryMemoryAllocator::GetInstance()->GetHeapState(in_used_size, total_size, device_i);
    } else {
        ZBAL_LOG_ERROR("heap on target device is not inited, no stats now");
    }
}
} // extern C

void sma_record_memory_history(std::optional<std::string> enabled, int64_t max_entries)
{
    if (enabled) {
        if (!(enabled == "state" || enabled == "all")) {
            ZBAL_ASSERT_S(false, "sma snapshot expected enabled to be 'state' or 'all'");
        }
    }
    max_entries = (enabled && *enabled == "all") ? -1 : max_entries;
    zbal::sma::device::DeviceInfoObserver::getInstance().recordHistory(enabled.has_value(), max_entries);
}

namespace py = pybind11;
py::dict sma_dump_snapshot()
{
    using zbal::sma::device::BlockInfo;
    using zbal::sma::device::SegmentInfo;

    int device = 0;
    c10_npu::GetDevice(&device);

    // segments parser function
    py::str device_s = "device";
    py::str address_s = "address";
    py::str total_size_s = "total_size";
    py::str allocated_size_s = "allocated_size";
    py::str active_size_s = "active_size";
    py::str requested_size_s = "requested_size";
    py::str stream_s = "stream";
    py::str block_type_s = "block_type";
    py::str large_s = "large";
    py::str small_s = "small";
    py::str size_s = "size";
    py::str state_s = "state";
    py::str segment_type_s = "segment_type";
    py::str segment_pool_id = "segment_pool_id";
    py::str active_allocated_s = "active_allocated";
    py::str active_pending_free_s = "active_pending_free";
    py::str inactive_s = "inactive";
    py::str addr_s = "addr";
    py::str blocks_s = "blocks";
    py::str is_expandable_s = "is_expandable";
    py::str frames_s = "frames";
    py::list empty_frames;

    const auto segmentInfoToDict = [&](const SegmentInfo &segmentInfo) {
        py::dict segmentDict;
        segmentDict[device_s] = segmentInfo.device_;
        segmentDict[address_s] = segmentInfo.address_;
        segmentDict[total_size_s] = segmentInfo.total_size_;
        segmentDict[allocated_size_s] = segmentInfo.allocated_size_;
        segmentDict[active_size_s] = segmentInfo.active_size_;
        segmentDict[requested_size_s] = segmentInfo.requested_size_;
        // we want the python objects to pickle easily so use an int to
        // represent the stream rather than a torch.cuda.stream object
        segmentDict[stream_s] = int64_t(segmentInfo.stream_);
        segmentDict[segment_pool_id] = (segmentInfo.is_private_ ? 1 : 0);
        segmentDict[segment_type_s] = (segmentInfo.is_large_ ? large_s : small_s);
        segmentDict[is_expandable_s] = false;
        segmentDict[frames_s] = empty_frames;

        auto address = segmentInfo.address_;
        py::list blocks;
        for (const auto &blockInfo : segmentInfo.blocks_) {
            py::dict blockDict;
            blockDict[address_s] = address;
            blockDict[size_s] = blockInfo.size_;
            blockDict[requested_size_s] = blockInfo.requested_size_;
            blockDict[state_s] =
                (blockInfo.allocated_ ? active_allocated_s : (blockInfo.active_ ? active_pending_free_s : inactive_s));
            blockDict[frames_s] = empty_frames;
            blocks.append(blockDict);
            address += blockInfo.size_;
        }
        segmentDict[blocks_s] = blocks;

        return segmentDict;
    };

    zbal::sma::device::SnapshotDeviceInfo snapshot;
    zbal::sma::SecondaryMemoryAllocator::GetInstance()->SnapShot(snapshot, device);
    py::list segments;

    for (const auto &segmentInfo : snapshot.seg_infos_) {
        segments.append(segmentInfoToDict(segmentInfo));
    }

    // traces parser function
    py::list traces;
    py::str action_s = "action";
    py::str alloc_s = "alloc";
    py::str free_requested_s = "free_requested";
    py::str free_completed_s = "free_completed";
    py::str segment_alloc_s = "segment_alloc";
    py::str segment_free_s = "segment_free";
    py::str empty_cache_s = "empty_cache";

    py::str snapshot_s = "snapshot";
    py::str workspace_snapshot_s = "workspace_snapshot";
    py::str oom_s = "oom";
    py::str device_free_s = "device_free";

    using TraceEntry = zbal::sma::device::TraceAction;
    auto action_to_str = [&](TraceEntry action) {
        switch (action) {
            case TraceEntry::ALLOC:
                return alloc_s;
            case TraceEntry::FREE_REQUESTED:
                return free_requested_s;
            case TraceEntry::FREE_COMPLETED:
                return free_completed_s;
            case TraceEntry::SEGMENT_ALLOC:
                return segment_alloc_s;
            case TraceEntry::SEGMENT_FREE:
                return segment_free_s;
            case TraceEntry::OOM:
                return oom_s;
            case TraceEntry::SNAPSHOT:
                return snapshot_s;
            case TraceEntry::WORKSPACE_SNAPSHOT:
                return workspace_snapshot_s;
            case TraceEntry::EMPTY_CACHE:
                return empty_cache_s;
            default:
                ZBAL_ASSERT_S(false, "sma snapshot invalid TraceAction");
        }
    };

    py::list trace;
    for (const auto &te : snapshot.trace_infos_) {
        py::dict trace_entry;
        trace_entry[action_s] = action_to_str(te.action_);
        trace_entry[te.action_ == TraceEntry::OOM ? device_free_s : addr_s] = te.addr_;
        trace_entry[size_s] = te.size_;
        trace_entry[stream_s] = int64_t(te.stream_);
        trace.append(trace_entry);
    }
    traces.append(trace);

    py::dict result;
    result["segments"] = segments;
    result["device_traces"] = traces;

    return result;
}
