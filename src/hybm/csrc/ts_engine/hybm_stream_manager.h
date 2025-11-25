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
