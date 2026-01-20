/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
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
#include "dl_hal_api.h"

constexpr int32_t RETURN_OK = 0;
constexpr int32_t RETURN_ERROR = -1;
constexpr uint64_t baseAddr = 0x10000000000;
constexpr uint32_t HYBM_SQCQ_DEPTH = 2048U;

extern "C" {
int *g_devmm_mem_dev;
int32_t halGvaReserveMemory(void **address, size_t size, int32_t deviceId, uint64_t flags)
{
    *address = (void *)baseAddr;
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

void svm_module_alloced_size_inc(void *a, uint32_t b, uint32_t c, uint64_t d)
{
    return;
}

uint64_t devmm_virt_alloc_mem_from_base(void *a, size_t b, uint32_t c, uint64_t d)
{
    return RETURN_OK;
}

int32_t devmm_ioctl_enable_heap(uint32_t a, uint32_t b, uint32_t c, uint64_t d, uint32_t e)
{
    return RETURN_OK;
}

int32_t devmm_get_heap_list_by_type(void *a, void *b, void *c)
{
    return RETURN_OK;
}

int32_t devmm_virt_set_heap_idle(void *a, void *b)
{
    return RETURN_OK;
}

int32_t devmm_virt_destroy_heap(void *a, void *b, bool c)
{
    return RETURN_OK;
}

void *devmm_virt_get_heap_mgmt()
{
    return nullptr;
}

int32_t devmm_ioctl_free_pages(uint64_t a)
{
    return RETURN_OK;
}

uint32_t devmm_va_to_heap_idx(const void *a, uint64_t b)
{
    return RETURN_OK;
}

void *devmm_virt_get_heap_from_queue(void *a, uint32_t b, size_t c)
{
    return nullptr;
}

void devmm_virt_normal_heap_update_info(void *a, void *b, void *c, void *d, uint64_t e)
{
    return;
}

void *devmm_va_to_heap(uint64_t a)
{
    return nullptr;
}

thread_local uint64_t g_stubTaskNum = 0;
int halSqTaskSend(uint32_t devId, struct halTaskSendInfo *info)
{
    g_stubTaskNum++;
    return 0;
}

int halCqReportRecv(uint32_t devId, struct halReportRecvInfo *info)
{
    return 0;
}

int halSqCqAllocate(uint32_t devId, struct halSqCqInputInfo *in, struct halSqCqOutputInfo *out)
{
    return 0;
}

int halSqCqFree(uint32_t devId, struct halSqCqFreeInfo *info)
{
    return 0;
}

int halResourceIdAlloc(uint32_t devId, struct halResourceIdInputInfo *in, struct halResourceIdOutputInfo *out)
{
    return 0;
}

int halResourceIdFree(uint32_t devId, struct halResourceIdInputInfo *in)
{
    return 0;
}

int halResourceConfig(uint32_t devId, struct halResourceIdInputInfo *in, struct halResourceConfigInfo *para)
{
    return 0;
}

int halSqCqQuery(uint32_t devId, struct halSqCqQueryInfo *info)
{
    if (info->prop == DRV_SQCQ_PROP_SQ_HEAD) {
        info->value[0] = g_stubTaskNum % HYBM_SQCQ_DEPTH;
    }
    return 0;
}

int halHostRegister(void *srcPtr, uint64_t size, uint32_t flag, uint32_t devid, void **dstPtr)
{
    return 0;
}

int halHostUnregisterEx(void *srcPtr, uint32_t devid, uint32_t flag)
{
    return 0;
}

int drvNotifyIdAddrOffset(uint32_t deviceId, struct drvNotifyInfo *drvInfo)
{
    return 0;
}

int halMemMap(void *ptr, size_t size, size_t offset, drv_mem_handle_t *handle, uint64_t flag)
{
    return 0;
}

int halMemAddressReserve(void **ptr, size_t size, size_t alignment, void *addr, uint64_t flag)
{
    *ptr = addr;
    return 0;
}

int halMemAddressFree(void *ptr)
{
    return 0;
}

int halMemCreate(drv_mem_handle_t **handle, size_t size, struct drv_mem_prop *prop, uint64_t flag)
{
    return 0;
}

int halMemRelease(drv_mem_handle_t *handle)
{
    return 0;
}

int halMemUnmap(void *ptr)
{
    return 0;
}

int halMemSetAccess(void *ptr, size_t size, struct drv_mem_access_desc *desc, size_t count)
{
    return 0;
}

int halMemExportToShareableHandleV2(drv_mem_handle_t *handle, drv_mem_handle_type type, uint64_t flags,
                                    struct MemShareHandle *sHandle)
{
    return 0;
}

int halMemImportFromShareableHandleV2(drv_mem_handle_type type, struct MemShareHandle *sHandle, uint32_t devid,
                                      drv_mem_handle_t **handle)
{
    return 0;
}

int halMemShareHandleSetAttribute(uint64_t handle, enum ShareHandleAttrType type, struct ShareHandleAttr attr)
{
    return 0;
}

int halMemTransShareableHandle(drv_mem_handle_type type, struct MemShareHandle *handle, uint32_t *serverId,
                               uint64_t *shareableHandle)
{
    return 0;
}

int halMemGetAllocationGranularity(const struct drv_mem_prop *prop, drv_mem_granularity_options option,
                                   size_t *granularity)
{
    return 0;
}
}