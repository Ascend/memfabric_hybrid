/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#include <cstdint>
#include <cstddef>

constexpr int32_t RETURN_OK = 0;
constexpr int32_t RETURN_ERROR = -1;
constexpr uint64_t START_ADDR = 0x100000000000ULL;

extern "C" {
int32_t aclrtSetDevice(int32_t deviceId)
{
    return RETURN_OK;
}

int32_t aclrtGetDevice(int32_t *deviceId)
{
    return RETURN_OK;
}

int32_t aclrtDeviceEnablePeerAccess(int32_t peerDeviceId, uint32_t flags)
{
    return RETURN_OK;
}

int32_t aclrtCreateStream(void **stream)
{
    return RETURN_OK;
}

int32_t aclrtDestroyStream(void *stream)
{
    return RETURN_OK;
}

int32_t aclrtSynchronizeStream(void *stream)
{
    return RETURN_OK;
}

int32_t aclrtMalloc(void **ptr, size_t count, uint32_t type)
{
    *ptr = (void *) START_ADDR;
    return RETURN_OK;
}

int32_t aclrtFree(void *ptr)
{
    return RETURN_OK;
}

int32_t aclrtMemcpy(void *dst, size_t destMax, const void *src, size_t count, uint32_t kind)
{
    return RETURN_OK;
}

int32_t aclrtMemcpyAsync(void *dst, size_t destMax, const void *src, size_t count, uint32_t kind,
                         void *stream)
{
    return RETURN_OK;
}

int32_t aclrtMemcpy2d(void *dst, size_t dpitch, const void *src, size_t spitch,
                      size_t width, size_t height, uint32_t kind)
{
    return RETURN_OK;
}

int32_t aclrtMemcpy2dAsync(void *dst, size_t dpitch, const void *src, size_t spitch,
                           size_t width, size_t height, uint32_t kind, void *stream)
{
    return RETURN_OK;
}

int32_t aclrtMemset(void *dst, size_t destMax, int32_t value, size_t count)
{
    return RETURN_OK;
}

int32_t rtDeviceGetBareTgid(uint32_t *pid)
{
    return RETURN_OK;
}

int32_t rtGetDeviceInfo(uint32_t deviceId, int32_t moduleType, int32_t infoType, int64_t *val)
{
    return RETURN_OK;
}

int32_t rtSetIpcMemorySuperPodPid(const char *name, uint32_t sdid, int32_t pid[], int32_t num)
{
    return RETURN_OK;
}

int32_t rtIpcSetMemoryName(const void *ptr, uint64_t byteCount, char *name, uint32_t len)
{
    return RETURN_OK;
}

int32_t rtIpcDestroyMemoryName(const char *name)
{
    return RETURN_OK;
}
}
