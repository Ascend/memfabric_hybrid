/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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
