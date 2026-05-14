/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <cstdint>
#include <cstddef>
#include <cstdlib>

#include "dl_cann_api_def.h"

constexpr int32_t RETURN_OK = 0;
constexpr int32_t RETURN_ERROR = -1;
constexpr uint64_t START_ADDR = 0x100000000000ULL;

typedef enum {
    ACL_DEV_ATTR_AICPU_CORE_NUM  = 1,
    ACL_DEV_ATTR_AICORE_CORE_NUM = 101,
    ACL_DEV_ATTR_CUBE_CORE_NUM = 102,
    ACL_DEV_ATTR_VECTOR_CORE_NUM = 201,
    ACL_DEV_ATTR_WARP_SIZE = 202,
    ACL_DEV_ATTR_MAX_THREAD_PER_VECTOR_CORE,
    ACL_DEV_ATTR_UBUF_PER_VECTOR_CORE,
    ACL_DEV_ATTR_TOTAL_GLOBAL_MEM_SIZE = 301,
    ACL_DEV_ATTR_L2_CACHE_SIZE,
    ACL_DEV_ATTR_SMP_ID = 401U,
    ACL_DEV_ATTR_PHY_CHIP_ID = 402U,
    ACL_DEV_ATTR_SUPER_POD_DEVICE_ID = 403U,
    ACL_DEV_ATTR_SUPER_POD_SERVER_ID = 404U,
    ACL_DEV_ATTR_SUPER_POD_ID = 405U,
    ACL_DEV_ATTR_CUST_OP_PRIVILEGE = 406U,
    ACL_DEV_ATTR_MAINBOARD_ID = 407U,
    ACL_DEV_ATTR_IS_VIRTUAL = 501U,
} aclrtDevAttr;

extern "C" {
int32_t aclrtSetDevice(int32_t deviceId)
{
    return RETURN_OK;
}

int32_t aclrtGetDevice(int32_t *deviceId)
{
    if (deviceId != nullptr) {
        *deviceId = 0;
    }
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
    if (ptr == nullptr) {
        return RETURN_ERROR;
    }
    *ptr = (void *)START_ADDR;
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

int32_t aclrtMemcpyAsync(void *dst, size_t destMax, const void *src, size_t count, uint32_t kind, void *stream)
{
    if (stream != nullptr) {
        *reinterpret_cast<uint64_t *>(stream) += 1;
    }
    return RETURN_OK;
}

int32_t rtMemcpyAsyncWithoutCheckKind(void *dst, size_t destMax, const void *src, size_t count, uint32_t kind,
                                      void *stream)
{
    if (stream != nullptr) {
        *reinterpret_cast<uint64_t *>(stream) += 1;
    }
    return RETURN_OK;
}

int32_t aclrtMemcpy2d(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height,
                      uint32_t kind)
{
    return RETURN_OK;
}

int32_t aclrtMemcpy2dAsync(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height,
                           uint32_t kind, void *stream)
{
    return RETURN_OK;
}

int32_t aclrtMemset(void *dst, size_t destMax, int32_t value, size_t count)
{
    return RETURN_OK;
}

int32_t rtDeviceGetBareTgid(uint32_t *pid)
{
    if (pid != nullptr) {
        *pid = 0;
    }
    return RETURN_OK;
}

int32_t rtGetDeviceInfo(uint32_t deviceId, int32_t moduleType, int32_t infoType, int64_t *val)
{
    if (val != nullptr) {
        *val = 0;
    }
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

int32_t rtIpcOpenMemory(void **ptr, const char *name)
{
    if (ptr == nullptr) {
        return RETURN_ERROR;
    }
    *ptr = reinterpret_cast<void *>(0x3200);
    return RETURN_OK;
}

int32_t aclrtCreateStreamWithConfig(void **stream, uint32_t prot, uint32_t config)
{
    return 0;
}

int32_t aclrtMallocHost(void **ptr, size_t count)
{
    (*ptr) = malloc(count);
    return 0;
}

int32_t aclrtFreeHost(void *ptr)
{
    free(ptr);
    return 0;
}

int32_t rtIpcCloseMemory(const void *ptr)
{
    return 0;
}

char *aclrtGetSocName()
{
    static char soc[] = "Ascend910_93";
    return soc;
}

int32_t rtEnableP2P(uint32_t devIdDes, uint32_t phyIdSrc, uint32_t flag)
{
    return 0;
}

int32_t rtDisableP2P(uint32_t devIdDes, uint32_t phyIdSrc)
{
    return 0;
}

int32_t rtGetLogicDevIdByUserDevId(const int32_t userDevId, int32_t *const logicDevId)
{
    *logicDevId = userDevId;
    return 0;
}

int32_t rtGetC2cCtrlAddr(uint64_t *ffts_address, uint32_t *ffts_len)
{
    return 0;
}

int32_t aclrtGetResInCurrentThread(zbal::aclrtDevResType type, uint32_t *blockDim)
{
    return 0;
}

int32_t aclrtDestroyEvent(void *aclrtEvent)
{
    return 0;
}

int32_t aclrtCtxGetCurrentDefaultStream(void *aclrtStream)
{
    return 0;
}

int32_t aclrtCreateEventExWithFlag(void *aclrtEvent, uint32_t flag)
{
    return 0;
}

int32_t aclrtRecordEvent(void *aclrtEvent, void *aclrtStream)
{
    return 0;
}

int32_t aclrtStreamWaitEvent(void *aclrtStream, void *aclrtEvent)
{
    return 0;
}

int32_t aclrtGetDeviceInfo(uint32_t deviceId, aclrtDevAttr attr, int64_t *value)
{
    return 0;
}

int32_t aclrtGetCurrentContext(int32_t *ctx)
{
    if (ctx != nullptr) {
        *ctx = 0;
    }
    return 0;
}

int32_t aclrtSetCurrentContext(int32_t ctx)
{
    return 0;
}
}