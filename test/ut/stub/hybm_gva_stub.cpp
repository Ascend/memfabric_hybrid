/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <string>

namespace ock {
namespace mf {
namespace drv {

constexpr uint64_t baseAddr = 0x10000000000;

void HybmInitialize(int deviceId, int fd)
{
    return;
}

int HybmMapShareMemory(const char *name, void *expectAddr, uint64_t size, uint64_t flags)
{
    return 0;
}

int HybmUnmapShareMemory(void *expectAddr, uint64_t flags)
{
    return 0;
}

int HybmIoctlAllocAnddAdvice(uint64_t ptr, size_t size, uint32_t devid, uint32_t advise)
{
    return 0;
}

int32_t HalGvaReserveMemory(uint64_t *address, size_t size, int32_t deviceId, uint64_t flags)
{
    *address = baseAddr;
    return 0;
}

int32_t HalGvaUnreserveMemory(uint64_t address)
{
    return 0;
}

int32_t HalGvaAlloc(uint64_t address, size_t size, uint64_t flags)
{
    return 0;
}

int32_t HalGvaFree(uint64_t address, size_t size)
{
    return 0;
}

int32_t HalGvaOpen(uint64_t address, const char *name, size_t size, uint64_t flags)
{
    return 0;
}

int32_t HalGvaClose(uint64_t address, uint64_t flags)
{
    return 0;
}
}
}
}