/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#include "hybm_stream_manager.h"

namespace ock {
namespace mf {

HybmStreamPtr HybmStreamManager::CreateStream(uint32_t deviceId, uint32_t prio, uint32_t flags) noexcept
{
    return std::make_shared<HybmStream>(deviceId, prio, flags);
}
}
}