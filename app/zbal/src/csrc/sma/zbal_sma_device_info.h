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
#ifndef ZBAL_SMA_DEVICE_INFO_H
#define ZBAL_SMA_DEVICE_INFO_H

#include "zbal_sma_common.h"
#include "zbal_sma_config.h"
#include "zbal_sma_device_pool.h"

// using TraceObserver = std::function<void(zbal::sma::device::TraceAction, int64_t, size_t, aclrtStream, int)>;
// using SegmentObserver = std::function<void(const std::vector<const zbal::sma::device::DeviceBlock *>&, int)>;


namespace zbal {
namespace sma {
namespace device {

enum TraceAction : uint32_t {
    ALLOC = 0,      // API made to the caching allocator for new memory
    FREE_REQUESTED, // API call made to the caching allocator to free memory
    FREE_COMPLETED, // The allocator might have to delay a free because
    // it is still in use on another stream via
    // record_stream This event is generated when a free
    // actually completes.
    SEGMENT_ALLOC, // a call to heapMalloc/aclrtMalloc to get more memory from the OS
    SEGMENT_FREE, // a call to heapMalloc/aclrtFree to return memory to the OS (e.g. to
    // defragment or empty_caches)
    SNAPSHOT, // a call to snapshot, used to correlate memory snapshots to
    // trace events
    OOM, // the allocator threw an OutOfMemoryError (addr_ is the amount of
    // free bytes reported by cuda)
    WORKSPACE_SNAPSHOT,
    EMPTY_CACHE
};

// Struct containing info on allocator action
struct TraceInfo {
    TraceInfo(TraceAction action, int device, int64_t addr, size_t size,
               aclrtStream stream)
        : action_(action), device_(device), addr_(addr), stream_(stream), size_(size)
    {
    }
    TraceAction action_;
    int device_;
    int64_t addr_; // for OOM, this is the amount of free bytes reported by cuda
    aclrtStream stream_;
    int64_t size_;
};

// Struct containing info of an allocation block (i.e. a fractional part of a cudaMalloc)..
struct BlockInfo {
    int64_t size_ = 0;
    int64_t requested_size_ = 0;
    int32_t gc_counter_ = 0;
    bool allocated_ = false;
    bool active_ = false;
};

// Struct containing info of a memory segment (i.e. one contiguous cudaMalloc).
struct SegmentInfo {
    int64_t device_ = 0;
    int64_t  address_ = 0;
    aclrtStream stream_ = nullptr;
    int64_t total_size_ = 0;
    int64_t requested_size_ = 0;
    int64_t allocated_size_ = 0;
    int64_t active_size_ = 0;
    bool is_large_ = false;
    bool is_private_ = false;
    std::vector<BlockInfo> blocks_;
};

// we will support multi shots later(each shot take list of segments)
struct SnapshotDeviceInfo {
    std::vector<SegmentInfo> seg_infos_;    // manually saved segments
    std::vector<TraceInfo> trace_infos_;    // automatic saved history traces
};

class DeviceInfoObserver {
public:
    DeviceInfoObserver() = default;
    ~DeviceInfoObserver() = default;

    static DeviceInfoObserver& getInstance()
    {
        static DeviceInfoObserver instance = DeviceInfoObserver();
        return instance;
    };

    // automatic called in each device allocator function
    void recordTrace(TraceAction action, int64_t addr, size_t size, aclrtStream stream, int device);

    // manually take in python/c++
    void takeSnapshot(const std::vector<const DeviceBlock *>& all_blocks, int device);

    // export snapshot
    const SnapshotDeviceInfo& dumpSnapshot(int device);

    // export snapshot automatically (from OOM etc.)
    void dumpSnapshotJson(int device, const std::string &output_prefix);

    // begin record trace with max size
    void recordHistory(bool record_history, int64_t max_size);

private:
    // use mutex to fix device A write trace into device B cases, which should not happen in zbal
    mutable std::recursive_mutex mutex_;

    std::vector<SnapshotDeviceInfo> snapshots_;

    int64_t max_trace_len_{kMaxTraceLen};
    size_t trace_next_ = 0;

    bool record_history_{false};
};

}  // namespace device
}  // namespace sma
}  // namespace zbal

#endif  // ZBAL_SMA_DEVICE_INFO_H
