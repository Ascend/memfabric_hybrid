/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef MF_HYBM_CORE_DL_ACL_API_H
#define MF_HYBM_CORE_DL_ACL_API_H

#include "hybm_common_include.h"

namespace ock {
namespace mf {

enum class aclrtMemLocationType {
    ACL_MEM_LOCATION_TYPE_HOST = 0,   // Host内存
    ACL_MEM_LOCATION_TYPE_DEVICE,     // Device内存
};

using aclrtMemLocation = struct aclrtMemLocation;
struct aclrtMemLocation {
    uint32_t id;
    aclrtMemLocationType type;    // 内存所在位置
};

using aclrtMemcpyBatchAttr = struct aclrtMemcpyBatchAttr;
struct aclrtMemcpyBatchAttr {
    aclrtMemLocation dstLoc;
    aclrtMemLocation srcLoc;
    uint8_t rsv[16];
};

using aclrtSetDeviceFunc = int32_t (*)(int32_t);
using aclrtGetDeviceFunc = int32_t (*)(int32_t *);
using aclrtDeviceEnablePeerAccessFunc = int32_t (*)(int32_t, uint32_t);
using aclrtCreateStreamFunc = int (*)(void **);
using aclrtCreateStreamWithConfigFunc = int (*)(void **, int32_t, uint32_t);
using aclrtDestroyStreamFunc = int (*)(void *);
using aclrtSynchronizeStreamFunc = int (*)(void *);
using aclrtMallocFunc = int32_t (*)(void **, size_t, uint32_t);
using aclrtFreeFunc = int (*)(void *);
using aclrtMallocHostFunc = int32_t (*)(void **, size_t);
using aclrtFreeHostFunc = int (*)(void *);
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
using rtEnableP2PFunc = int32_t (*)(uint32_t, uint32_t, uint32_t);
using rtDisableP2PFunc = int32_t (*)(uint32_t, uint32_t);
using rtGetLogicDevIdByUserDevIdFunc = int32_t (*)(const int32_t, int32_t *const);
using rtIpcOpenMemoryFunc = int32_t (*)(void **, const char *);
using rtIpcCloseMemoryFunc = int32_t (*)(const void *);
using aclrtGetSocNameFunc = const char *(*)();
using aclrtMemcpyBatchFunc = int32_t (*)(void **, size_t *, void **, size_t *, size_t,
                                         aclrtMemcpyBatchAttr *, size_t *, size_t, size_t *);

class DlAclApi {
public:
    static Result LoadLibrary(const std::string &libDirPath);
    static void CleanupLibrary();

    static inline Result AclrtSetDevice(int32_t deviceId, bool force = false)
    {
#ifndef USE_CANN
        return BM_OK;
#else
        if (pAclrtSetDevice == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        if (force) {
            return pAclrtSetDevice(deviceId);
        }
        int32_t nowDeviceId = -1;
        if (AclrtGetDevice(&nowDeviceId) == 0 && nowDeviceId == deviceId) {
            return BM_OK;
        } else {
            return pAclrtSetDevice(deviceId);
        }
#endif
    }

    static inline Result AclrtGetDevice(int32_t *deviceId)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pAclrtGetDevice == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtGetDevice(deviceId);
    }

    static inline Result AclrtDeviceEnablePeerAccess(int32_t peerDeviceId, uint32_t flags)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pAclrtDeviceEnablePeerAccess == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtDeviceEnablePeerAccess(peerDeviceId, flags);
    }

    static inline Result AclrtCreateStream(void **stream)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pAclrtCreateStream == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtCreateStream(stream);
    }

    static inline Result AclrtCreateStreamWithConfig(void **stream, uint32_t prot, uint32_t config)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pAclrtCreateStreamWithConfig == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtCreateStreamWithConfig(stream, prot, config);
    }

    static inline Result AclrtDestroyStream(void *stream)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pAclrtDestroyStream == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtDestroyStream(stream);
    }

    static inline Result AclrtSynchronizeStream(void *stream)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pAclrtSynchronizeStream == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtSynchronizeStream(stream);
    }

    static inline Result AclrtMalloc(void **ptr, size_t count, uint32_t type)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pAclrtMalloc == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtMalloc(ptr, count, type);
    }

    static inline Result AclrtFree(void *ptr)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pAclrtFree == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtFree(ptr);
    }

    static inline Result AclrtMallocHost(void **ptr, size_t count)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pAclrtMallocHost == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtMallocHost(ptr, count);
    }

    static inline Result AclrtFreeHost(void *ptr)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pAclrtFreeHost == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtFreeHost(ptr);
    }

    static inline Result AclrtMemcpy(void *dst, size_t destMax, const void *src, size_t count, uint32_t kind)
    {
#ifndef USE_CANN
        if (kind != ACL_MEMCPY_HOST_TO_HOST) {
            return BM_INVALID_PARAM;
        }
        if (count > destMax) {
            return BM_INVALID_PARAM;
        }
        // 将 void* 转为 char*
        char *dst_char = static_cast<char *>(dst);
        const char *src_char = static_cast<const char *>(src);
        std::copy_n(src_char, count, dst_char);
        return BM_OK;
#else
        if (pAclrtMemcpy == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtMemcpy(dst, destMax, src, count, kind);
#endif
    }

    static inline Result AclrtMemcpyAsync(void *dst, size_t destMax, const void *src, size_t count, uint32_t kind,
                                          void *stream)
    {
#ifndef USE_CANN
        if (kind != ACL_MEMCPY_HOST_TO_HOST) {
            return BM_INVALID_PARAM;
        }
        if (count > destMax) {
            return BM_INVALID_PARAM;
        }
        // 将 void* 转为 char*
        char *dst_char = static_cast<char *>(dst);
        const char *src_char = static_cast<const char *>(src);
        std::copy_n(src_char, count, dst_char);
        return BM_OK;
#else
        if (pAclrtMemcpyAsync == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtMemcpyAsync(dst, destMax, src, count, kind, stream);
#endif
    }

    static inline Result AclrtMemcpyBatch(void **dsts, size_t *destMax,
                                          void **srcs, size_t *sizes, size_t numBatches,
                                          aclrtMemcpyBatchAttr *attrs, size_t *attrsIndexes,
                                          size_t numAttrs, size_t *failIndex)
    {
#ifndef USE_CANN
        return BM_ERROR;
#endif
        if (pAclrtMemcpyBatch == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtMemcpyBatch(dsts, destMax, srcs, sizes, numBatches, attrs, attrsIndexes, numAttrs, failIndex);
    }

    static inline Result AclrtMemcpy2d(void *dst, size_t dpitch, const void *src, size_t spitch,
                                       size_t width, size_t height, uint32_t kind)
    {
#ifndef USE_CANN
        return BM_ERROR;
#endif
        if (pAclrtMemcpy2d == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtMemcpy2d(dst, dpitch, src, spitch, width, height, kind);
    }

    static inline Result AclrtMemcpy2dAsync(void *dst, size_t dpitch, const void *src, size_t spitch,
                                            size_t width, size_t height, uint32_t kind, void *stream)
    {
#ifndef USE_CANN
        return BM_ERROR;
#endif
        if (pAclrtMemcpy2dAsync == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtMemcpy2dAsync(dst, dpitch, src, spitch, width, height, kind, stream);
    }

    static inline Result AclrtMemset(void *dst, size_t destMax, int32_t value, size_t count)
    {
#ifndef USE_CANN
        return BM_ERROR;
#endif
        if (pAclrtMemset == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pAclrtMemset(dst, destMax, value, count);
    }

    static inline Result RtDeviceGetBareTgid(uint32_t *pid)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pRtDeviceGetBareTgid == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtDeviceGetBareTgid(pid);
    }

    static inline Result RtGetDeviceInfo(uint32_t deviceId, int32_t moduleType, int32_t infoType, int64_t *val)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pRtGetDeviceInfo == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtGetDeviceInfo(deviceId, moduleType, infoType, val);
    }

    static inline Result RtSetIpcMemorySuperPodPid(const char *name, uint32_t sdid, int32_t pid[], int32_t num)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pRtSetIpcMemorySuperPodPid == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtSetIpcMemorySuperPodPid(name, sdid, pid, num);
    }

    static inline Result RtIpcSetMemoryName(const void *ptr, uint64_t byteCount, char *name, uint32_t len)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pRtIpcSetMemoryName == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtIpcSetMemoryName(ptr, byteCount, name, len);
    }

    static inline Result RtIpcDestroyMemoryName(const char *name)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pRtIpcDestroyMemoryName == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtIpcDestroyMemoryName(name);
    }

    static inline Result RtIpcOpenMemory(void **ptr, const char *name)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pRtIpcOpenMemory == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtIpcOpenMemory(ptr, name);
    }

    static inline Result RtIpcCloseMemory(const void *ptr)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pRtIpcCloseMemory == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtIpcCloseMemory(ptr);
    }

    static inline const char *AclrtGetSocName()
    {
#ifndef USE_CANN
        return "USE_CANN IS OFF";
#endif
        return pAclrtGetSocName();
    }

    static inline Result RtEnableP2P(uint32_t devIdDes, uint32_t phyIdSrc, uint32_t flag)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pRtEnableP2P == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtEnableP2P(devIdDes, phyIdSrc, flag);
    }

    static inline Result RtDisableP2P(uint32_t devIdDes, uint32_t phyIdSrc)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pRtDisableP2P == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtDisableP2P(devIdDes, phyIdSrc);
    }

    static inline Result RtGetLogicDevIdByUserDevId(const int32_t userDevId, int32_t * const logicDevId)
    {
#ifndef USE_CANN
        return BM_OK;
#endif
        if (pRtGetLogicDevIdByUserDevId == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtGetLogicDevIdByUserDevId(userDevId, logicDevId);
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
    static aclrtCreateStreamWithConfigFunc pAclrtCreateStreamWithConfig;
    static aclrtDestroyStreamFunc pAclrtDestroyStream;
    static aclrtSynchronizeStreamFunc pAclrtSynchronizeStream;
    static aclrtMallocFunc pAclrtMalloc;
    static aclrtFreeFunc pAclrtFree;
    static aclrtMallocHostFunc pAclrtMallocHost;
    static aclrtFreeHostFunc pAclrtFreeHost;
    static aclrtMemcpyFunc pAclrtMemcpy;
    static aclrtMemcpyBatchFunc pAclrtMemcpyBatch;
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
    static aclrtGetSocNameFunc pAclrtGetSocName;
    static rtEnableP2PFunc pRtEnableP2P;
    static rtDisableP2PFunc pRtDisableP2P;
    static rtGetLogicDevIdByUserDevIdFunc pRtGetLogicDevIdByUserDevId;
};
}
}

#endif  // MF_HYBM_CORE_DL_ACL_API_H