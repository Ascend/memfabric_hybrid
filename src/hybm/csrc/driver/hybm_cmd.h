/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef HYBM_HYBM_CMD_H
#define HYBM_HYBM_CMD_H

#include <string>

namespace ock {
namespace mf {
namespace drv {

void HybmInitialize(int deviceId, int fd) noexcept;

int HybmMapShareMemory(const char *name, void *expectAddr, uint64_t size, uint64_t flags) noexcept;

int HybmUnmapShareMemory(void *expectAddr, uint64_t flags) noexcept;

int HybmIoctlAllocAnddAdvice(uint64_t ptr, size_t size, uint32_t devid, uint32_t advise) noexcept;

}
}
}

#endif  // HYBM_HYBM_CMD_H
