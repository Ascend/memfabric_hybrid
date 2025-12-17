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
#ifndef MEM_FABRIC_HYBRID_HYBM_ENGINE_FACTORY_H
#define MEM_FABRIC_HYBRID_HYBM_ENGINE_FACTORY_H

#include "hybm_entity.h"
#include "hybm_entity_default.h"

namespace ock {
namespace mf {
class MemEntityFactory {
public:
    static MemEntityFactory &Instance()
    {
        static MemEntityFactory INSTANCE;
        return INSTANCE;
    }

public:
    MemEntityFactory() = default;
    ~MemEntityFactory();

    EngineImplPtr GetOrCreateEngine(uint16_t id, uint32_t flags);
    EngineImplPtr FindEngineByPtr(hybm_entity_t entity);
    bool RemoveEngine(hybm_entity_t entity);

public:
    std::map<uint16_t, EngineImplPtr> engines_;
    std::map<hybm_entity_t, uint16_t> enginesFromAddress_;
    std::mutex enginesMutex_;
};
}
}

#endif // MEM_FABRIC_HYBRID_HYBM_ENGINE_FACTORY_H