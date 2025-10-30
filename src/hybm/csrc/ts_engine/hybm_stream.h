/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef MF_HYBRID_HYBM_STREAM_H
#define MF_HYBRID_HYBM_STREAM_H

#include <vector>
#include <memory>
#include <limits>
#include <atomic>

#include "hybm_task.h"

namespace ock {
namespace mf {
class HybmStream {
public:
    HybmStream(uint32_t deviceId, uint32_t prio, uint32_t flags) noexcept;
    virtual ~HybmStream() = default; // 合理设计析构流程，避免sq和streamId资源泄漏

    int Initialize() noexcept;
    void Destroy();

    int SubmitTasks(const StreamTask &tasks) noexcept;
    int Synchronize() noexcept;

    uint32_t GetId() const;

private:
    int32_t AllocStreamId();
    int32_t AllocSqcq(uint32_t ssid);
    int32_t AllocLogicCq();
    bool GetCqeStatus();
    int32_t GetSqHead(uint32_t &head);
    int32_t ReceiveCqe(uint32_t &lastTask);

private:
    const uint32_t deviceId_;
    const uint32_t prio_;
    const uint32_t flags_;

    uint32_t tsId_{std::numeric_limits<uint32_t>::max()};
    uint32_t sqId_{0};
    uint32_t cqId_{0};
    uint32_t logicCq_{0};
    uint32_t streamId_{UINT32_MAX};
    uint32_t sqHead_{0};
    uint32_t sqTail_{0};
    std::atomic<int64_t> runningTaskCount_{0};
    std::vector<StreamTask> taskList_;
    bool inited_ = false;
};

inline uint32_t HybmStream::GetId() const
{
    return streamId_;
}

using HybmStreamPtr = std::shared_ptr<HybmStream>;
}
}

#endif  // MF_HYBRID_HYBM_STREAM_H
