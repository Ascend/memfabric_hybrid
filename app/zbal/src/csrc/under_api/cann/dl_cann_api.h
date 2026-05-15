/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef DL_CANN_API_H
#define DL_CANN_API_H

#include "zbal_common_includes.h"
#include "dl_cann_api_def.h"

namespace zbal {
namespace underapi {
using aclrtGetSocNameFunc = const char *(*)();
using rtGetDeviceInfoFunc = int32_t (*)(uint32_t, int32_t, int32_t, int64_t *val);
using aclrtSetDeviceFunc = int32_t (*)(int32_t);
using aclrtGetDeviceFunc = int32_t (*)(int32_t *);
using aclrtSynchronizeStreamFunc = int (*)(void *);
using aclrtMallocFunc = int32_t (*)(void **, size_t, uint32_t);
using aclrtFreeFunc = int (*)(void *);
using aclrtMallocHostFunc = int32_t (*)(void **, size_t);
using aclrtFreeHostFunc = int (*)(void *);
using aclrtMemcpyFunc = int32_t (*)(void *, size_t, const void *, size_t, uint32_t);
using aclrtMemcpyAsyncFunc = int32_t (*)(void *, size_t, const void *, size_t, uint32_t, void *);
using aclrtMemsetFunc = int32_t (*)(void *, size_t, int32_t, size_t);
using rtGetLogicDevIdByUserDevIdFunc = int32_t (*)(const int32_t, int32_t *const);
using rtGetC2cCtrlAddrFunc = int32_t (*)(uint64_t *, uint32_t *);
using aclrtGetResInCurrentThreadFunc = int32_t (*)(aclrtDevResType, uint32_t *);
using aclrtHostRegisterFunc = int32_t (*)(void *, uint64_t, aclrtHostRegisterType, void **);
using aclrtHostUnregisterFunc = int32_t (*)(void *);
using aclrtSynchronizeEventFunc = int (*)(void *);
using aclrtReserveMemAddressFunc = int (*)(void **, size_t, size_t, void *, uint64_t);
using aclrtReleaseMemAddressFunc = int (*)(void *);

class DlCannApi {
public:
    static ZResult LoadLibrary(const std::string &libDirPath);
    static void CleanupLibrary();

public:
    static const char *AclrtGetSocName();
    static ZResult RtGetDeviceInfo(uint32_t deviceId, int32_t moduleType, int32_t infoType, int64_t *val);
    static ZResult AclrtSetDevice(int32_t deviceId, bool force = false);
    static ZResult AclrtGetDevice(int32_t *deviceId);
    static ZResult AclrtSynchronizeStream(void *stream);
    static ZResult AclrtMalloc(void **ptr, size_t count, uint32_t type);
    static ZResult AclrtFree(void *ptr);
    static ZResult AclrtMallocHost(void **ptr, size_t count);
    static ZResult AclrtFreeHost(void *ptr);
    static ZResult AclrtMemcpy(void *dst, size_t destMax, const void *src, size_t count, uint32_t kind);
    static ZResult AclrtMemcpyAsync(void *dst, size_t destMax, const void *src, size_t count, uint32_t kind,
                                    void *stream);
    static ZResult AclrtMemset(void *ptr, size_t maxCount, int32_t value, size_t count);
    static ZResult RtGetLogicDevIdByUserDevId(const int32_t userDevId, int32_t *const logicDevId);
    static ZResult RtGetC2cCtrlAddr(uint64_t *address, uint32_t *len);
    static ZResult AclrtGetAIVCountInCurrentThread(uint32_t *value);
    static ZResult AclrtHostRegister(void *hostPtr, uint64_t size, void **outDevPtr);
    static ZResult AclrtHostUnRegister(void *hostPtr);
    static ZResult AclrtSynchronizeEvent(void *event);
    static ZResult AclrtReserveMemAddress(void **virPtr, size_t size, size_t alignment, void *expectPtr,
                                          uint64_t flags);
    static ZResult AclrtReleaseMemAddress(void *virPtr);

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *gAclHandle;
    static const char *gAscendAclLibName;

    static aclrtGetSocNameFunc pAclrtGetSocName;
    static rtGetDeviceInfoFunc pRtGetDeviceInfo;
    static aclrtSetDeviceFunc pAclrtSetDevice;
    static aclrtGetDeviceFunc pAclrtGetDevice;
    static aclrtSynchronizeStreamFunc pAclrtSynchronizeStream;
    static aclrtMallocFunc pAclrtMalloc;
    static aclrtFreeFunc pAclrtFree;
    static aclrtMallocHostFunc pAclrtMallocHost;
    static aclrtFreeHostFunc pAclrtFreeHost;
    static aclrtMemcpyFunc pAclrtMemcpy;
    static aclrtMemcpyAsyncFunc pAclrtMemcpyAsync;
    static aclrtMemsetFunc pAclrtMemset;
    static rtGetLogicDevIdByUserDevIdFunc pRtGetLogicDevIdByUserDevId;
    static rtGetC2cCtrlAddrFunc pRtGetC2cCtrlAddr;
    static aclrtGetResInCurrentThreadFunc pAclrtGetResInCurrentThread;
    static aclrtHostRegisterFunc pAclrtHostRegister;
    static aclrtHostUnregisterFunc pAclrtHostUnregister;
    static aclrtSynchronizeEventFunc pAclrtSynchronizeEvent;
    static aclrtReserveMemAddressFunc pAclrtReserveMemAddress;
    static aclrtReleaseMemAddressFunc pAclrtReleaseMemAddress;
};

inline const char *DlCannApi::AclrtGetSocName()
{
    return pAclrtGetSocName();
}

inline ZResult DlCannApi::AclrtSetDevice(int32_t deviceId, bool force)
{
    if (UNLIKELY(pAclrtSetDevice == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }

    if (force) {
        return pAclrtSetDevice(deviceId);
    }
    int32_t nowDeviceId = -1;
    if (AclrtGetDevice(&nowDeviceId) == 0 && nowDeviceId == deviceId) {
        return Z_OK;
    } else {
        return pAclrtSetDevice(deviceId);
    }
}

inline ZResult DlCannApi::RtGetDeviceInfo(uint32_t deviceId, int32_t moduleType, int32_t infoType, int64_t *val)
{
    if (UNLIKELY(pRtGetDeviceInfo == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }
    return pRtGetDeviceInfo(deviceId, moduleType, infoType, val);
}

inline ZResult DlCannApi::AclrtGetDevice(int32_t *deviceId)
{
    if (UNLIKELY(pAclrtGetDevice == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }
    return pAclrtGetDevice(deviceId);
}

inline ZResult DlCannApi::AclrtSynchronizeStream(void *stream)
{
    if (UNLIKELY(pAclrtSynchronizeStream == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }
    return pAclrtSynchronizeStream(stream);
}

inline ZResult DlCannApi::AclrtSynchronizeEvent(void *event)
{
    if (UNLIKELY(pAclrtSynchronizeEvent == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }
    return pAclrtSynchronizeEvent(event);
}

inline ZResult DlCannApi::AclrtReserveMemAddress(void **virPtr, size_t size, size_t alignment, void *expectPtr,
                                                 uint64_t flags)
{
    if (UNLIKELY(pAclrtReserveMemAddress == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }
    return pAclrtReserveMemAddress(virPtr, size, alignment, expectPtr, flags);
}

inline ZResult DlCannApi::AclrtReleaseMemAddress(void *virPtr)
{
    if (UNLIKELY(pAclrtReleaseMemAddress == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }
    return pAclrtReleaseMemAddress(virPtr);
}

inline ZResult DlCannApi::AclrtMalloc(void **ptr, size_t count, uint32_t type)
{
    if (UNLIKELY(pAclrtMalloc == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }
    return pAclrtMalloc(ptr, count, type);
}

inline ZResult DlCannApi::AclrtFree(void *ptr)
{
    if (UNLIKELY(pAclrtFree == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }
    return pAclrtFree(ptr);
}

inline ZResult DlCannApi::AclrtMallocHost(void **ptr, size_t count)
{
    if (UNLIKELY(pAclrtMallocHost == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }
    return pAclrtMallocHost(ptr, count);
}

inline ZResult DlCannApi::AclrtFreeHost(void *ptr)
{
    if (UNLIKELY(pAclrtFreeHost == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }
    return pAclrtFreeHost(ptr);
}

inline ZResult DlCannApi::AclrtMemcpy(void *dst, size_t destMax, const void *src, size_t count, uint32_t kind)
{
    if (UNLIKELY(pAclrtMemcpy == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }
    return pAclrtMemcpy(dst, destMax, src, count, kind);
}

inline ZResult DlCannApi::AclrtMemcpyAsync(void *dst, size_t destMax, const void *src, size_t count, uint32_t kind,
                                           void *stream)
{
    if (UNLIKELY(pAclrtMemcpyAsync == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }
    return pAclrtMemcpyAsync(dst, destMax, src, count, kind, stream);
}

inline ZResult DlCannApi::AclrtMemset(void *ptr, size_t maxCount, int32_t value, size_t count)
{
    if (UNLIKELY(pAclrtMemset == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }
    return pAclrtMemset(ptr, maxCount, value, count);
}

inline ZResult DlCannApi::RtGetLogicDevIdByUserDevId(const int32_t userDevId, int32_t *const logicDevId)
{
    if (UNLIKELY(pRtGetLogicDevIdByUserDevId == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }
    return pRtGetLogicDevIdByUserDevId(userDevId, logicDevId);
}

inline ZResult DlCannApi::RtGetC2cCtrlAddr(uint64_t *address, uint32_t *len)
{
    if (UNLIKELY(pRtGetC2cCtrlAddr == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }
    return pRtGetC2cCtrlAddr(address, len);
}

inline ZResult DlCannApi::AclrtGetAIVCountInCurrentThread(uint32_t *value)
{
    if (UNLIKELY(pAclrtGetResInCurrentThread == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }

    return pAclrtGetResInCurrentThread(ACL_RT_DEV_RES_VECTOR_CORE, value);
}

inline ZResult DlCannApi::AclrtHostRegister(void *hostPtr, uint64_t size, void **outDevPtr)
{
    if (UNLIKELY(pAclrtHostRegister == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }

    return pAclrtHostRegister(hostPtr, size, ACL_HOST_REGISTER_MAPPED, outDevPtr);
}

inline ZResult DlCannApi::AclrtHostUnRegister(void *hostPtr)
{
    if (UNLIKELY(pAclrtHostUnregister == nullptr)) {
        return Z_DL_FUNCTION_UNLOAD;
    }

    return pAclrtHostUnregister(hostPtr);
}
} // namespace underapi
} // namespace zbal

#endif // DL_CANN_API_H
