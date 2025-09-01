/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBM_CORE_DL_ACL_API_H
#define MF_HYBM_CORE_DL_ACL_API_H

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
using rtIpcOpenMemoryFunc = int32_t (*)(void **, const char *);
using rtIpcCloseMemoryFunc = int32_t (*)(const void *);

class DlAclApi {
public:
    static Result LoadLibrary(const std::string &libDirPath);
    static void CleanupLibrary();

    static inline Result AclrtSetDevice(int32_t deviceId)
    {
        if (pAclrtSetDevice == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtSetDevice(deviceId);
    }

    static inline Result AclrtGetDevice(int32_t *deviceId)
    {
        if (pAclrtGetDevice == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtGetDevice(deviceId);
    }

    static inline Result AclrtDeviceEnablePeerAccess(int32_t peerDeviceId, uint32_t flags)
    {
        if (pAclrtDeviceEnablePeerAccess == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtDeviceEnablePeerAccess(peerDeviceId, flags);
    }

    static inline Result AclrtCreateStream(void **stream)
    {
        if (pAclrtCreateStream == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtCreateStream(stream);
    }

    static inline Result AclrtDestroyStream(void *stream)
    {
        if (pAclrtDestroyStream == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtDestroyStream(stream);
    }

    static inline Result AclrtSynchronizeStream(void *stream)
    {
        if (pAclrtSynchronizeStream == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtSynchronizeStream(stream);
    }

    static inline Result AclrtMalloc(void **ptr, size_t count, uint32_t type)
    {
        if (pAclrtMalloc == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtMalloc(ptr, count, type);
    }

    static inline Result AclrtFree(void *ptr)
    {
        if (pAclrtFree == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        auto ret = pAclrtFree(ptr);
        return ret;
    }

    static inline Result AclrtMemcpy(void *dst, size_t destMax, const void *src, size_t count, uint32_t kind)
    {
        if (pAclrtMemcpy == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtMemcpy(dst, destMax, src, count, kind);
    }

    static inline Result AclrtMemcpyAsync(void *dst, size_t destMax, const void *src, size_t count, uint32_t kind,
                                          void *stream)
    {
        if (pAclrtMemcpyAsync == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtMemcpyAsync(dst, destMax, src, count, kind, stream);
    }

    static inline Result AclrtMemcpy2d(void *dst, size_t dpitch, const void *src, size_t spitch,
                                       size_t width, size_t height, uint32_t kind)
    {
        if (pAclrtMemcpy2d == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtMemcpy2d(dst, dpitch, src, spitch, width, height, kind);
    }

    static inline Result AclrtMemcpy2dAsync(void *dst, size_t dpitch, const void *src, size_t spitch,
                                            size_t width, size_t height, uint32_t kind, void *stream)
    {
        if (pAclrtMemcpy2dAsync == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtMemcpy2dAsync(dst, dpitch, src, spitch, width, height, kind, stream);
    }

    static inline Result AclrtMemset(void *dst, size_t destMax, int32_t value, size_t count)
    {
        if (pAclrtMemset == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtMemset(dst, destMax, value, count);
    }

    static inline Result RtDeviceGetBareTgid(uint32_t *pid)
    {
        if (pRtDeviceGetBareTgid == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtDeviceGetBareTgid(pid);
    }

    static inline Result RtGetDeviceInfo(uint32_t deviceId, int32_t moduleType, int32_t infoType, int64_t *val)
    {
        if (pRtGetDeviceInfo == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtGetDeviceInfo(deviceId, moduleType, infoType, val);
    }

    static inline Result RtSetIpcMemorySuperPodPid(const char *name, uint32_t sdid, int32_t pid[], int32_t num)
    {
        if (pRtSetIpcMemorySuperPodPid == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtSetIpcMemorySuperPodPid(name, sdid, pid, num);
    }

    static inline Result RtIpcSetMemoryName(const void *ptr, uint64_t byteCount, char *name, uint32_t len)
    {
        if (pRtIpcSetMemoryName == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtIpcSetMemoryName(ptr, byteCount, name, len);
    }

    static inline Result RtIpcDestroyMemoryName(const char *name)
    {
        if (pRtIpcDestroyMemoryName == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtIpcDestroyMemoryName(name);
    }

    static inline Result RtIpcOpenMemory(void **ptr, const char *name)
    {
        if (pRtIpcOpenMemory == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtIpcOpenMemory(ptr, name);
    }

    static inline Result RtIpcCloseMemory(const void *ptr)
    {
        if (pRtIpcCloseMemory == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtIpcCloseMemory(ptr);
    }

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *rtHandle;
    static const char *gAscendAclLibName;

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
    static rtIpcOpenMemoryFunc pRtIpcOpenMemory;
    static rtIpcCloseMemoryFunc pRtIpcCloseMemory;
};
}
}

#endif  // MF_HYBM_CORE_DL_ACL_API_H
