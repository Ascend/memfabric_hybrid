/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_ENGINE_FACTORY_H
#define MEM_FABRIC_HYBRID_HYBM_ENGINE_FACTORY_H

#include "hybm_entity.h"

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

    MemEntityPtr GetOrCreateEngine(uint16_t id, uint32_t flags);
    bool RemoveEngine(hybm_entity_t entity);

public:
    std::map<uint16_t, MemEntityPtr> engines_;
    std::map<hybm_entity_t, uint16_t> enginesFromAddress_;
    std::mutex enginesMutex_;
};
}
}

#endif // MEM_FABRIC_HYBRID_HYBM_ENGINE_FACTORY_H
