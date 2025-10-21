/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#ifndef MF_HYBRID_HYBM_STREAM_MANAGER_H
#define MF_HYBRID_HYBM_STREAM_MANAGER_H

#include "hybm_stream.h"

namespace ock {
namespace mf {
class HybmStreamManager {
public:
    static HybmStreamPtr GetThreadHybmStream(uint32_t devId, uint32_t prio, uint32_t flags);
    static void *GetThreadAclStream(int32_t devId);
};
}
}

#endif // MF_HYBRID_HYBM_STREAM_MANAGER_H
