/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#ifndef MF_HYBRID_HYBM_STREAM_H
#define MF_HYBRID_HYBM_STREAM_H

#include <atomic>
#include <limits>
#include <memory>
#include <vector>

#include "hybm_task.h"

namespace ock {
namespace mf {
class HybmStream {
public:
    HybmStream(uint32_t deviceId, uint32_t prio, uint32_t flags) noexcept;
    ~HybmStream() { Destroy(); }

    int Initialize() noexcept;
    void Destroy();

    int SubmitTasks(const StreamTask &tasks) noexcept;
    int Synchronize(uint32_t task = UINT32_MAX) noexcept;

    uint32_t GetId() const;
    uint32_t GetDevId() const;
    uint32_t GetTsId() const;
    bool GetWqeFlag() const;

private:
    int32_t AllocStreamId();
    int32_t AllocSqcq(uint32_t ssid);
    int32_t AllocLogicCq();
    bool GetCqeStatus();
    int32_t GetSqHead(uint32_t &head);
    int32_t ReceiveCqe(uint32_t &lastTask);
    bool TaskInRange(uint32_t task) const;

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
    bool wqeFlag_ = false;
    bool inited_ = false;
};

inline uint32_t HybmStream::GetId() const
{
    return streamId_;
}

inline uint32_t HybmStream::GetDevId() const
{
    return deviceId_;
}

inline uint32_t HybmStream::GetTsId() const
{
    return tsId_;
}

inline bool HybmStream::GetWqeFlag() const
{
    return wqeFlag_;
}

inline bool HybmStream::TaskInRange(uint32_t task) const
{
    return task == UINT32_MAX ||
        ((sqHead_ <= sqTail_) ? (task >= sqHead_ && task < sqTail_) : (task >= sqHead_ || task < sqTail_));
}

using HybmStreamPtr = std::shared_ptr<HybmStream>;
}
}

#endif // MF_HYBRID_HYBM_STREAM_H
