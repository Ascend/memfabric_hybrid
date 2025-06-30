/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_RUNTIME_API_H
#define MEM_FABRIC_HYBRID_RUNTIME_API_H

#include "hybm_common_include.h"

namespace ock {
namespace mf {

using aclrtSetDeviceFunc = int32_t (*)(int32_t);
using aclrtGetDeviceFunc = int32_t (*)(int32_t *);
using aclrtDeviceEnablePeerAccessFunc = int32_t (*)(int32_t, uint32_t);
using aclrtCreateStreamFunc = int (*)(void **);
using aclrtDestroyStreamFunc = int (*)(void *);
using aclrtSynchronizeStreamFunc = int (*)(void *);
using aclrtMallocFunc = int32_t (*)(void **, size_t, uint32_t);
using aclrtFreeFunc = int (*)(void *);
using aclrtMemcpyFunc = int32_t (*)(void *, size_t, const void *, size_t, uint32_t);
using aclrtMemcpyAsyncFunc = int32_t (*)(void *, size_t, const void *, size_t, uint32_t, void *);
using aclrtMemcpy2dFunc = int32_t (*)(void *, size_t, const void *, size_t, size_t, size_t, uint32_t);
using aclrtMemcpy2dAsyncFunc = int32_t (*)(void *, size_t, const void *, size_t, size_t, size_t, uint32_t, void *);
using aclrtMemsetFunc = int32_t (*)(void *, size_t, int32_t, size_t);
using rtDeviceGetBareTgidFunc = int32_t (*)(uint32_t *);
using rtGetDeviceInfoFunc = int32_t (*)(uint32_t, int32_t, int32_t, int64_t *val);
using rtIpcSetMemoryNameFunc = int32_t (*)(const void *, uint64_t, char *, uint32_t);
using rtSetIpcMemorySuperPodPidFunc = int32_t (*)(const char *, uint32_t, int32_t *, int32_t);
using rtIpcDestroyMemoryNameFunc = int32_t (*)(const char *);

using halGvaReserveMemoryFun = int32_t (*)(void **, size_t, int32_t, uint64_t);
using halGvaUnreserveMemoryFun = int32_t (*)(void);
using halGvaAllocFun = int32_t (*)(void *, size_t, uint64_t);
using halGvaFreeFun = int32_t (*)(void *, size_t);
using halGvaOpenFun = int32_t (*)(void *, const char *, size_t, uint64_t);
using halGvaCloseFun = int32_t (*)(void *, uint64_t);

class RuntimeApi {
public:
    static Result LoadLibrary(const std::string &libDirPath);

    static inline Result AclrtSetDevice(int32_t deviceId)
    {
        return pAclrtSetDevice(deviceId);
    }

    static inline Result AclrtGetDevice(int32_t *deviceId)
    {
        return pAclrtGetDevice(deviceId);
    }

    static inline Result AclrtDeviceEnablePeerAccess(int32_t peerDeviceId, uint32_t flags)
    {
        return pAclrtDeviceEnablePeerAccess(peerDeviceId, flags);
    }

    static inline Result AclrtCreateStream(void **stream)
    {
        return pAclrtCreateStream(stream);
    }

    static inline Result AclrtDestroyStream(void *stream)
    {
        return pAclrtDestroyStream(stream);
    }

    static inline Result AclrtSynchronizeStream(void *stream)
    {
        return pAclrtSynchronizeStream(stream);
    }

    static inline Result AclrtMalloc(void **ptr, size_t count, uint32_t type)
    {
        return pAclrtMalloc(ptr, count, type);
    }

    static inline Result AclrtFree(void *ptr)
    {
        return pAclrtFree(ptr);
    }

    static inline Result AclrtMemcpy(void *dst, size_t destMax, const void *src, size_t count, uint32_t kind)
    {
        return pAclrtMemcpy(dst, destMax, src, count, kind);
    }

    static inline Result AclrtMemcpyAsync(void *dst, size_t destMax, const void *src, size_t count, uint32_t kind,
                                                void *stream)
    {
        return pAclrtMemcpyAsync(dst, destMax, src, count, kind, stream);
    }

    static inline Result AclrtMemcpy2d(void *dst, size_t dpitch, const void *src, size_t spitch,
                                       size_t width, size_t height, uint32_t kind)
    {
        return pAclrtMemcpy2d(dst, dpitch, src, spitch, width, height, kind);
    }

    static inline Result AclrtMemcpy2dAsync(void *dst, size_t dpitch, const void *src, size_t spitch,
                                            size_t width, size_t height, uint32_t kind, void *stream)
    {
        return pAclrtMemcpy2dAsync(dst, dpitch, src, spitch, width, height, kind, stream);
    }

    static inline Result AclrtMemset(void *dst, size_t destMax, int32_t value, size_t count)
    {
        return pAclrtMemset(dst, destMax, value, count);
    }

    static inline Result RtDeviceGetBareTgid(uint32_t *pid)
    {
        return pRtDeviceGetBareTgid(pid);
    }

    static inline Result RtGetDeviceInfo(uint32_t deviceId, int32_t moduleType, int32_t infoType, int64_t *val)
    {
        return pRtGetDeviceInfo(deviceId, moduleType, infoType, val);
    }

    static inline Result RtSetIpcMemorySuperPodPid(const char *name, uint32_t sdid, int32_t pid[], int32_t num)
    {
        return pRtSetIpcMemorySuperPodPid(name, sdid, pid, num);
    }

    static inline Result RtIpcSetMemoryName(const void *ptr, uint64_t byteCount, char *name, uint32_t len)
    {
        return pRtIpcSetMemoryName(ptr, byteCount, name, len);
    }

    static inline Result RtIpcDestroyMemoryName(const char *name)
    {
        return pRtIpcDestroyMemoryName(name);
    }

    static inline Result HalGvaReserveMemory(void **address, size_t size, int32_t deviceId, uint64_t flags)
    {
        return pHalGvaReserveMemory(address, size, deviceId, flags);
    }

    static inline Result HalGvaUnreserveMemory()
    {
        return pHalGvaUnreserveMemory();
    }

    static inline Result HalGvaAlloc(void *address, size_t size, uint64_t flags)
    {
        return pHalGvaAlloc(address, size, flags);
    }

    static inline Result HalGvaFree(void *address, size_t size)
    {
        return pHalGvaFree(address, size);
    }

    static inline Result HalGvaOpen(void *address, const char *name, size_t size, uint64_t flags)
    {
        return pHalGvaOpen(address, name, size, flags);
    }

    static inline Result HalGvaClose(void *address, uint64_t flags)
    {
        return pHalGvaClose(address, flags);
    }

private:
    static int32_t GetLibPath(const std::string &libDir, std::string &outputPath);

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *rtHandle;
    static void *halHandle;
    static const char *gAscendAclLibName;
    static const char *gAscendHalLibName;

    static aclrtSetDeviceFunc pAclrtSetDevice;
    static aclrtGetDeviceFunc pAclrtGetDevice;
    static aclrtDeviceEnablePeerAccessFunc pAclrtDeviceEnablePeerAccess;
    static aclrtCreateStreamFunc pAclrtCreateStream;
    static aclrtDestroyStreamFunc pAclrtDestroyStream;
    static aclrtSynchronizeStreamFunc pAclrtSynchronizeStream;
    static aclrtMallocFunc pAclrtMalloc;
    static aclrtFreeFunc pAclrtFree;
    static aclrtMemcpyFunc pAclrtMemcpy;
    static aclrtMemcpyAsyncFunc pAclrtMemcpyAsync;
    static aclrtMemcpy2dFunc pAclrtMemcpy2d;
    static aclrtMemcpy2dAsyncFunc pAclrtMemcpy2dAsync;
    static aclrtMemsetFunc pAclrtMemset;
    static rtDeviceGetBareTgidFunc pRtDeviceGetBareTgid;
    static rtGetDeviceInfoFunc pRtGetDeviceInfo;
    static rtSetIpcMemorySuperPodPidFunc pRtSetIpcMemorySuperPodPid;
    static rtIpcSetMemoryNameFunc pRtIpcSetMemoryName;
    static rtIpcDestroyMemoryNameFunc pRtIpcDestroyMemoryName;

    static halGvaReserveMemoryFun pHalGvaReserveMemory;
    static halGvaUnreserveMemoryFun pHalGvaUnreserveMemory;
    static halGvaAllocFun pHalGvaAlloc;
    static halGvaFreeFun pHalGvaFree;
    static halGvaOpenFun pHalGvaOpen;
    static halGvaCloseFun pHalGvaClose;
};
}
}

#endif  // MEM_FABRIC_HYBRID_RUNTIME_API_H
