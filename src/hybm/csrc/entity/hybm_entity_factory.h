/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
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
    ~MemEntityFactory() = default;

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
