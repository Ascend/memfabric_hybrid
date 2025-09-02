/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_HYBM_STREAM_MANAGER_H
#define MF_HYBRID_HYBM_STREAM_MANAGER_H

#include "hybm_stream.h"

namespace ock {
namespace mf {
class HybmStreamManager {
public:
    static HybmStreamPtr CreateStream(uint32_t deviceId, uint32_t prio, uint32_t flags) noexcept;
};
}
}

#endif  // MF_HYBRID_HYBM_STREAM_MANAGER_H
