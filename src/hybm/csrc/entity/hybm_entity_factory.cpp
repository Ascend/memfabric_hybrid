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
#include "hybm_entity_factory.h"
#include "hybm_entity_default.h"

namespace ock {
namespace mf {
MemEntityFactory::~MemEntityFactory()
{
    for (const auto &engine : engines_) {
        engine.second->UnInitialize();
        engine.second->UnReserveMemorySpace();
    }
    engines_.clear();
    enginesFromAddress_.clear();
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
} // namespace mf
} // namespace ock