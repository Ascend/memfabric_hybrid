/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#include <cstdint>
#include <cstddef>

constexpr int32_t RETURN_OK = 0;
constexpr int32_t RETURN_ERROR = -1;
constexpr uint64_t baseAddr = 0x10000000000;

extern "C" {
int32_t halGvaReserveMemory(void **address, size_t size, int32_t deviceId, uint64_t flags)
{
    *address = (void *) baseAddr;
    return RETURN_OK;
}

int32_t halGvaUnreserveMemory()
{
    return RETURN_OK;
}

int32_t halGvaAlloc(void *address, size_t size, uint64_t flags)
{
    return RETURN_OK;
}

int32_t halGvaFree(void *address, size_t size)
{
    return RETURN_OK;
}

int32_t halGvaOpen(void *address, const char *name, size_t size, uint64_t flags)
{
    return RETURN_OK;
}

int32_t halGvaClose(void *address, uint64_t flags)
{
    return RETURN_OK;
}
}