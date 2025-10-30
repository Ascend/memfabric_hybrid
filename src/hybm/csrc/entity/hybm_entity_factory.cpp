/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hybm_entity_factory.h"

namespace ock {
namespace mf {
EngineImplPtr MemEntityFactory::GetOrCreateEngine(uint16_t id, uint32_t flags)
{
    std::lock_guard<std::mutex> guard(enginesMutex_);
    auto iter = engines_.find(id);
    if (iter != engines_.end()) {
        return iter->second;
    }

    /* create new engine */
    auto engine = std::make_shared<MemEntityDefault>(id);
    engines_.emplace(id, engine);
    enginesFromAddress_.emplace(engine.get(), id);
    return engine;
}

EngineImplPtr MemEntityFactory::FindEngineByPtr(hybm_entity_t entity)
{
    std::lock_guard<std::mutex> guard(enginesMutex_);
    auto pos = enginesFromAddress_.find(entity);
    if (pos == enginesFromAddress_.end()) {
        return nullptr;
    }
    auto id = pos->second;
    auto iter = engines_.find(id);
    if (iter == engines_.end()) {
        return nullptr;
    }
    return iter->second;
}

bool MemEntityFactory::RemoveEngine(hybm_entity_t entity)
{
    std::lock_guard<std::mutex> guard(enginesMutex_);
    auto pos = enginesFromAddress_.find(entity);
    if (pos == enginesFromAddress_.end()) {
        return false;
    }

    auto id = pos->second;
    enginesFromAddress_.erase(pos);
    engines_.erase(id);
    return true;
}
}
}
