/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
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

    HybmStreamPtr GetStream() const;

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

inline HybmStreamPtr HybmStreamNotify::GetStream() const
{
    return stream_;
}

using HybmStreamNotifyPtr = std::shared_ptr<HybmStreamNotify>;
}
}
#endif // MF_HYBRID_HYBM_STREAM_NOTIFY_H