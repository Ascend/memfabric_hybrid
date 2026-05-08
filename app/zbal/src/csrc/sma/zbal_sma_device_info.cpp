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
#include <fstream>
#include <iomanip>
#include <string>
#include <map>
#include "zbal_sma_device_info.h"

namespace zbal {
namespace sma {
namespace device {

void DeviceInfoObserver::recordTrace(TraceAction action, int64_t addr, size_t size, aclrtStream stream, int device)
{
    if (!record_history_) {
        return;
    }
    if (static_cast<size_t>(device) >= snapshots_.size()) {
        snapshots_.resize(device + 1);
    }
    int32_t curr_device = -1;
    c10_npu::GetDevice(&curr_device);
    if (curr_device != device) {
        // std::lock_guard<std::recursive_mutex> lock(mutex_);
        ZBAL_LOG_ERROR("Un-support cases on device A use device B sma pool");
    }

    auto te = TraceInfo(action, device, addr, size, stream);

    if (snapshots_[device].trace_infos_.size() < static_cast<size_t>(max_trace_len_)) {
        snapshots_[device].trace_infos_.emplace_back(te);
    } else {
        snapshots_[device].trace_infos_[trace_next_++] = te;
        if (trace_next_ == static_cast<size_t>(max_trace_len_)) {
            trace_next_ = 0;
        }
    }
}

void DeviceInfoObserver::takeSnapshot(const std::vector<const DeviceBlock *>& all_blocks, int device)
{
    if (static_cast<size_t>(device) >= snapshots_.size()) {
        snapshots_.resize(device + 1);
    }
    if (snapshots_[device].seg_infos_.size() > 0) {
        ZBAL_LOG_WARN("already have snapshot record, this action is skipped, [TODO] support multiply snapshots");
        return;
    }

    int32_t curr_device = -1;
    c10_npu::GetDevice(&curr_device);
    if (curr_device != device) {
        // std::lock_guard<std::recursive_mutex> lock(mutex_);
        ZBAL_LOG_ERROR("Un-support cases on device A use device B sma pool");
    }

    uint64_t total_active = 0;
    for (const DeviceBlock * const head_block : all_blocks) {
        // we report one segment for each continuous range of memory
        if (head_block->prev_) {
            continue;
        }
        snapshots_[device].seg_infos_.emplace_back();
        SegmentInfo &segment_info = snapshots_[device].seg_infos_.back();
        segment_info.device_ = head_block->deviceId_;
        segment_info.address_ = reinterpret_cast<int64_t>(head_block->ptr_);
        segment_info.stream_ = head_block->stream_;
        segment_info.is_large_ = (head_block->block_type_ == BT_BIG);
        segment_info.is_private_ = head_block->pool_->is_private_;
        const DeviceBlock *block = head_block;
        while (block != nullptr) {
            segment_info.blocks_.emplace_back();
            BlockInfo &block_info = segment_info.blocks_.back();

            block_info.size_ = block->size_;
            block_info.requested_size_ = block->requested_size_;
            block_info.allocated_ = block->allocated_;
            block_info.active_ = block->allocated_ || (block->event_count_ > 0);

            segment_info.total_size_ += block_info.size_;
            if (block_info.allocated_) {
                segment_info.allocated_size_ += block_info.size_;
            }
            if (block_info.active_) {
                segment_info.active_size_ += block_info.size_;
                segment_info.requested_size_ += block_info.requested_size_;
            }
            block = block->next_;
        }
        total_active += segment_info.active_size_;
    }

    std::sort(snapshots_[device].seg_infos_.begin(), snapshots_[device].seg_infos_.end(),
              [](const SegmentInfo &a, const SegmentInfo &b) { return a.address_ < b.address_; });

    recordTrace(TraceAction::SNAPSHOT, 0, total_active, nullptr, device);
}

const SnapshotDeviceInfo& DeviceInfoObserver::dumpSnapshot(int device)
{
    return snapshots_[device];
}

void DeviceInfoObserver::dumpSnapshotJson(int device, const std::string &output_prefix)
{
    auto snapshot = snapshots_[device];

    if (snapshot.seg_infos_.empty() && snapshot.trace_infos_.empty()) {
        ZBAL_LOG_ERROR("snapshot is not record yet!");
        return;
    }

    std::ofstream o(output_prefix + std::to_string(device) + ".json");
    if (!o.is_open()) {
        return;
    }

    // --- begin JSON ---
    o << "{\n";

    // 1. process segments
    o << "    \"segments\": [\n";
    for (size_t i = 0; i < snapshot.seg_infos_.size(); ++i) {
        const auto& seg = snapshot.seg_infos_[i];
        o << "        {\n";
        o << "            \"device\": " << seg.device_ << ",\n";
        o << "            \"address\": " << seg.address_ << ",\n";
        o << "            \"total_size\": " << seg.total_size_ << ",\n";
        o << "            \"allocated_size\": " << seg.allocated_size_ << ",\n";
        o << "            \"active_size\": " << seg.active_size_ << ",\n";
        o << "            \"requested_size\": " << seg.requested_size_ << ",\n";
        o << "            \"stream\": " << (int64_t)seg.stream_ << ",\n";
        o << "            \"segment_pool_id\": " << (seg.is_private_ ? 1 : 0) << ",\n";
        o << "            \"segment_type\": \"" << (seg.is_large_ ? "large" : "small") << "\",\n";

        o << "            \"blocks\": [\n";
        uint64_t curr_addr = seg.address_;
        for (size_t j = 0; j < seg.blocks_.size(); ++j) {
            const auto& b = seg.blocks_[j];
            std::string state = b.allocated_ ? "active_allocated" :
                                (b.active_ ? "active_pending_free" : "inactive");
            o << "                {\n";
            o << "                    \"address\": " << curr_addr << ",\n";
            o << "                    \"size\": " << b.size_ << ",\n";
            o << "                    \"requested_size\": " << b.requested_size_ << ",\n";
            o << "                    \"state\": \"" << state << "\"\n";
            o << "                }" << (j == seg.blocks_.size() - 1 ? "" : ",") << "\n";
            curr_addr += b.size_;
        }
        o << "            ]\n"; // blocks end
        o << "        }" << (i == snapshot.seg_infos_.size() - 1 ? "" : ",") << "\n";
    }
    o << "    ],\n"; // segments end

    // 2. process traces
    static const std::map<TraceAction, std::string> action_map = {
        {TraceAction::ALLOC, "alloc"},
        {TraceAction::FREE_REQUESTED, "free_requested"},
        {TraceAction::FREE_COMPLETED, "free_completed"},
        {TraceAction::SEGMENT_ALLOC, "segment_alloc"},
        {TraceAction::SEGMENT_FREE, "segment_free"},
        {TraceAction::OOM, "oom"}
    };

    o << "    \"device_traces\": [[\n";
    for (size_t i = 0; i < snapshot.trace_infos_.size(); ++i) {
        const auto& te = snapshot.trace_infos_[i];
        std::string act = action_map.count(te.action_) ? action_map.at(te.action_) : "unknown";

        o << "        {\n";
        o << "            \"action\": \"" << act << "\",\n";
        const char* key = (te.action_ == TraceAction::OOM ? "device_free" : "addr");
        o << "            \"" << key << "\": " << te.addr_ << ",\n";
        o << "            \"size\": " << te.size_ << ",\n";
        o << "            \"stream\": " << (int64_t)te.stream_ << "\n";
        o << "        }" << (i == snapshot.trace_infos_.size() - 1 ? "" : ",") << "\n";
    }
    o << "    ]]\n"; // device_traces end

    o << "}\n";
    o.close();
}


void DeviceInfoObserver::recordHistory(bool record_history, int64_t max_size)
{
    if (record_history_ && !record_history) {
        ZBAL_LOG_WARN("record state exchanged, clear all old history");
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        snapshots_.clear();
    }
    record_history_ = record_history;
    max_trace_len_ = max_size;
}

}  // namespace device
}  // namespace sma
}  // namespace zbal
