/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#ifndef MEM_FABRIC_HYBRID_DEVMM_SVM_GVA_H
#define MEM_FABRIC_HYBRID_DEVMM_SVM_GVA_H

#include <cstdint>

namespace ock {
namespace mf {
namespace drv {

int32_t HalGvaReserveMemory(uint64_t *address, size_t size, int32_t deviceId, uint64_t flags);

int32_t HalGvaUnreserveMemory(uint64_t address);

int32_t HalGvaAlloc(uint64_t address, size_t size, uint64_t flags);

int32_t HalGvaFree(uint64_t address, size_t size);

int32_t HalGvaOpen(uint64_t address, const char *name, size_t size, uint64_t flags);

int32_t HalGvaClose(uint64_t address, uint64_t flags);

}
}
}

#endif // MEM_FABRIC_HYBRID_DEVMM_SVM_GVA_H
