/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_HYBRID_HYBM_STREAM_NOTIFY_H
#define MF_HYBRID_HYBM_STREAM_NOTIFY_H

#include "hybm_stream.h"

constexpr uint32_t TASK_WAIT_TIME_OUT = 5U;

namespace ock {
namespace mf {
class HybmStreamNotify {
public:
    explicit HybmStreamNotify(HybmStreamPtr stream) : stream_(stream) {}

    virtual ~HybmStreamNotify() = default;

    int32_t Init();

    int32_t Wait();

    uint64_t GetOffset() const;

private:
    int32_t NotifyIdAlloc(uint32_t devId, uint32_t tsId, uint32_t &notifyId);

    uint32_t notifyid_ = UINT32_MAX;
    uint64_t offset_ = 0U;

    HybmStreamPtr stream_ = nullptr;
};

inline uint64_t HybmStreamNotify::GetOffset() const
{
    return offset_;
}

using HybmStreamNotifyPtr = std::shared_ptr<HybmStreamNotify>;
}
}
#endif // MF_HYBRID_HYBM_STREAM_NOTIFY_H
